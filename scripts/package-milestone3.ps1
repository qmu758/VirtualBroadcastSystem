$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'outputs\Milestone3'
$zip = Join-Path $root 'outputs\VirtualBroadcastSystem-Milestone3.zip'
if (Test-Path $out) { Remove-Item -LiteralPath $out -Recurse -Force }
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item "$root\build-ninja-x64\VirtualBroadcaster.exe" $out
Copy-Item "$root\build-ninja-x64\BonDriver_Virtual_x64.dll" $out
Copy-Item "$root\build-ninja-x86\BonDriver_Virtual_x86.dll" $out
Copy-Item "$root\config\example.toml" $out
Copy-Item "$root\config\BonDriver_Virtual.toml" $out
Copy-Item "$root\README.md" $out
$source = Join-Path $out 'source'
New-Item -ItemType Directory -Path $source | Out-Null
foreach ($item in 'include','src','tests','scripts','config') { Copy-Item "$root\$item" $source -Recurse }
Copy-Item "$root\CMakeLists.txt","$root\CMakePresets.json","$root\README.md" $source
if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path "$out\*" -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Created $out"
Write-Host "Created $zip"
