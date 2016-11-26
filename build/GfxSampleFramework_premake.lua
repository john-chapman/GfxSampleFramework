local EXTERN_DIR      = "../extern/" -- external projects
local APT_DIR         = EXTERN_DIR .. "ApplicationTools/"
local SRC_DIR         = "../src/"
local ALL_SRC_DIR     = SRC_DIR .. "all/"
local ALL_EXTERN_DIR  = ALL_SRC_DIR .. "extern/"
local WIN_SRC_DIR     = SRC_DIR .. "win/"
local VR_ALL_SRC_DIR  = SRC_DIR .. "vr/all/"
local TESTS_DIR       = "../tests/"
local ALL_TESTS_DIR   = TESTS_DIR .. "all/"

filter { "configurations:debug" }
	defines { "APT_DEBUG", "FRM_DEBUG" }
	targetsuffix "_debug"
	symbols "On"
	optimize "Off"
	
filter { "configurations:release" }
	symbols "Off"
	optimize "Full"

filter { "action:vs*" }
	defines { "_CRT_SECURE_NO_WARNINGS", "_SCL_SECURE_NO_WARNINGS" }
	characterset "MBCS" -- force Win32 API to use *A variants (i.e. can pass char* for strings)

workspace "GfxSampleFramework"
	location(_ACTION)
	configurations { "Debug", "Release" }
	platforms { "Win64" }
	flags { "C++11", "StaticRuntime" }
	filter { "platforms:Win64" }
		system "windows"
		architecture "x86_64"
	
	defines { "GLEW_STATIC" }
	includedirs({ 
		ALL_SRC_DIR, 
		ALL_EXTERN_DIR, 
		APT_DIR .. "src/all/",
		APT_DIR .. "src/all/extern/",
		})
	filter { "platforms:Win*" }
		includedirs({ 
			WIN_SRC_DIR, 
			APT_DIR .. "src/win/",
			APT_DIR .. "src/win/extern/",
			})

	group "libs"
		externalproject "ApplicationTools"
			location(APT_DIR .. "build/" .. _ACTION)
			uuid "6ADD11F4-56D6-3046-7F08-16CB6B601052" -- ApplicationTools_premake.lua
			kind "StaticLib"
			language "C++"
		
		project "framework"
			kind "StaticLib"
			language "C++"
			targetdir "../lib"
			uuid "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"
			
			vpaths({
				["*"]      = ALL_SRC_DIR .. "frm/**",
				["extern"] = ALL_EXTERN_DIR .. "**", 
				["win"]    = WIN_SRC_DIR .. "frm/**",
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
	group ""
		
	project "framework_tests"
		kind "ConsoleApp"
		language "C++"
		targetdir "../bin"
		uuid "85A5363D-F130-A401-FA41-27F6664C0002"
		
		vpaths({
			["*"] = ALL_TESTS_DIR .. "**",
			})
		
		includedirs({ 
			TESTS_DIR, 
			TESTS_EXTERN_DIR,
			})
		files({ 
			ALL_TESTS_DIR .. "**.h", 
			ALL_TESTS_DIR .. "**.hpp", 
			ALL_TESTS_DIR .. "**.c", 
			ALL_TESTS_DIR .. "**.cpp",
			})
					
		links { "ApplicationTools", "framework" }
		filter { "platforms:Win*" }
			links { "shlwapi", "hid", "opengl32" }

		filter { "action:vs*" }
			postbuildcommands({
				"rmdir \"$(ProjectDir)..\\..\\bin\\common\"",
				"mklink /j \"$(ProjectDir)..\\..\\bin\\common\" " .. "\"$(ProjectDir)..\\..\\data\\common\"",
				})
		

workspace "GfxSampleFrameworkVr"
	location(_ACTION)
	configurations { "Debug", "Release" }
	platforms { "Win64" }
	flags { "C++11", "StaticRuntime" }
	filter { "platforms:Win64" }
		system "windows"
		architecture "x86_64"
		
	defines { "GLEW_STATIC" }
	includedirs({ 
		ALL_SRC_DIR, 
		ALL_EXTERN_DIR, 
		APT_DIR .. "src/all/",
		APT_DIR .. "src/all/extern",
		})
	filter { "platforms:Win*" }
		includedirs({ 
			WIN_SRC_DIR, 
			APT_DIR .. "src/win/",
			APT_DIR .. "src/win/extern/",
			})
			
	group "libs"
		externalproject "ApplicationTools"
			location(APT_DIR .. "build/" .. _ACTION)
			uuid "6ADD11F4-56D6-3046-7F08-16CB6B601052" -- uuid from ApplicationTools_premake.lua
			kind "StaticLib"
			language "C++"
			
		externalproject "framework"
			uuid "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"
			kind "StaticLib"
			language "C++"
			
		project "frameworkvr"
			kind "StaticLib"
			language "C++"
			targetdir "../lib"
			uuid "9BEAA512-07A0-1E08-9094-18DFFC48150C"
			
			vpaths({
				["*"] = VR_ALL_SRC_DIR .. "frm/**",
				})
			
			includedirs({
				VR_ALL_SRC_DIR,
				"$(OVRSDKROOT)/LibOVR/Include/", -- \todo environment vars?
				})
			files({ 
				VR_ALL_SRC_DIR    .. "**.h", 
				VR_ALL_SRC_DIR    .. "**.hpp", 
				VR_ALL_SRC_DIR    .. "**.c", 
				VR_ALL_SRC_DIR    .. "**.cpp",
				})
	group ""