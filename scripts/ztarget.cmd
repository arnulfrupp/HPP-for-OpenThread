@echo off

IF "%ZEPHYR_BASE%"=="" (
    set DO_PAUSE=yes
)

IF "%1"=="dongle" GOTO SUCCESS
IF "%1"=="dk" GOTO SUCCESS

echo.
echo options:
echo target dk
echo target dongle

goto END

:SUCCESS

echo.
echo %1>%~dp0\var\target.txt
echo target board = "%1"
CALL "%~dp0\zsetenv.cmd"
echo.

:END

@echo off
IF "%DO_PAUSE%" == "yes" pause