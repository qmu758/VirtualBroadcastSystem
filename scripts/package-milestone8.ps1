param([string]$Root=(Resolve-Path "$PSScriptRoot\.."))
$Root=(Resolve-Path -LiteralPath $Root).Path
$out=Join-Path $Root "outputs\Milestone8";$zip=Join-Path $Root "outputs\VirtualBroadcastSystem-Milestone8.zip"
if (Test-Path -LiteralPath $out) {
  $resolved=(Resolve-Path -LiteralPath $out).Path
  if ($resolved -ne $out -or $resolved -notlike "$Root\outputs\*") { throw "Refusing to remove unexpected output path: $resolved" }
  Remove-Item -LiteralPath $resolved -Recurse -Force
}
New-Item -ItemType Directory -Force $out|Out-Null
Copy-Item "$Root\build-ninja-x64\VirtualBroadcaster.exe","$Root\build-ninja-x64\BonDriver_Virtual_x64.dll","$Root\build-ninja-x64\vb_generate_sample_ts.exe","$Root\build-ninja-x86\BonDriver_Virtual_x86.dll" $out -Force
Copy-Item "$Root\config\example.toml","$Root\config\multi-channel-example.toml","$Root\config\BonDriver_Virtual.toml","$Root\config\existing-ids-example.json","$Root\README.md","$Root\VERSIONS.txt","$Root\STATIC_ANALYSIS.txt","$Root\output\sample.ts" $out -Force
Copy-Item "$Root\config\sample-logo.png","$Root\config\sample-logo-a.png","$Root\config\sample-logo-b.png","$Root\config\sample-logo-c.png" $out -Force
foreach($name in "sample-data","scripts","tests","tools","include","src"){Copy-Item "$Root\$name" "$out\$name" -Recurse -Force}
Copy-Item "$Root\CMakeLists.txt","$Root\CMakePresets.json","$Root\LICENSE" $out -Force
foreach($config in "example.toml","multi-channel-example.toml") {
  $path=Join-Path $out $config
  (Get-Content -LiteralPath $path -Raw).Replace('../sample-data/','sample-data/') | Set-Content -LiteralPath $path -Encoding utf8NoBOM
}
Compress-Archive -Path "$out\*" -DestinationPath $zip -Force
Get-FileHash $zip -Algorithm SHA256
