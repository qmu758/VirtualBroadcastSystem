param([string]$Root=(Resolve-Path "$PSScriptRoot\.."))
$out=Join-Path $Root "outputs\Milestone7";$zip=Join-Path $Root "outputs\VirtualBroadcastSystem-Milestone7.zip"
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item "$Root\build-ninja-x64\VirtualBroadcaster.exe","$Root\build-ninja-x64\BonDriver_Virtual_x64.dll","$Root\build-ninja-x86\BonDriver_Virtual_x86.dll" $out -Force
Copy-Item "$Root\config\example.toml","$Root\config\multi-channel-example.toml","$Root\config\BonDriver_Virtual.toml","$Root\README.md" $out -Force
Copy-Item "$Root\sample-data" "$out\sample-data" -Recurse -Force
Copy-Item "$Root\scripts" "$out\scripts" -Recurse -Force
Copy-Item "$Root\tests" "$out\tests" -Recurse -Force
Copy-Item "$Root\include" "$out\include" -Recurse -Force
Copy-Item "$Root\src" "$out\src" -Recurse -Force
Copy-Item "$Root\CMakeLists.txt","$Root\CMakePresets.json" $out -Force
Compress-Archive -Path "$out\*" -DestinationPath $zip -Force
Get-FileHash -Algorithm SHA256 $zip
