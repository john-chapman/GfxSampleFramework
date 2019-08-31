dofile "GfxSampleFramework_premake.lua"
dofile "../extern/ApplicationTools/build/ApplicationTools_premake.lua"

local FRM_ROOT = "../"
local APT_ROOT = "../extern/ApplicationTools/"

workspace "GfxSampleFramework"
	location(_ACTION)
	platforms { "Win64" }
	filter { "platforms:Win64" }
	system "windows"
	architecture "x86_64"
	filter {}
	
	rtti "Off"
	exceptionhandling "Off"
	staticruntime "On"
	
	configurations { "Debug", "Release" }
	filter { "configurations:Debug" }
		targetsuffix "_debug"
		symbols "On"
		optimize "Off"
	filter {}
	filter { "configurations:Release" }
		symbols "Off"
		optimize "Full"
	filter {}

	group "libs"
		--ApplicationTools_ProjectExternal("../extern/ApplicationTools")
		ApplicationTools_Project(
			APT_ROOT,
			APT_ROOT .. "lib"
			)
	group ""
	group "libs"
		GfxSampleFramework_Project(
			FRM_ROOT .. "",     -- root
			FRM_ROOT .. "lib",  -- libDir
			FRM_ROOT .. "bin",  -- binDir
			{                   -- config
				FRM_MODULE_AUDIO = false,
				FRM_MODULE_VR    = false,
			})
		--GfxSampleFramework_ProjectExternal("../")
	group ""

	project "GfxSampleFramework_Tests"
		kind "ConsoleApp"
		language "C++"
		targetdir "../bin"

		local ALL_TESTS_DIR = "../tests/all/"

		includedirs { ALL_TESTS_DIR }
		files({
			ALL_TESTS_DIR .. "**.h",
			ALL_TESTS_DIR .. "**.hpp",
			ALL_TESTS_DIR .. "**.c",
			ALL_TESTS_DIR .. "**.cpp",
			})

		ApplicationTools_Link()
		GfxSampleFramework_Link()
		
		local name    = "AppSampleTest"
		local projDir = "$(ProjectDir)../../"
		local dataDir = projDir .. "data/" .. name
		local binDir  = projDir .. "bin/"  .. name
		filter { "action:vs*" }
			postbuildcommands({
			 -- make the project data dir
				"if not exist \"" .. dataDir .. "\" mkdir \"" .. dataDir .. "\"",

			 -- make link to project data dir in bin
				"if not exist \"" .. binDir .. "\" mklink /j \"" .. binDir .. "\" \"" .. dataDir .. "\"",
				})

