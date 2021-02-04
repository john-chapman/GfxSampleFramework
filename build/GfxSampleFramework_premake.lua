--[[
	Usage #1: External Project File
	-------------------------------
	To use one of the prebuilt project files, call dofile() at the top of your premake script:

		dofile("extern/GfxSampleFramework/build/GfxSampleFramework_premake.lua")

	Then call GfxSampleFramework_ProjectExternal() inside your workspace declaration:

		workspace "MyWorkspace"
			GfxSampleFramework_ProjectExternal("extern/GfxSampleFramework")

	Finally, for each project which needs to link GfxSampleFramework:

		project "MyProject"
			GfxSampleFramework_Link()

	This is the least flexible of the two options but has the advantage of being able to update GfxSampleFramework without rebuilding your project files.

	Usage #2: Local Project File
	----------------------------
	To customize the project, call dofile() at the top of your premake script:

		dofile("extern/GfxSampleFramework/build/GfxSampleFramework_premake.lua")

	Then call GfxSampleFramework_Project() inside your workspace declaration:

		workspace "MyWorkspace"
			GfxSampleFramework_Project(
				"extern/GfxSampleFramework", -- lib root
				"../lib",                    -- library output path
				"../bin",                    -- binary output path
				{                            -- config map
					FRM_MODULE_AUDIO   = true,
					FRM_MODULE_PHYSICS = true,
					FRM_MODULE_VR      = false
				})

	See core/def.h for more info about configuring the library.
	
	Finally, for each project which needs to link ApplicationTools:

		project "MyProject"
			GfxSampleFramework_Link()

	This option provides the most flexibility, but don't forget to rebuild your project files after updating.

--]]

local function MakePath(_elements)
	local ret = ""
	for i, v in ipairs(_elements) do
		ret = ret .. tostring(v) .. "/"
	end
 -- \todo remove duplicate separators
	return ret
end

local function FileExists(_path)
	local f = io.open(_path, "r")
	if f ~= nil then 
		io.close(f) 
		return true 
	else
		return false
	end
end

local SRC_PATH_ROOT      = "src"
local SRC_PATH_EXTERN    = "extern"
local SRC_PATH_ALL       = "all/frm"
local SRC_PATH_PLATFORM  = ""

local MODULES = {}            -- active module names (e.g. 'core', 'audio')
local MODULE_SRC_PATHS = {}   -- all/platform + extern paths
local MODULE_DATA =           -- per-module name + custom config function
{
	["FRM_MODULE_CORE"] =
	{
		name = "core",
		config = function()
			return true
		end,
	},

	["FRM_MODULE_AUDIO"] =
	{
		name = "audio",
		config = function()
			return true
		end,
	},

	["FRM_MODULE_PHYSICS"] =
	{
		name = "physics",
		config = function()
			local physxRoot = os.getenv("PHYSX_SDK")
			if physxRoot == nil then
				print("\t\tError: 'PHYSX_SDK' not set.")
				return false
			end
			physxRoot = string.gsub(physxRoot, "\\", "/")
			print("\t\tPHYSX_SDK: '" .. physxRoot .. "'")

			physxRoot = "$(PHYSX_SDK)" -- \todo not portable but used currently to avoid absolute paths in the generate .vcxproj
			includedirs({
				physxRoot .. "/physx/include",				
				physxRoot .. "/pxshared/include", -- \todo required?
				})

		 -- \todo support different platforms/configs - need access to more info from premake to compose the path below
			local physxLib = physxRoot .. "/physx/bin/win.x86_64.vc142.mt/" .. "%{cfg.buildcfg}"
			libdirs({ physxLib })
		--[[
			local requiredFiles =
			{
				"PhysX_64"
			}
			for i,v in ipairs(requiredFiles) do
				local filePath = physxLib .. "/" .. v .. ".lib"
				if not FileExists(filePath) then
					print("\t\tError: '" .. filePath .. "' not found")
					return false
				end
			end
		--]]
	
		 -- copy DLLs to the executable directory
			postbuildcommands({
				"{COPY} \"" .. physxLib .. "/*.dll\" \"" .. "$(OutDir)/*.dll\"",
				})

			return true
		end,
	},

	["FRM_MODULE_VR"] =
	{
		name = "vr",
		config = function()
			local ovrRoot = os.getenv("OVR_SDK")
			if ovrRoot == nil then
				print("\t\tError: 'OVR_SDK' not set.")
				return false
			end
			ovrRoot = string.gsub(ovrRoot, "\\", "/")
			print("\t\tOVR_SDK: '" .. ovrRoot .. "'")

			ovrRoot = "$(OVR_SDK)" -- \todo not portable but used currently to avoid absolute paths in the generate .vcxproj
			includedirs({
				ovrRoot .. "/LibOVR/Include"
				})

		 -- \todo support different platforms/configs - need access to more info from premake to compose the path below
			local ovrLib = ovrRoot .. "/LibOVR/Lib/Windows/x64/" .. "%{cfg.buildcfg}"
			libdirs({ ovrLib })
			links({ "LibOVR" })

			return true;
		end,
	},
}

local GLOBAL_DEFINES =
{
	"GLEW_STATIC=1"
}

