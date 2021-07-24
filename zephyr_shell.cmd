@ECHO OFF
CALL "%~dp0scripts\zsetenv.cmd"

set PATH=%PATH%;%CD%\scripts

echo.
echo Building for board: "%ZBOARD%"
echo SDK Version: "%NCS_TAG%"
echo.

START /B /WAIT CMD
EXIT