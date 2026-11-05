@echo off
setlocal
rem This release's package, from build.bat's output (run that first, with the disc in assets): the runtime and the
rem compiler, no game code. Run it once with a European disc and once with the USA disc, each in a checkout of its own,
rem and attach both zips and their .json. The European discs share one package.
cd /d "%~dp0"
if not exist out\build\release\eon_release.txt (echo out\build\release\eon_release.txt is missing: run build.bat first. & exit /b 1)
set /p EON_RELEASE=<out\build\release\eon_release.txt
call sdk\scripts\release-package.bat EoN-%EON_RELEASE%-package.zip EoN EoNDriving
endlocal
