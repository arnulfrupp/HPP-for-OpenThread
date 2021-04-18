@ECHO OFF

set NCS_TAG=v1.5.1

CALL "%HOMEPATH%\ncs\%NCS_TAG%\toolchain\cmd\env.cmd"

IF NOT EXIST "%ZEPHYR_BASE%\..\.west\config" (
    echo.
    echo NCS repositories are not here !!!
    echo.
    echo.
    pause
    exit
)

SETLOCAL
set /p ZTARGET=<%~dp0\target.txt

IF "%ZTARGET%"=="" ( 
    echo.
    echo File target.txt not found in /scripts !!!
    echo.
    echo.
    pause
    exit
)


::nRF52840 development board
::==========================

IF "%ZTARGET%"=="dk" ( 

    ENDLOCAL 
    set ZBOARD=nrf52840dk_nrf52840
    set ZBUILDDIR=build_nrf52840dk
)

::nRF52840 dongle
::===============

IF "%ZTARGET%"=="dongle" ( 
  
    ENDLOCAL
    set ZBOARD=nrf52840dongle_nrf52840
    set ZBUILDDIR=build_nrf52840dongle
    set /p ZCOM=<%~dp0\dongle_com_port.txt
)


::Board unknown?
::==============

IF "%ZBOARD%"=="" ( 
    echo.
    echo ###############################################
    echo Unknown target board in /scripts/target.txt !!!
    echo.
    echo Use "ztarget <target>" to set the target board
    echo ###############################################
    echo.
)

ENDLOCAL