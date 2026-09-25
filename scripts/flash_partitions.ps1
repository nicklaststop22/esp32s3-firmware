# Flashes individual partitions to an ESP32-S3 target
param(
    [string]$Port = "COM5",
    [int]$Baud = 921600
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$PartDir = Join-Path $ScriptDir "..\firmware\partitions"

$Bootloader = Join-Path $PartDir "bootloader.bin"
$PartTable  = Join-Path $PartDir "partition_table.bin"
$OtaData    = Join-Path $PartDir "otadata.bin"
$App0       = Join-Path $PartDir "app0_firmware.bin"
$Spiffs     = Join-Path $PartDir "spiffs_littlefs.bin"

Write-Host "Target Port: $Port" -ForegroundColor Cyan
Write-Host "Flashing partitions to ESP32-S3..." -ForegroundColor Yellow

esptool --chip esp32s3 --port $Port --baud $Baud --before usb-reset write-flash `
    0x0000 "$Bootloader" `
    0x8000 "$PartTable" `
    0xe000 "$OtaData" `
    0x10000 "$App0" `
    0x670000 "$Spiffs"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nAll partitions successfully written to $Port!" -ForegroundColor Green
} else {
    Write-Host "`nPartition flashing encountered an error." -ForegroundColor Red
}
