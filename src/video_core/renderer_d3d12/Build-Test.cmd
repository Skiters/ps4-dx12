@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /O2 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DFMT_HEADER_ONLY /I"%~dp0..\.." /I"%~dp0..\..\..\..\shadps4-xess-src\build-xess\_deps\fmt-src\include" "%~dp0test.cpp" "%~dp0rasterizer.cpp" /Fe:"%~dp0rasterizer-test.exe" /Fo.\ /link d3d12.lib dxgi.lib
if errorlevel 1 exit /b 1
"%~dp0rasterizer-test.exe"
exit /b %ERRORLEVEL%



