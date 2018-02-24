local FRM_UUID        = "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"

local SRC_DIR         = "/src/"
local ALL_SRC_DIR     = SRC_DIR .. "all/"
local ALL_EXTERN_DIR  = ALL_SRC_DIR .. "extern/"
local WIN_SRC_DIR     = SRC_DIR .. "win/"
local WIN_EXTERN_DIR  = WIN_SRC_DIR .. "extern/"

local function GfxSampleFramework_SetPaths(_root)
	SRC_DIR         = _root .. SRC_DIR
	ALL_SRC_DIR     = _root .. ALL_SRC_DIR
	ALL_EXTERN_DIR  = _root .. ALL_EXTERN_DIR
	WIN_SRC_DIR     = _root .. WIN_SRC_DIR
	WIN_EXTERN_DIR  = _root .. WIN_EXTERN_DIR
end

local function GfxSampleFramework_Globals()
	project "*"

	defines { "GLEW_STATIC" }
	rtti "Off"
	exceptionhandling "Off"

	includedirs({
		ALL_SRC_DIR,
		ALL_EXTERN_DIR,
		})
	filter { "platforms:Win*" }
		includedirs({
			WIN_SRC_DIR,
			WIN_EXTERN_DIR,
			})
	filter {}
end

function GfxSampleFramework_Project(_root, _libDir, _binDir, _config)
	_root   = _root or ""
	_libDir = _libDir or "../lib"
	_binDir = _binDir or "../bin"
	_config = _config or {}

	GfxSampleFramework_SetPaths(_root)

	project "GfxSampleFramework"
		kind "StaticLib"
		language "C++"
		cppdialect "C++11"
		targetdir(_libDir)
		uuid(FRM_UUID)

		filter { "configurations:debug" }
			targetsuffix "_debug"
			symbols "On"
			optimize "Off"
		filter {}

		filter { "configurations:release" }
			symbols "Off"
			optimize "Full"
		filter {}

		vpaths({
			["*"]        = ALL_SRC_DIR .. "frm/**",
			["extern/*"] = ALL_EXTERN_DIR .. "**",
			["win"]      = WIN_SRC_DIR .. "frm/**",
			})

		files({
			ALL_SRC_DIR    .. "**.h",
			ALL_SRC_DIR    .. "**.hpp",
			ALL_SRC_DIR    .. "**.c",
			ALL_SRC_DIR    .. "**.cpp",
			ALL_EXTERN_DIR .. "**.c",
			ALL_EXTERN_DIR .. "**.cpp",
			})
		removefiles({
			ALL_EXTERN_DIR .. "lua/lua.c", -- standalone lua interpreter
			ALL_EXTERN_DIR .. "lua/luac.c",
			})
		filter { "platforms:Win*" }
			files({
				WIN_SRC_DIR .. "**.h",
				WIN_SRC_DIR .. "**.hpp",
				WIN_SRC_DIR .. "**.c",
				WIN_SRC_DIR .. "**.cpp",
			})
		filter {}
		
		for k,v in pairs(_config) do
			if v then
				defines { tostring(k) .. "=" .. tostring(v) }
			end
		end

		filter { "action:vs*" }
			postbuildcommands({
				"rmdir \"$(ProjectDir)../" .. _binDir .. "/common\"",
				"mklink /j \"$(ProjectDir)../" .. _binDir .. "/common\" " .. "\"$(ProjectDir)../" .. _root .. "/data/common\"",
				})
		filter {}

	GfxSampleFramework_Globals()
end

function GfxSampleFramework_ProjectExternal(_root)
	_root = _root or ""

	GfxSampleFramework_SetPaths(_root)

	externalproject "GfxSampleFramework"
		location(_root .. "build/" .. _ACTION)
		uuid(APT_UUID)
		kind "StaticLib"
		language "C++"

	GfxSampleFramework_Globals()
end

function GfxSampleFramework_Link()
	links { "GfxSampleFramework" }

	filter { "platforms:Win*" }
		links { "hid", "opengl32" }
end
