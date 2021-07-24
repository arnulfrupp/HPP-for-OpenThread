@echo off

if exist zflash.cmd (
    echo.
    echo - Going to project drive root directory -
    cd..
)

IF "%ZEPHYR_BASE%"=="" (
    CALL "scripts\zsetenv.cmd"
    set DO_PAUSE=yes
)


IF "%ZBOARD%"=="nrf52840dongle_nrf52840" ( 
 
    IF NOT EXIST "%ZEPHYR_BASE%\..\toolchain\opt\bin\Scripts\nrfutil.exe" (
        echo.
       	echo Application "nrfFutil" not found.
        echo Installing wiht "pip install nrfutil ..."
        echo.
        pause
        pip install nrfutil
        echo.
        pause
        echo.
    )

    nrfutil pkg generate --hw-version 52 --sd-req=0x00 --application %ZBUILDDIR%\zephyr\zephyr.hex --application-version 1 scripts\output\build.zip
    echo.
    echo Put dongle at port %ZCOM% in programming mode [side button]
    echo - edit "scripts\var\dongle_com_port.txt" to change COM port -
    nrfutil dfu usb-serial -pkg scripts\output\build.zip -p %ZCOM%

) else (

    west flash -d %ZBUILDDIR%
)

@echo off
IF "%DO_PAUSE%" == "yes" pause