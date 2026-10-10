@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
set "DEBUG_TOOL_OUTPUT=debug-launcher"
if "%~1"=="snapshot" set "DEBUG_TOOL_OUTPUT=stack-snapshot"
cl /nologo /EHsc /std:c++20 /O2 "%~dp0debug_launcher.cpp" /Fo"%~dp0debug-tools\%DEBUG_TOOL_OUTPUT%.obj" /Fe"%~dp0debug-tools\%DEBUG_TOOL_OUTPUT%.exe" /link dbghelp.lib psapi.lib
exit /b %errorlevel%
