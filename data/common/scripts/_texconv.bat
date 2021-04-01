@ECHO OFF
SETLOCAL
PUSHD %dp0%

SET tx=%1
SET format=R8G8B8A8_UNORM
SET filter=TRIANGLE

ECHO %tx% | FIND /C /I "_basecolor" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC1_UNORM
)

ECHO %tx% | FIND /C /I "_normal" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC5_UNORM
)

ECHO %tx% | FIND /C /I "_roughness" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC4_UNORM
)

ECHO %tx% | FIND /C /I "_occlusion" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC4_UNORM
)

ECHO %tx% | FIND /C /I "_alpha" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC4_UNORM
)

ECHO %tx% | FIND /C /I "_height" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC4_UNORM
)

ECHO %tx% | FIND /C /I "_metallic" 1>NUL
IF NOT ERRORLEVEL 1 (
	SET format=BC4_UNORM
)

:: \todo texconv fails if the number of mips is invalid?
SET args=-nologo -ft DDS -dx10 -f %format% -if %filter% -y -wrap
ECHO %args%
CALL texconv %args% %tx%

POPD
ENDLOCAL
