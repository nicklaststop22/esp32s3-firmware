@echo off
set PORT=%1
if "%PORT%"=="" set PORT=COM5

echo Flashing full 16MB image to %PORT%...
esptool --chip esp32s3 --port %PORT% --baud 921600 --before usb-reset write-flash 0x0 "%~dp0..\firmware\esp32s3_full_16MB_firmware_backup.bin"
pause
