local EXTERN_DIR      = "../extern/" -- external projects
local SRC_DIR         = "../src/"
local ALL_SRC_DIR     = SRC_DIR .. "all/"
local ALL_EXTERN_DIR  = ALL_SRC_DIR .. "extern/"
local WIN_SRC_DIR     = SRC_DIR .. "win/"
local VR_ALL_SRC_DIR  = SRC_DIR .. "vr/all/"
local TESTS_DIR       = "../tests/"
local ALL_TESTS_DIR   = TESTS_DIR .. "all/"

local ENABLE_VR = true

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
	includedirs({ 
		ALL_SRC_DIR, 
		ALL_EXTERN_DIR, 
		EXTERN_DIR .. "ApplicationTools/src/all/" 
		})
	filter { "platforms:Win*" }
		includedirs({ 
			WIN_SRC_DIR, 
			EXTERN_DIR .. "ApplicationTools/src/win/" 
			})

	
	externalproject "ApplicationTools"
		location(EXTERN_DIR .. "ApplicationTools/build/" .. _ACTION)
		uuid "6ADD11F4-56D6-3046-7F08-16CB6B601052" -- uuid from ApplicationTools_premake.lua
		kind "StaticLib"
		language "C++"
	
	project "framework"
		kind "StaticLib"
		language "C++"
		targetdir "../lib"
		uuid "33827CC1-9FEC-3038-E82A-E2DD54D40E8D"
		
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
		
	project "framework_tests"
		kind "ConsoleApp"
		language "C++"
		targetdir "../bin"
		uuid "85A5363D-F130-A401-FA41-27F6664C0002"
		
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
		EXTERN_DIR .. "ApplicationTools/src/all/" 
		})
	filter { "platforms:Win*" }
		includedirs({ 
			WIN_SRC_DIR, 
			EXTERN_DIR .. "ApplicationTools/src/win/" 
			})
			
	externalproject "ApplicationTools"
		location(EXTERN_DIR .. "ApplicationTools/build/" .. _ACTION)
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