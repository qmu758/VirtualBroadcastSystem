param([Parameter(Mandatory=$true)][string]$Path,[switch]$Epg,[switch]$ExtractLogo,[string]$LogoOutput="logo-data.bin")
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path))
if (($bytes.Length % 188) -ne 0) { throw "TS size is not a multiple of 188" }
$pids = @{}
$eitTables = @{}
$sectionBuffers = @{}
$completeSections = @()
for ($offset = 0; $offset -lt $bytes.Length; $offset += 188) {
    if ($bytes[$offset] -ne 0x47) { throw "sync byte error at offset $offset" }
    $pid = (($bytes[$offset + 1] -band 0x1f) -shl 8) -bor $bytes[$offset + 2]
    if ($pids.ContainsKey($pid)) { $pids[$pid]++ } else { $pids[$pid] = 1 }
    if ($pid -eq 0x0010 -or $pid -eq 0x0011 -or $pid -eq 0x0029) {
        $payload = $offset + 4
        $afc = ($bytes[$offset + 3] -shr 4) -band 3
        if ($afc -eq 2 -or $afc -eq 3) { $payload += 1 + $bytes[$payload] }
        if ($payload -lt $offset + 188) {
            $pusi = (($bytes[$offset + 1] -band 0x40) -ne 0)
            if ($pusi) { $payload += 1 + $bytes[$payload]; $sectionBuffers[$pid] = [Collections.Generic.List[byte]]::new() }
            if ($sectionBuffers.ContainsKey($pid)) {
                $buffer = $sectionBuffers[$pid]
                for ($i=$payload; $i -lt $offset+188; $i++) { $buffer.Add($bytes[$i]) }
                if ($buffer.Count -ge 3) {
                    $total = 3 + ((($buffer[1] -band 15) -shl 8) -bor $buffer[2])
                    if ($buffer.Count -ge $total) { $completeSections += ,@($pid,[byte[]]$buffer.GetRange(0,$total)); $sectionBuffers.Remove($pid) }
                }
            }
        }
    }
    if ($pid -eq 0x0012 -and ($bytes[$offset + 1] -band 0x40)) {
        $payload = $offset + 4
        $afc = ($bytes[$offset + 3] -shr 4) -band 3
        if ($afc -eq 2 -or $afc -eq 3) { $payload += 1 + $bytes[$payload] }
        if ($payload -lt ($offset + 188)) {
            $section = $payload + 1 + $bytes[$payload]
            if ($section -lt ($offset + 188)) {
                $tid = $bytes[$section]
                if ($tid -eq 0x4e -or ($tid -ge 0x50 -and $tid -le 0x5f)) {
                    $eitTables[$tid] = 1 + $(if ($eitTables.ContainsKey($tid)) { $eitTables[$tid] } else { 0 })
                    if ($Epg -and $section + 26 -lt $offset + 188) {
                        $sid = ($bytes[$section+3] -shl 8) -bor $bytes[$section+4]
                        $sn = $bytes[$section+6]
                        $eid = ($bytes[$section+14] -shl 8) -bor $bytes[$section+15]
                        $start = '{0:X2}{1:X2} {2:X2}:{3:X2}:{4:X2}' -f $bytes[$section+16],$bytes[$section+17],$bytes[$section+18],$bytes[$section+19],$bytes[$section+20]
                        $duration = '{0:X2}:{1:X2}:{2:X2}' -f $bytes[$section+21],$bytes[$section+22],$bytes[$section+23]
                        'EIT table=0x{0:X2} section={1} service={2} event={3} start(MJD/BCD)={4} duration={5}' -f $tid,$sn,$sid,$eid,$start,$duration
                    }
                }
            }
        }
    }
}
foreach ($required in 0x0000, 0x0010, 0x0011, 0x0012, 0x0014, 0x0029) {
    if (-not $pids.ContainsKey($required)) { throw ('required PID 0x{0:X4} is absent' -f $required) }
}
$pids.GetEnumerator() | Sort-Object Name | ForEach-Object {
    'PID 0x{0:X4}: {1} packets' -f $_.Name, $_.Value
}
'All packets are 188 bytes with sync byte 0x47.'
foreach ($entry in ($eitTables.GetEnumerator() | Sort-Object Name)) { 'EIT table 0x{0:X2}: {1} section starts' -f $entry.Name,$entry.Value }
foreach ($entry in $completeSections) {
    $pid,$s = $entry
    if ($pid -eq 0x0010 -and $s[0] -eq 0x40) {
        $nid=($s[3]-shl 8)-bor $s[4];$networkLength=(($s[8]-band 15)-shl 8)-bor $s[9];$loop=10+$networkLength+2
        if ($loop+6 -lt $s.Length) { $tsid=($s[$loop]-shl 8)-bor $s[$loop+1];$onid=($s[$loop+2]-shl 8)-bor $s[$loop+3];$dl=(($s[$loop+4]-band 15)-shl 8)-bor $s[$loop+5];$d=$loop+6;$end=$d+$dl
            while ($d+1 -lt $end) { $tag=$s[$d];$len=$s[$d+1];if ($tag -eq 0xcd -and $len -ge 2) { 'NIT NID={0} TSID={1} ONID={2} Remote Control Key ID={3}' -f $nid,$tsid,$onid,$s[$d+2] };$d+=2+$len }
        }
    }
    if ($pid -eq 0x0011) {
        for ($i=11; $i+8 -lt $s.Length-4; $i++) { if ($s[$i] -eq 0xcf -and $s[$i+1] -eq 7 -and $s[$i+2] -eq 1) {
            $id=(($s[$i+3]-band 1)-shl 8)-bor $s[$i+4];$ver=(($s[$i+5]-band 15)-shl 8)-bor $s[$i+6];$did=($s[$i+7]-shl 8)-bor $s[$i+8]
            'SDT Logo Transmission Descriptor type=0x01 logo_id={0} logo_version={1} download_data_id=0x{2:X4}' -f $id,$ver,$did; break
        } }
    }
    if ($pid -eq 0x0029 -and $s[0] -eq 0xc8) {
        $did=($s[3]-shl 8)-bor $s[4];$ver=($s[5]-shr 1)-band 31;$onid=($s[8]-shl 8)-bor $s[9]
        $type=$s[13];$id=(($s[14]-band 1)-shl 8)-bor $s[15];$lver=(($s[16]-band 15)-shl 8)-bor $s[17];$size=($s[18]-shl 8)-bor $s[19]
        'CDT PID=0x0029 table_id=0xC8 download_data_id=0x{0:X4} version={1} ONID={2} data_type=0x{3:X2} section={4}/{5} logo_type={6} logo_id={7} logo_version={8} data_size={9}' -f $did,$ver,$onid,$s[10],$s[6],$s[7],$type,$id,$lver,$size
        if ($ExtractLogo -and $size -gt 0) { [IO.File]::WriteAllBytes($LogoOutput,$s[20..(19+$size)]); "Extracted raw ARIB logo data to $LogoOutput" }
    }
}
