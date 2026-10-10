@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /O2 /DWIN32_LEAN_AND_MEAN /DNOMINMAX texture_test.cpp /Fe:texture-test.exe /Fo:texture-test.obj /link d3d12.lib dxgi.lib
if errorlevel 1 exit /b 1
texture-test.exe
exit /b %ERRORLEVEL%
