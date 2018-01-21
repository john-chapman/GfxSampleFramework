OpenGL framework for graphics samples, prototyping, etc. 

See [GfxSamples](https://github.com/john-chapman/GfxSamples) as a reference for the project setup and usage.

Use `git clone --recursive` to init/clone all submodules, as follows:

```
git clone --recursive https://github.com/john-chapman/GfxSampleFramework.git
```

Build via build/premake.lua as follows, requires [premake5](https://premake.github.io/):

```
premake5 --file=premake.lua [target]
```

### Dependencies

Submodule dependencies:
- [ApplicationTools](https://github.com/john-chapman/ApplicationTools)
 
Embedded dependencies:
- [EASTL](https://github.com/electronicarts/EASTL)
- [linalg](https://github.com/john-chapman/linalg)
- [LodePNG](http://lodev.org/lodepng/)
- [Miniz](https://github.com/richgel999/miniz)
- [RapidJSON](http://rapidjson.org/)
- [stb](https://github.com/nothings/stb)
- [Im3d](https://github.com/john-chapman/im3d/)
- [ImGui](https://github.com/ocornut/imgui)
- [tinyobjloader](https://github.com/syoyo/tinyobjloader)
- [lua](https://www.lua.org)
