local SRC_PATH_ROOT      = "src"
local SRC_PATH_EXTERN    = "extern"
local SRC_PATH_ALL       = "all/frm"
local SRC_PATH_PLATFORM  = ""

local MODULES            = {}
local MODULE_PATHS       = {}
local MODULE_NAMES       =
{
	["FRM_MODULE_CORE"]  = "core",
	["FRM_MODULE_AUDIO"] = "audio",
	["FRM_MODULE_VR"]    = "vr",
}

local GLOBAL_DEFINES     =
{
	"GLEW_STATIC=1"
}

local function makepath(_elements)
	local ret = ""
	for i, v in ipairs(_elements) do
		ret = ret .. tostring(v) .. "/"
	end
 -- \todo remove duplicate delimiters
	return ret
end

local function GfxSampleFramework_Config(_root, _config)
	_config = _config or {}
	_config["FRM_MODULE_CORE"] = true -- always include the core

	SRC_PATH_ROOT = _root .. "/" .. SRC_PATH_ROOT
	filter { "platforms:Win*" }
		SRC_PATH_PLATFORM = "win/frm"
	filter {}

 -- defines
	for k, v in pairs(_config) do
		local vstr  = tostring(v)
		local vtype = type(v)
		if vtype == "function" then
			vstr = nil
		elseif vtype == "nil" then
			vstr = nil
		elseif vtype == "boolean" then
			if v then
				vstr = "1"
			else
				vstr = "0"
			end
		end
		if vstr ~= nil then
			table.insert(GLOBAL_DEFINES, tostring(k) .. "=" .. vstr)
		end
	end

 -- modules
	for k, moduleName in pairs(MODULE_NAMES) do
		if _config[k] then
			table.insert(MODULES, moduleName)
		end
	end

 -- module paths
	for i, moduleName in ipairs(MODULES) do
		table.insert(MODULE_PATHS, makepath { SRC_PATH_ROOT, SRC_PATH_ALL, moduleName }                       )
		table.insert(MODULE_PATHS, makepath { SRC_PATH_ROOT, SRC_PATH_ALL, moduleName, SRC_PATH_EXTERN }      )
		table.insert(MODULE_PATHS, makepath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, moduleName }                  )
		table.insert(MODULE_PATHS, makepath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, moduleName, SRC_PATH_EXTERN } )
	end

	print("defines:")
	for i, def in ipairs(GLOBAL_DEFINES) do
		print("\t" .. def)
	end
	print("modules:")
	for i, moduleName in ipairs(MODULES) do
		print("\t" .. moduleName)
	end
	print("src paths:")
	for i, path in ipairs(MODULE_PATHS) do
		print("\t" .. path)
	end
end

local function GfxSampleFramework_ProjectCommon()
	kind       "StaticLib"
	language   "C++"
	cppdialect "C++11"
	uuid       "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"
end

local function GfxSampleFramework_Globals()
	project "*"
		for i, def in ipairs(GLOBAL_DEFINES) do
			defines(def)
		end

	 -- include frm sources as e.g. <frm/core/def.h>
		includedirs(makepath { SRC_PATH_ROOT, "all" })
		filter { "platforms:Win*" }
			includedirs(makepath { SRC_PATH_ROOT, "win" })
		filter {}

	 -- include extern sources as e.g. <imgui/imgui.h>
		for i, moduleName in ipairs(MODULES) do
			includedirs(makepath { SRC_PATH_ROOT, SRC_PATH_ALL, moduleName, SRC_PATH_EXTERN })
			includedirs(makepath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, moduleName, SRC_PATH_EXTERN })
		end
end


function GfxSampleFramework_Project(_root, _libDir, _binDir, _config)
	_root   = _root or ""
	_libDir = _libDir or "../lib"
	_binDir = _binDir or "../bin"

	GfxSampleFramework_Config(_root, _config)

	project "GfxSampleFramework"
		targetdir(_libDir)
		GfxSampleFramework_ProjectCommon()

		local configFile = io.open(tostring(_ACTION) .. "/GfxSampleFramework.config.lua", "w")
			configFile:write("return {\n")
			for k, v in pairs(_config) do
				configFile:write("\t[\"" .. tostring(k) .. "\"] = " .. tostring(v) .. ",\n")
			end
			configFile:write("}")
		configFile:close()

		for k, moduleName in pairs(MODULE_NAMES) do
			local paths = 
			{
				makepath { SRC_PATH_ROOT, SRC_PATH_ALL, moduleName, "/**"        },
				makepath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, moduleName,  "/**"  }
			}
			vpaths({ [moduleName .. "/*"] = paths })
		end

		for i, path in ipairs(MODULE_PATHS) do
			files({ 
				path .. "/**.h",
				path .. "/**.hpp",
				path .. "/**.c",
				path .. "/**.cpp"
				})
		end
		removefiles({
			_root .. SRC_PATH_ALL .. "frm/core/extern/lua/lua.c", -- standalone lua interpreter
			_root .. SRC_PATH_ALL .. "frm/core/extern/lua/luac.c"
			})

		filter { "action:vs*" }
			postbuildcommands({
				"rmdir \"$(ProjectDir)../" .. _binDir .. "/common\"",
				"mklink /j \"$(ProjectDir)../" .. _binDir .. "/common\" " .. "\"$(ProjectDir)../" .. _root .. "/data/common\"",
				})
		filter {}

	GfxSampleFramework_Globals()
end

function GfxSampleFramework_ProjectExternal(_root)
	_root   = _root or ""

	externalproject "GfxSampleFramework"
		location(_root .. "build/" .. _ACTION)
		GfxSampleFramework_ProjectCommon()

	local config = dofile(tostring(_ACTION) .. "/GfxSampleFramework.config.lua")
	GfxSampleFramework_Config(_root, config)

	GfxSampleFramework_Globals()
end

function GfxSampleFramework_Link()
	links { "GfxSampleFramework" }

	filter { "platforms:Win*" }
		links { "hid", "opengl32" }
	filter {}
end
