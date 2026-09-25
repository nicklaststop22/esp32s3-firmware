# Flashes the complete 16MB firmware image to an ESP32-S3 target
param(
    [string]$Port = "COM5",
    [int]$Baud = 921600
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$FirmwareBin = Join-Path $ScriptDir "..\firmware\esp32s3_full_16MB_firmware_backup.bin"

if (-not (Test-Path $FirmwareBin)) {
    Write-Error "Firmware binary not found: $FirmwareBin"
    exit 1
}

Write-Host "Target Port: $Port" -ForegroundColor Cyan
Write-Host "Baud Rate:   $Baud" -ForegroundColor Cyan
Write-Host "Firmware:    $FirmwareBin" -ForegroundColor Cyan
Write-Host "Flashing full 16MB image..." -ForegroundColor Yellow

esptool --chip esp32s3 --port $Port --baud $Baud --before usb-reset write-flash 0x0 "$FirmwareBin"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSuccessfully flashed firmware to $Port!" -ForegroundColor Green
} else {
    Write-Host "`nFlashing failed. Verify your port and ensure the device is in bootloader mode (Hold BOOT, press RESET, release BOOT)." -ForegroundColor Red
}
