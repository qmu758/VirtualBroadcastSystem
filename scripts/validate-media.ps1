param([Parameter(Mandatory=$true)][string]$Input,[string]$Report="ffprobe-report.json")
$probe=Get-Command ffprobe -ErrorAction SilentlyContinue;if(-not$probe){Write-Host "SKIP: ffprobe not installed";exit 0}
& $probe.Source -v error -show_programs -show_streams -show_format -show_packets -read_intervals "%+10" -of json $Input | Set-Content -Encoding utf8 $Report
if($LASTEXITCODE){throw "ffprobe validation failed"}
$json=Get-Content $Report -Raw|ConvertFrom-Json;$json.streams|ForEach-Object{"stream index=$($_.index) codec=$($_.codec_name) type=$($_.codec_type) resolution=$($_.width)x$($_.height) fps=$($_.avg_frame_rate) sample_rate=$($_.sample_rate)"}
