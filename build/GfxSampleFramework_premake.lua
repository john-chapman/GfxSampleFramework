local FRM_UUID        = "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"
local SRC_DIR         = "/src/"
local ALL_SRC_DIR     = SRC_DIR .. "all/"
local ALL_EXTERN_DIR  = ALL_SRC_DIR .. "extern/"
local WIN_SRC_DIR     = SRC_DIR .. "win/"
local WIN_EXTERN_DIR  = WIN_SRC_DIR .. "extern/"

local function GfxSampleFramework_Common(_root)
	SRC_DIR         = _root .. SRC_DIR
	ALL_SRC_DIR     = _root .. ALL_SRC_DIR
	ALL_EXTERN_DIR  = _root .. ALL_EXTERN_DIR
	WIN_SRC_DIR     = _root .. WIN_SRC_DIR
	WIN_EXTERN_DIR  = _root .. WIN_EXTERN_DIR

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

function GfxSampleFramework_Project(_root, _targetDir)
	_root = _root or ""
	_targetDir = _targetDir or "../lib"
	
	GfxSampleFramework_Common(_root)
	
	project "GfxSampleFramework"
		kind "StaticLib"
		language "C++"
		targetdir(_targetDir)
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
			
		filter { "action:vs*" }
			postbuildcommands({
				"rmdir \"$(ProjectDir)..\\..\\bin\\common\"",
				"mklink /j \"$(ProjectDir)..\\..\\bin\\common\" " .. "\"$(ProjectDir)..\\..\\data\\common\"",
				})
		filter {}
		
	project "*"
end

function GfxSampleFramework_ProjectExternal(_root)
	_root = _root or ""

	GfxSampleFramework_Common(_root)

	externalproject "GfxSampleFramework"
		location(_root .. "build/" .. _ACTION)
		uuid(APT_UUID)
		kind "StaticLib"
		language "C++"
		
	project "*"
end

function GfxSampleFramework_Link()
	links { "GfxSampleFramework" }

	filter { "platforms:Win*" }
		links { "hid", "opengl32" }
end
