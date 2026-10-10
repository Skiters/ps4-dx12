@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -C "%~dp0..\..\mesa-dxil-bary-build" -f bary-overlay.ninja -j 2 src/microsoft/spirv_to_dxil/spirv_to_dxil.dll
exit /b %errorlevel%
