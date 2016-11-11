local SRC_DIR            = "../src/"
local ALL_SRC_DIR        = SRC_DIR .. "all/"
local WIN_SRC_DIR        = SRC_DIR .. "win/"
local EXTERN_DIR         = "../extern/"
local TESTS_DIR          = "../tests/"
local TESTS_EXTERN_DIR   = TESTS_DIR .. "extern/"

filter { "configurations:debug" }
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
	includedirs({ ALL_SRC_DIR, EXTERN_DIR, EXTERN_DIR .. "ApplicationTools/src/all/" })
	filter { "platforms:Win*" }
		includedirs({ WIN_SRC_DIR, EXTERN_DIR .. "ApplicationTools/src/win/" })

	
	externalproject "ApplicationTools"
		location(EXTERN_DIR .. "ApplicationTools/build/" .. _ACTION)
		uuid "6ADD11F4-56D6-3046-7F08-16CB6B601052" -- matches uuid in ApplicationTools_premake.lua
		kind "StaticLib"
		language "C++"
	
	project "framework"
		kind "StaticLib"
		language "C++"
		targetdir "../lib"
		uuid "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"
		
		files({ ALL_SRC_DIR .. "**", EXTERN_DIR .. "**" })
		removefiles({ 
			EXTERN_DIR .. "**.h", 
			EXTERN_DIR .. "ApplicationTools/**", 
			EXTERN_DIR .. "imgui/LICENSE", 
			EXTERN_DIR .. "imgui/README.md",
			EXTERN_DIR .. "lua/lua.c",
			EXTERN_DIR .. "lua/luac.c",
	ALL_SRC_DIR .. "frm/AppSampleVr.*" -- \todo move vr stuff into a separate dir
			})
		filter { "platforms:Win*" }
			files({ WIN_SRC_DIR .. "**" })
		
		
	project "framework_tests"
		kind "ConsoleApp"
		language "C++"
		targetdir "../bin"
		uuid "85A5363D-F130-A401-FA41-27F6664C0002"
		
		includedirs { TESTS_DIR, TESTS_EXTERN_DIR }
		files({ TESTS_DIR .. "**", TESTS_EXTERN_DIR .. "**" })
		removefiles({ TESTS_EXTERN_DIR .. "**.h" })
removefiles({ TESTS_DIR .. "frameworkvr_tests.cpp" })
					
		links { "ApplicationTools", "framework" }
		filter { "platforms:Win*" }
			links { "shlwapi", "hid", "opengl32" }
