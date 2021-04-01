@ECHO OFF
SETLOCAL
PUSHD %dp0%

IF EXIST *.DDS DEL /F /Q *.DDS

PAUSE

CALL _texconv.bat *_basecolor.*
CALL _texconv.bat *_normal.*
CALL _texconv.bat *_roughness.*
CALL _texconv.bat *_occlusion.*
CALL _texconv.bat *_alpha.*
CALL _texconv.bat *_height.*
CALL _texconv.bat *_metallic.*

POPD
ENDLOCAL
