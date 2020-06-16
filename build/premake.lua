dofile "GfxSampleFramework_premake.lua"

local FRM_ROOT = "../"

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
		defines "_DEBUG"
	filter {}
	filter { "configurations:Release" }
		symbols "On"
		optimize "Full"
		defines "NDEBUG"
	filter {}

	group "libs"
		GfxSampleFramework_Project(
			FRM_ROOT .. "",     -- root
			FRM_ROOT .. "lib",  -- libDir
			FRM_ROOT .. "bin",  -- binDir
			{                   -- config
				FRM_MODULE_AUDIO   = false,
				FRM_MODULE_VR      = false,
				FRM_MODULE_PHYSICS = false,
			})
	group ""

	local projDir = "$(ProjectDir)..\\..\\"
	local dataDir = projDir .. "data\\"
	local binDir  = projDir .. "bin\\"
	local projList = dofile("projects.lua")
	for name,fileList in pairs(projList) do
		project(tostring(name))
			kind "ConsoleApp"
			targetdir "../bin"
			GfxSampleFramework_Link()

			vpaths({ ["*"] = { "../tests/" .. tostring(name) .. "/**" } })
			files(fileList)
			--files("../src/_sample.cpp")

			local projDataDir = dataDir .. tostring(name)
			local projBinDir  = binDir  .. tostring(name)
			filter { "action:vs*" }
				postbuildcommands({
				  -- make the project data dir
					"if not exist " .. projDataDir .. " mkdir \"" .. projDataDir .. "\"",
				  -- make link to project data dir in bin
					"if not exist " .. projBinDir .. " mklink /j \"" .. projBinDir .. "\" \"" .. projDataDir .. "\"",
					})
			filter {}
	end
