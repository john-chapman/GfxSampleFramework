Framework for graphics samples/prototyping. Currently OpenGL/Windows only. 

See [GfxSamples](https://github.com/john-chapman/GfxSamples) as a reference for the project setup/usage.

Use `git clone --recursive` to init/clone all submodules, as follows:

	git clone --recursive https://github.com/john-chapman/GfxSampleFramework.git
	
Build via build/GfxSampleFramework_premake.lua as follows, requires [premake5](https://premake.github.io/):

	premake5 --file=GfxSampleFramework_premake.lua [target]

### Dependencies

Submodule dependencies:
 - [ApplicationTools](https://github.com/john-chapman/ApplicationTools)
 
Embedded dependencies:
 - [EASTL](https://github.com/electronicarts/EASTL)
 - [GLM](https://github.com/g-truc/glm)
 - [GLEW](http://glew.sourceforge.net/)
 - [Im3d](https://github.com/john-chapman/im3d/)
 - [ImGui](https://github.com/ocornut/imgui)
 - [RapidJSON](http://rapidjson.org/)
 - [LodePNG](http://lodev.org/lodepng/)
 - [Miniz](https://github.com/richgel999/miniz)
 - [stb](https://github.com/nothings/stb)
 - [tinyobjloader](https://github.com/syoyo/tinyobjloader)
 - [lua](https://www.lua.org)
	
