# Render the ES3C28P HUD (320x240) on the PC, with no ESP-IDF and no board, to check the layout.
#
#   pip install ziglang          (provides a C++ compiler; no other tools needed)
#   .\run.ps1                    -> out\sheet_*.png contact sheets (2x scale) of many scenarios
#
# The real hud_renderer.cpp, bitmap_font.cpp, generated_assets.cpp, trip_stats.cpp and the touch/OTA
# stubs are compiled against stub ESP headers; the display driver is replaced by a framebuffer.
$ErrorActionPreference = 'Stop'
$main = (Resolve-Path "$PSScriptRoot\..\..\waze-hud\main").Path
$out = Join-Path $PSScriptRoot 'out'
New-Item -ItemType Directory -Force $out | Out-Null
$flags = @('-std=gnu++17', '-O1', '-Wno-nullability-completeness',
           '-include', "$PSScriptRoot\stubs\gmtime_stub.h", '-include', 'algorithm', '-include', 'array',
           "-I$PSScriptRoot\stubs", "-I$main")
$sources = @("$main\display\hud_renderer.cpp", "$main\display\bitmap_font.cpp",
             "$main\assets\generated_assets.cpp", "$main\state\trip_stats.cpp",
             "$main\touch\touch_sensor_stub.cpp", "$main\ota\wifi_ota_manager_stub.cpp")

python -m ziglang c++ @flags "$PSScriptRoot\preview.cpp" @sources -o "$out\preview.exe"
if ($LASTEXITCODE -ne 0) { throw 'preview build failed' }
$env:OUTDIR = $out
& "$out\preview.exe"
$sheets = @(
  @('n00_nav_no_alert','n01_one_alert','n02_two_alerts','n03_one_card','n04_far_only','n04b_free_one_card'),
  @('n05_lanes_alerts','n06_traffic','n07_nopassing','n08_overspeed','n09_roundabout','n10_long_names'),
  @('f00_free_drive','f01_free_drive_alerts','s00_waiting','s01_connected_waiting','s02_system_status','s03_night'))
foreach ($names in $sheets) { python "$PSScriptRoot\sheet.py" $out @names }
