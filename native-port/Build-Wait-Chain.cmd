@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /O2 /EHsc /utf-8 "%~dp0wait_chain.cpp" /Fo"%~dp0debug-tools\wait-chain.obj" /Fe"%~dp0debug-tools\wait-chain.exe" /link advapi32.lib
exit /b %errorlevel%
