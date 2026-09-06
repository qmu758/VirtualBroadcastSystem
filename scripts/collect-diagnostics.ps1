param([string]$Config="config/example.toml",[string]$Ts="output/debug.ts",[string]$Output="diagnostics")
$exe=Resolve-Path "build-ninja-x64/VirtualBroadcaster.exe";New-Item -ItemType Directory -Force $Output|Out-Null
& $exe --diagnostics --config $Config|Set-Content "$Output/channel-list.txt" -Encoding utf8
& $exe --self-check $Config|Set-Content "$Output/self-check.txt" -Encoding utf8
if(Test-Path $Ts){& $exe --analyze-ts $Ts|Set-Content "$Output/ts-report.txt" -Encoding utf8}
Get-ChildItem *.log -ErrorAction SilentlyContinue|Copy-Item -Destination $Output -Force
"OS=$([Environment]::OSVersion.VersionString)`nProcessArchitecture=$([Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture)`nCollected=$(Get-Date -Format o)"|Set-Content "$Output/versions.txt"
"Configuration values are not copied; only validation output is included to avoid exposing local paths."|Set-Content "$Output/config-sanitized.txt"
