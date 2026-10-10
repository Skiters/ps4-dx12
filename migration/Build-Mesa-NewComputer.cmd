@echo off
setlocal
set "PORT_WORK=%~dp0..\.."
set "PORT_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%PORT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PORT_VS=%%I"
if not defined PORT_VS exit /b 3
call "%PORT_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 4
set "PATH=%PORT_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
for %%F in (dxil_nir.c dxil_signature.c nir_to_dxil.c) do copy /y "%~dp0mesa-source-overlay\%%F" "%PORT_WORK%\mesa-dxil-source\src\microsoft\compiler\%%F" >nul
python -m mesonbuild.mesonmain setup "%PORT_WORK%\mesa-dxil-bary-build" "%PORT_WORK%\mesa-dxil-source" -Dspirv-to-dxil=true -Dgallium-drivers=[] -Dvulkan-drivers=[] -Dllvm=disabled -Dplatforms=[] -Dopengl=false -Degl=disabled -Dgles1=disabled -Dgles2=disabled -Dglx=disabled -Dmicrosoft-clc=disabled -Dbuildtype=release -Dc_args=/wd4189
if errorlevel 1 exit /b %errorlevel%
if not defined SHADPS4_BUILD_JOBS set "SHADPS4_BUILD_JOBS=14"
ninja -C "%PORT_WORK%\mesa-dxil-bary-build" -j %SHADPS4_BUILD_JOBS% src/microsoft/spirv_to_dxil/spirv_to_dxil.dll
exit /b %errorlevel%
