OpenGL-based application framework for graphics samples, prototyping, etc. 

**Note that this project is very much WIP and therefore frequently subject to breaking changes!**

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
- [dr_wav](https://github.com/mackron/dr_libs)
- [LodePNG](http://lodev.org/lodepng/)
- [PortAudio](http://www.portaudio.com/)
- [stb](https://github.com/nothings/stb)
- [Im3d](https://github.com/john-chapman/im3d/)
- [ImGui](https://github.com/ocornut/imgui)
- [tinyobjloader](https://github.com/syoyo/tinyobjloader)
- [lua](https://www.lua.org)
