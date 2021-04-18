@echo off

if exist zbuild.cmd (
    echo.
    echo - Going to project drive root directory -
    cd..
)

IF "%ZEPHYR_BASE%"=="" (
    CALL "scripts\zsetenv.cmd"
    set DO_PAUSE=yes
)

@echo on
west build -b %ZBOARD% -d %ZBUILDDIR%

@echo off
IF "%DO_PAUSE%" == "yes" pause
