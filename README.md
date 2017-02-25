Framework for graphics samples/prototyping. OpenGL/Windows only.

**This project is very much WIP and therefore subject to breaking changes.** This will change in the future, but for now use with caution.

See the [GfxSamples](https://github.com/john-chapman/GfxSamples) repo as a reference for the project setup/usage.

Use `git clone --recursive` to init/clone all submodules, as follows:

	git clone --recursive https://github.com/john-chapman/GfxSampleFramework.git
	
Build via build/GfxSampleFramework_premake.lua as follows, requires [premake5](https://premake.github.io/):

	premake5 --file=GfxSampleFramework_premake.lua [target]

### Dependencies

Submodule dependencies:
 - [ApplicationTools](https://github.com/john-chapman/ApplicationTools)
 - [GLM](https://github.com/g-truc/glm)
	
Committed dependencies:
 - [Im3d](https://github.com/john-chapman/im3d/)
 - [ImGui](https://github.com/ocornut/imgui)
 - [RapidJSON](http://rapidjson.org/)
 - [LodePNG](http://lodev.org/lodepng/)
 - [STB](https://github.com/nothings/stb)
 - [tinyobjloader](https://github.com/syoyo/tinyobjloader)
 - [lua](https://www.lua.org)
	
