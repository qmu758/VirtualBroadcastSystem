param([int]$Port=12340,[int]$Seconds=30)
$ffmpeg=Get-Command ffmpeg -ErrorAction SilentlyContinue;if(-not$ffmpeg){Write-Host "SKIP: ffmpeg not installed";exit 0}
& $ffmpeg.Source -re -f lavfi -i "testsrc2=size=1280x720:rate=30000/1001" -f lavfi -i "sine=frequency=1000:sample_rate=48000" -t $Seconds -c:v mpeg2video -b:v 8M -c:a aac -b:a 192k -f mpegts "udp://127.0.0.1:$Port?pkt_size=1316"
