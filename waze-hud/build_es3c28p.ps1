# Build the ES3C28P/ES3N28P (ESP32-S3) firmware and merge it into one factory
# image that is flashed at offset 0x0.
#
#   .\build_es3c28p.ps1            -> dist\waze_hud_es3c28p_factory.bin
#   .\build_es3c28p.ps1 -Mock      -> dist\waze_hud_es3c28p_mock_factory.bin (no phone needed)
#   .\build_es3c28p.ps1 -Port COM5 -> also flashes the merged image
#
# dependencies.lock stores a single target, so the S3 lock file is swapped in
# for the build and the CYD lock file is restored afterwards.
param(
    [switch]$Mock,
    [string]$Port = '',
    [string]$IdfProfile = 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
. $IdfProfile | Out-Null

$suffix = if ($Mock) { '-mock' } else { '' }
$buildDir = "build-s3$suffix"
$sdkconfig = "sdkconfig.es3c28p$($suffix -replace '-', '.')"
$defaults = 'sdkconfig.es3c28p.defaults'
if ($Mock) { $defaults += ';sdkconfig.mock.defaults' }
$output = "dist\waze_hud_es3c28p$($suffix -replace '-', '_')_factory.bin"
$cydLock = 'dependencies.lock.cyd.tmp'

Copy-Item dependencies.lock $cydLock -Force
Copy-Item dependencies.es3c28p.lock dependencies.lock -Force
try {
    $common = @('-B', $buildDir, '-D', "SDKCONFIG=$sdkconfig", '-D', "SDKCONFIG_DEFAULTS=$defaults")
    if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
        idf.py @common set-target esp32s3
        if ($LASTEXITCODE -ne 0) { throw 'set-target failed' }
    }
    idf.py @common build
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }

    Push-Location $buildDir
    try {
        python -m esptool --chip esp32s3 merge_bin -o "..\$output" --flash_mode dio `
            --flash_size 16MB --flash_freq 80m '@flash_args'
        if ($LASTEXITCODE -ne 0) { throw 'merge_bin failed' }
    } finally { Pop-Location }
    Write-Host "Factory image: $output (flash at 0x0)"

    if ($Port) {
        python -m esptool --chip esp32s3 -p $Port -b 460800 write_flash 0x0 $output
        if ($LASTEXITCODE -ne 0) { throw 'flash failed' }
    }
} finally {
    Copy-Item dependencies.lock dependencies.es3c28p.lock -Force
    Copy-Item $cydLock dependencies.lock -Force
    Remove-Item $cydLock -Force
}