local function GfxSampleFramework_Config(_root, _config)
	_config = _config or {}
	_config["FRM_MODULE_CORE"] = true -- always include the core

	SRC_PATH_ROOT = _root .. "/" .. SRC_PATH_ROOT
	filter { "platforms:Win*" }
		SRC_PATH_PLATFORM = "win/frm"
	filter {}

 -- modules
	print("modules:")
	for k, module in pairs(MODULE_DATA) do	
		if _config[k] then
			print("\t" .. module.name)
			if module.config() then
			 -- set active module name
				table.insert(MODULES, module.name)

			 -- set module source paths
			 	local modulePaths = {}
				table.insert(modulePaths, MakePath { SRC_PATH_ROOT, SRC_PATH_ALL, module.name }                       )
				table.insert(modulePaths, MakePath { SRC_PATH_ROOT, SRC_PATH_ALL, module.name, SRC_PATH_EXTERN }      )
				table.insert(modulePaths, MakePath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, module.name }                  )
				table.insert(modulePaths, MakePath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, module.name, SRC_PATH_EXTERN } )
				for i, path in ipairs(modulePaths) do
					print("\t\t" .. path)
					table.insert(MODULE_SRC_PATHS, path)
				end				
			else
				print("\t\tFailed to configure " .. module.name .. ".")
				_config[k] = false -- prevent from being added to the list of defines below
			end
		end
	end

 -- defines
	print("defines:")
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
			local def = tostring(k) .. "=" .. vstr
			table.insert(GLOBAL_DEFINES, def)
			print("\t" .. def)
		end
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

		defines { "EA_COMPILER_NO_EXCEPTIONS" }
		rtti "Off"
		exceptionhandling "Off"

		filter { "action:vs*" }
			defines { "_CRT_SECURE_NO_WARNINGS", "_SCL_SECURE_NO_WARNINGS" }
			buildoptions { "/EHsc" }
			characterset "MBCS" -- force Win32 API to use *A variants (i.e. can pass char* for strings)
		filter {}

		filter { "configurations:debug" }
			defines { "FRM_DEBUG" }
		filter {}

	 -- include frm sources as e.g. <frm/core/def.h>
		includedirs(MakePath { SRC_PATH_ROOT, "all" })
		filter { "platforms:Win*" }
			includedirs(MakePath { SRC_PATH_ROOT, "win" })
		filter {}

	 -- include extern sources as e.g. <imgui/imgui.h>
		for i, moduleName in ipairs(MODULES) do
			includedirs(MakePath { SRC_PATH_ROOT, SRC_PATH_ALL, moduleName, SRC_PATH_EXTERN })
			includedirs(MakePath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, moduleName, SRC_PATH_EXTERN })
		end
end


function GfxSampleFramework_Project(_root, _libDir, _binDir, _config)
	_root   = _root or ""
	_libDir = _libDir or "../lib"
	_binDir = _binDir or "../bin"
	_config = _config or {}

	GfxSampleFramework_Config(_root, _config)

	project "GfxSampleFramework"
		targetdir(_libDir)
		GfxSampleFramework_ProjectCommon()

		if _ACTION == nil then
			return
		end

		local configFile = io.open(tostring(_ACTION) .. "/GfxSampleFramework.config.lua", "w")
			configFile:write("return {\n")
			for k, v in pairs(_config) do
				configFile:write("\t[\"" .. tostring(k) .. "\"] = " .. tostring(v) .. ",\n")
			end
			configFile:write("}")
		configFile:close()

		for k, module in pairs(MODULE_DATA) do
			local paths = 
			{
				MakePath { SRC_PATH_ROOT, SRC_PATH_ALL, module.name, "/**"        },
				MakePath { SRC_PATH_ROOT, SRC_PATH_PLATFORM, module.name,  "/**"  }
			}
			vpaths({ [module.name .. "/*"] = paths })
		end

		for i, path in ipairs(MODULE_SRC_PATHS) do
			files({ 
				path .. "/**.h",
				path .. "/**.hpp",
				path .. "/**.c",
				path .. "/**.cpp",
				path .. "/**.inl"
				})

			filter { "action:vs*" }
				files({
					path .. "/**.natvis",
				})
			filter {}
		end
		removefiles({
			MakePath { SRC_PATH_ROOT, SRC_PATH_ALL, "core/extern/lua/lua.c"  }, -- standalone lua interpreter
			MakePath { SRC_PATH_ROOT, SRC_PATH_ALL, "core/extern/lua/luac.c" }
			})

	 -- symbolic links in the bin dir for /data/common (for shaders, textures etc) and /extern (for DLLs)
		filter { "action:vs*" }
			local projDir = "$(ProjectDir)../"
			local binDir  = projDir .. _binDir
			local rootDir = projDir .. _root
			postbuildcommands({
				"if not exist \"" .. binDir .. "\" mkdir \"" .. binDir .. "\"",
			 	"if not exist \"" .. binDir .. "/common\" mklink /j \"" .. binDir .. "/common\" \"" .. rootDir .. "/data/common\"",
				"if not exist \"" .. binDir .. "/extern\" mklink /j \"" .. binDir .. "/extern\" \"" .. rootDir .. "/extern\"",
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
