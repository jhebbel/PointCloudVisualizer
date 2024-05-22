@echo off

IF NOT EXIST build mkdir build
pushd build

set files=../code/main.c
set compile_flags=/std:c11 /nologo /GR- /EHa- /Oi /WX /W4 /wd4100 /wd4189 /external:anglebrackets /external:W0 /FC /I..\third_party\windows
set linker_flags=/opt:ref /subsystem:console user32.lib gdi32.lib shell32.lib opengl32.lib ws2_32.lib ..\lib\glfw3_mt.lib

echo Building...
echo:

echo Debug Build
cl %compile_flags% /MT /Od /Z7 /DDEBUG /D_DEBUG /D_CRT_SECURE_NO_WARNINGS /Fe"debug" %files% /link %linker_flags%

echo:

echo Release Build
cl %compile_flags% /MT /O2 /DRELEASE /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /Fe"release" %files% /link %linker_flags%

popd
