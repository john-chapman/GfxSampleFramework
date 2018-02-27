dofile "GfxSampleFramework_premake.lua"
dofile "../extern/ApplicationTools/build/ApplicationTools_premake.lua"

workspace "GfxSampleFramework"
	location(_ACTION)
	platforms { "Win64" }
	flags { "StaticRuntime" }
	filter { "platforms:Win64" }
		system "windows"
		architecture "x86_64"
	filter {}
	
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
		ApplicationTools_ProjectExternal("../extern/ApplicationTools")
	group ""
	group "libs"
		GfxSampleFramework_Project(
			"../",     -- root
			"../lib",  -- libDir
			"../bin"   -- binDir
			)
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
		
		local name = "AppSampleTest"
		filter { "action:vs*" }
			postbuildcommands({
			-- make the project data dir
				"mkdir \"$(ProjectDir)..\\..\\data\\" .. tostring(name) .. "\"",
	
			-- make link to project data dir in bin
				"rmdir \"$(ProjectDir)..\\..\\bin\\" .. tostring(name) .. "\"",
				"mklink /j \"$(ProjectDir)..\\..\\bin\\" .. tostring(name) .. "\" " .. "\"$(ProjectDir)..\\..\\data\\" .. tostring(name) .. "\"",
				})

