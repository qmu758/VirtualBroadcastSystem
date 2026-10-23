# Virtual Broadcast System for Windows

OBS Studioの映像と音声をMPEG-2 TSとして受け取り、仮想BonDriver経由でTVTestへ渡すWindows向け仮想放送システムです。通信はPC内部で完結し、RF送信、OFDM変調、電波送信は行いません。

## 最初に読むところ

初めて使う場合は、次の順番で設定します。

1. `example.toml`で局名や出力先を確認する
2. `VirtualBroadcaster.exe`を起動する
3. OBSからUDP/MPEG-TSを送る
4. TVTestでBonDriverを選択する

## クイックスタート

### 1. 局名と入力ポートを確認する

配布フォルダの`example.toml`をUTF-8対応editorで開きます。通常は次の設定のまま開始できます。

```toml
[service]
name = "松山自主放送"
provider = "Virtual Broadcasting"

[input]
uri = "udp://127.0.0.1:12340"

[output]
file_enabled = true
pipe_enabled = true
file_path = "output/debug.ts"
max_file_size_mb = 50
```

`name`がTVTestに表示される局名です。`file_enabled = false`にすると診断TSを保存せず、Named Pipeへの配信だけを行います。

地域は`[region]`の`prefecture`と`area`で変更します。`[network] auto_id = true`にすると、局名からPC内仮想局用のONID/NID/TSID/SID候補を決定的に生成します。同じ局名なら再起動後も同じ値です。これは公式に予約された仮想用途IDではなく、実在局と衝突しないことを保証するものでもありません。`[ids.collision_check] existing_ids_file`へ既知のID一覧JSONを指定すると、候補との衝突時に起動を停止します。

### 2. VirtualBroadcasterを起動する

PowerShellで配布フォルダへ移動し、次を実行します。

```powershell
.\VirtualBroadcaster.exe --config .\example.toml
```

起動した画面は閉じずにそのままにします。設定を事前確認する場合は次を実行できます。

```powershell
.\VirtualBroadcaster.exe --self-check .\example.toml
.\VirtualBroadcaster.exe --diagnostics --config .\example.toml
```

### 3. OBSからUDP/MPEG-TSを送る

OBS Studioで以下を設定します。OBSのversionや表示言語によって項目名が多少異なる場合があります。

1. **設定** → **出力** を開く
2. **出力モード**を **詳細** にする
3. **録画**タブを開く
4. **種別**を **カスタム出力（FFmpeg）** にする
5. **FFmpeg出力の種類**を **URLに出力** にする
6. **ファイルパスまたはURL**へ `udp://127.0.0.1:12340?pkt_size=1316` を入力する
7. **コンテナフォーマット**を `mpegts` または `mpegts (MPEG-TS)` にする
8. 映像encoderを `mpeg2video`またはH.264、音声encoderをAAC、sample rateを48 kHz、audio trackを1にする
9. 設定を適用し、OBSメイン画面の **録画開始** を押す

通常の **配信開始** ではなく **録画開始** を使用します。`pkt_size=1316`は188-byte TS packetを7個ずつUDP datagramへまとめる指定です。

AACが表示されない場合は、先にcontainerを`mpegts`へ変更し、**互換性がない可能性のあるcodecをすべて表示**を有効にします。それでも選べない場合は`mp2` / `MPEG-2 Audio Layer II`、192 kbps、48 kHz stereoを使用できます。

音声がハウリングする場合は、OBSの音声ミキサーにある歯車から **オーディオの詳細プロパティ** を開き、音声sourceを **モニターオフ** にします。TVTestの再生音をOBSのデスクトップ音声で再取得すると音声が循環するため、TVTest確認中はデスクトップ音声を無効にするか、個別音声sourceだけを使用してください。

#### 放送に使う音声だけを出力する

OBSの初期設定ではPC全体の再生音を取り込む **デスクトップ音声** が有効なため、裏で再生した動画、通知音、TVTestの音声なども放送へ混ざります。次の構成に変更してください。

1. OBSの **設定** → **音声** → **グローバル音声デバイス**を開きます。
2. **デスクトップ音声**と**デスクトップ音声 2**を **無効** にします。
3. マイクを放送へ出さない場合は、**マイク音声**と**マイク音声 2～4**も **無効** にします。
4. OBSメイン画面の **ソース**欄で **+** → **アプリケーション音声キャプチャ（ベータ）**を追加します。この機能はOBS Studio 28以降のWindows 10 version 2004以降またはWindows 11で利用できます。
5. 仮想自主放送へ出したいplayer、browser、gameなどのwindow/processだけを選択します。titleが変化するアプリは、window titleより実行ファイル名を優先して照合します。
6. OBS 30.1以降では、**ウィンドウキャプチャ**または**ゲームキャプチャ**のpropertyにある **音声をキャプチャ（ベータ）**を使用する方法もあります。この場合、同じアプリの **アプリケーション音声キャプチャ**を重ねて追加しないでください。
7. 音声ミキサーで、放送対象の個別sourceだけmeterが動き、**デスクトップ音声**が存在しない、または常にmuteであることを確認します。
8. 各sourceの **オーディオの詳細プロパティ**で、UDP/MPEG-TS出力に使用するaudio track 1だけを選択します。

OBS自身のMedia Source、VLC Video Source、Browser Sourceなど、シーンへ直接追加したsourceの音声はデスクトップ音声を無効にしても出力できます。個別アプリ音声キャプチャに対応しないアプリは、Windowsの音量ミキサーでそのアプリだけをvirtual audio cableへ割り当て、OBSではそのcableを **音声入力キャプチャ**として追加します。

VirtualBroadcasterはOBSから受け取った圧縮済み音声ESをそのままPID変換してTVTestへ渡します。したがって、OBSが混合した後の音声から特定アプリだけをVirtualBroadcaster側で除去することはできません。

OBS公式手順: https://obsproject.com/kb/application-audio-capture-guide

### 4. TVTestで再生する

1. TVTestを終了する
2. 64-bit TVTestには`BonDriver_Virtual_x64.dll`、32-bit TVTestには`BonDriver_Virtual_x86.dll`をTVTestのBonDriverフォルダへ配置する
3. `BonDriver_Virtual.toml`も同じフォルダへ配置する
4. `VirtualBroadcaster.exe`とOBSの出力を開始する
5. TVTestを起動する
6. チューナーとして **Virtual Broadcast Tuner**を選択する
7. Space **Virtual**を選択する
8. Channel 0の局名を選択する

映像が出ない場合は、まず`VirtualBroadcaster.exe`の画面に`OBS program detected`が表示されているか確認します。次に`BonDriver_Virtual.log`で`OpenTuner`、Control Pipe、TS Pipe、`SetChannel`、`first TS packet received`を順に確認してください。

32-bit/64-bitが一致しないDLLはTVTestからロードできません。TVTestのbit数が不明な場合は、TVTestの配布ファイル名またはタスクマネージャーのprocess表記を確認してください。

### ブロックノイズが出る場合

画面の一部が大きな四角形や縦方向の崩れになる場合は、低bitrateだけでなくTS packet欠落の可能性があります。最新版ではUDP受信bufferを8 MiB、Named Pipe送信queueを約12 MiBへ拡大し、OBS入力のcontinuity欠落を検出します。

1. `VirtualBroadcaster.log`で`OBS input continuity loss`または`Client queue overflow`を検索します。
2. `BonDriver_Virtual.log`で`Ring buffer overflow`、`TS resync`、`disconnect`を検索します。
3. OBSのURLが`udp://127.0.0.1:12340?pkt_size=1316`で、同じportへ別のOBS/FFmpegが同時送信していないことを確認します。
4. MPEG-2 Videoの1080p30なら映像bitrateをまず15～20 Mbps、H.264なら8～12 Mbps程度で試します。1080p60や動きの多い映像ではさらに余裕を持たせます。
5. keyframe間隔は1～2秒を目安にします。30 fpsなら30～60 frames、60 fpsなら60～120 framesです。packet欠落後の画面回復も早くなります。
6. `output/debug.ts`にも同じ崩れがあるか確認します。ファイルにもあればOBS入力/encoder側、ファイルが正常でTVTestだけ崩れる場合はNamed Pipe、buffer、TVTest decoder側を重点的に調べます。

入力packetが欠落した場合、最新版は出力continuityにも不連続を反映するため、TVTestのD counterが増えることがあります。これは新たにpacketを壊したのではなく、従来はPID再配置時に隠れていた入力欠落を可視化したものです。

### 複数チャンネル

`multi-channel-example.toml`を使用し、各`[[channels]]`へ異なる`udp_port`を設定します。

```text
Channel 0: udp://127.0.0.1:12340?pkt_size=1316
Channel 1: udp://127.0.0.1:12341?pkt_size=1316
Channel 2: udp://127.0.0.1:12342?pkt_size=1316
```

起動コマンドは次のとおりです。

```powershell
.\VirtualBroadcaster.exe --config .\multi-channel-example.toml
```

## ドキュメント構成

- まず動かす: このページ冒頭のクイックスタート
- 問題を調べる: Milestone 8のdiagnosticsとTVTest checklist
- 設定項目を調べる: 「設定ファイル仕様」
- 内部構造を調べる: CMake、各class、Named Pipe protocol、PID割り当て
- 規格実装を調べる: Milestone 3以降のNIT/SDT/TOT/EIT/CDT/DSM-CC各章

## Milestone 8: 互換性診断と長時間安定性

Milestone 8では放送機能を増やさず、`--diagnostics`、`--self-check [config]`、`--analyze-ts file.ts`、BonDriver診断ログ、Control/TS Pipe再接続、buffer統計、互換性・stress test、診断scriptを追加しています。

### 対応状況

| 項目 | 実装 | 自動テスト | 実アプリ確認 |
|---|---:|---:|---:|
| 映像・音声のPID変換と時刻 | 対応 | Packet/PCR/PTS/DTS回帰 | OBS/TVTestは未確認 |
| BonDriver x86/x64 | 対応 | build/export/pipe/reconnect | TVTestは未確認 |
| 日本語SDT / EIT / JST TOT | 対応 | CRC/時刻/7日境界 | TVTest EPGは未確認 |
| CDTロゴ | 対応 | ID/CRC/安定性 | TVTestロゴ取得は未確認 |
| DSM-CC/BML | 対応 | DII/DDB round-trip/安定性 | BML viewerは未確認 |
| 複数channel/client | 対応 | 100回切替/同時client | 複数TVTestは未確認 |

### 診断コマンド

```powershell
VirtualBroadcaster.exe --diagnostics --config config/multi-channel-example.toml
VirtualBroadcaster.exe --self-check config/example.toml
VirtualBroadcaster.exe --analyze-ts output/channel-0.ts
scripts/inspect-ts.ps1 output/channel-0.ts -Epg
scripts/inspect-carousel.ps1 output/channel-0.ts -Pid 0x0200 -ExtractDirectory extracted
scripts/validate-media.ps1 output/channel-0.ts
scripts/collect-diagnostics.ps1 -Config config/example.toml -Ts output/channel-0.ts
```

The TS analyzer reports PID packet counts, continuity errors, scrambling, PCR backward jumps/gaps, table presence, section CRC errors, and PAT/PMT referenced-but-absent PIDs. The self-check validates channel IDs/PIDs, SI ID propagation, EPG construction, logo linkage/files, DSM-CC PMT relationship, carousel root/entry/resources, and configured limits. Buffer diagnostics retain current fill, maximum fill, overflow packet count, and reconnect count internally; transition logs provide the field evidence without logging every `GetTsStream`.

### OBSとFFmpegの互換性

Recommended OBS custom FFmpeg output is MPEG-TS to `udp://127.0.0.1:12340?pkt_size=1316`. Use MPEG-2 Video or H.264 video and MPEG-2 AAC audio at 48 kHz. The current parser recognizes MPEG-1/2, H.264 and HEVC video stream types and MPEG audio/AAC stream types; unsupported PMT stream types are rejected with no video/audio program lock. `scripts/send-test-pattern.ps1` generates copyright-free MPEG-2/AAC test input when FFmpeg exists. `scripts/validate-media.ps1` records codec, resolution, frame rate, audio sample rate and packet timestamp data when ffprobe exists; external tools remain optional.

### TVTest診断チェックリスト

1. DLL recognized: confirm `BonDriver_Virtual.log` contains `DLL/CreateBonDriver`; a 32-bit TVTest needs x86, and a 64-bit TVTest needs x64.
2. `OpenTuner`: check Control/TS Pipe connected messages; run broadcaster `--self-check` if unavailable.
3. Channel enumeration: check `ENUM_CHANNELS count=N` and `--diagnostics`.
4. `SetChannel`: check SetChannel/buffer purge logs and selected TS with `--analyze-ts`.
5. Video/audio: run `validate-media.ps1`; verify decoder support and PMT referenced PIDs.
6. Station name/remote key: inspect SDT/NIT with `inspect-ts.ps1`.
7. EPG/TOT: inspect EIT/TOT CRC and confirm PC JST/time configuration.
8. CDT logo: inspect SDT logo descriptor and CDT separately; clear TVTest logo cache or increment configured logo version for a repeat test.
9. Data broadcast: verify PMT component, DII, DDB and module reconstruction before testing a BML viewer/plugin. TVTest alone is not assumed to render BML.

TVTest's public documentation describes terrestrial logo acquisition from the tuned stream and potentially long/cache-delayed acquisition. ARIB STD-B10 defines BIT transmission parameters, but the public TVTest documentation and examined CDT/logo behavior do not establish BIT as a prerequisite for terrestrial CDT logo decoding. Therefore Milestone 8 does **not** emit a speculative BIT. An optional Milestone 8.1 remains appropriate only after a receiver trace or source-level requirement identifies the exact BIT broadcaster/first-loop descriptor fields needed.

### Endurance and limitations

The compatibility test advances the shared clock across seven days and the December 31/January 1 boundary, exercising TOT/EIT/MJD and p/f schedule generation. It performs 100 channel changes, a live server stop/restart with automatic full-session reconnect and channel restoration, CDT byte stability, DSM-CC DII/DDB stability, and buffer bounds. OBS interruption is naturally tolerated because UDP receive timeouts do not mutate mux state; dynamic input PMT/PID changes remain a limitation because current Milestone 1 program discovery locks the initial input program. Full BML DTD validation, actual 24-hour wall-clock soak, TVTest, OBS GUI, BML viewer, TSDuck, and `/analyze` were not available as real-application runs here.

Tests use CTest labels `unit`, `integration`, `compatibility`, and `stress`. Run ordinary groups with `ctest -L "unit|integration"`; run the endurance fixture explicitly with `ctest -L stress`.

## Milestone 7: BML / DSM-CC Data Carousel

Milestone 7 adds a channel-local DSM-CC **Data Carousel**. It does not add RF transmission, ISDB-T modulation, or an Object Carousel/BIOP layer. Each root-relative resource is a direct carousel module; `index.bml` is module ID 0, and remaining IDs are deterministic hashes of normalized relative paths. Module versions are content-hash based, so timestamps do not perturb a build.

### Signalling and carousel

- PMT ES: `stream_type=0x0D`, configurable PID (default `0x0200`).
- `stream_identifier_descriptor`: component tag `0x40` for the entry carousel.
- `data_component_descriptor`: tag `0xFD`, `data_component_id=0x000C`, `data_event_id=0xF`, `event_section_flag=0`.
- DII: table ID `0x3B`, message ID `0x1002`. DDB: table ID `0x3C`, message ID `0x1003`. Both have MPEG-2 CRC32.
- Default DDB block is 4066 bytes, keeping the complete DDB section within `section_length <= 4093`.
- The 32-bit carousel `downloadId` has the `data_event_id` in its high nibble. It is distinct from CDT's 16-bit `download_data_id`.

The application is terrestrial receiver **A-profile**, BML 1.0, direct-module Data Carousel. `module_info_byte` uses Type (`0x01`) and Name (`0x02`) descriptors, never private JSON. Supported resources: `.bml`, `.css`, `.js`, `.png`, `.jpg`, `.jpeg`. BML is XML 1.0 UTF-8 using STD-B24's UCS encoding option; PNG/JPEG remain their specified mono-media formats.

`dii_interval_ms=1000`, `carousel_cycle_ms=5000`, and `max_bitrate_kbps=256` are configurable virtual-compatibility defaults, not claims about mandatory RF broadcasting periods. At most one DDB section is emitted per due call. Existing OBS video/audio payload, PCR, PTS, and DTS are not modified; insertion slightly increases non-CBR TS bitrate.

Roots are canonicalized and restricted below the configured directory. Absolute/parent paths, unsupported extensions, missing entry BML, invalid image signatures, malformed BML roots, and configured limits fail explicitly. Defaults are 256 modules, 1 MiB/module, 16 MiB total. Disabled channels have no PMT data ES and no DII/DDB.

Use `[data_broadcast]` for the legacy channel or `[channels.data_broadcast]` immediately after `[[channels]]`. Inspect with `scripts/inspect-carousel.ps1 -Path output/channel-0.ts -Pid 0x0200`; add `-ExtractDirectory extracted` to reconstruct modules. The default sample application displays “松山自主放送 データ放送” and links news/weather pages without WebSocket, `fetch`, or localStorage.

TVTest alone is not assumed to render BML; a compatible viewer/plugin may be required. Diagnose PMT, DSM-CC PID, DII, DDB, reconstruction, then BML launch separately. Complete BML DTD/profile semantic validation, hot reload, EIT data-content linkage, event-section carousel, and Object Carousel are not implemented. TVTest/BML-viewer display is unverified here.

Specifications consulted: ARIB STD-B24 Version 6.4-E1 Volume 3; ARIB TR-B14 Version 6.7-E1 and publicly available Version 3.8-E1 Part 3. Official references: https://www.arib.or.jp/english/std_tr/broadcasting/desc/std-b24.html , https://www.arib.or.jp/english/std_tr/broadcasting/tr-b14.html , and https://www.arib.or.jp/english/html/overview/doc/6-STD-B24v6_4-2p3-1-E1.pdf .

## 1. ディレクトリ構成

```text
config/example.toml                 UTF-8設定
include/virtual_broadcast/common/   TS packet、CRC等
include/virtual_broadcast/broadcaster/
src/broadcaster/app/                main、VirtualBroadcaster
src/broadcaster/input/              ObsInput
src/broadcaster/mux/                TsMuxer、PSI/SI generators
src/broadcaster/config/             ConfigManager、IdManager
src/broadcaster/channel/            ChannelManager
src/broadcaster/transport/          NamedPipeServer (M2)
src/broadcaster/data/               DataCarousel (M7)
src/bondriver/                      M2の別プロジェクト
tests/unit/                         section/packet単体テスト
tests/integration/                  pipe、実時間muxテスト
output/                             debug.ts
scripts/                            VSビルド補助
```

将来用の空クラスは作らず、各Milestoneでビルド可能な機能単位として追加します。

## 2. CMake構成

| ターゲット | 種別 | 導入 | 内容 |
|---|---|---:|---|
| `vb_common` | static library | M1 | packet、MPEG-2 CRC32 |
| `vb_broadcaster_core` | static library | M1 | OBS入力、PSI、mux |
| `VirtualBroadcaster` | executable | M1 | 送出プロセス |
| `vb_tests` | executable | M1 | 単体テスト |
| `BonDriver_Virtual_x86` | DLL | M2 | Win32 BonDriver |
| `BonDriver_Virtual_x64` | DLL | M2 | x64 BonDriver |

`CMakePresets.json` に `win32-release` と `x64-release` を用意します。同一ソースから別build treeを生成し、M2ではDLL名をbitness別に固定します。Visual Studio 2022を基準とします。

## 3. 各クラスの責務

| クラス | 責務 |
|---|---|
| `VirtualBroadcaster` | 設定、入力、mux、出力のライフサイクル |
| `TsMuxer` | PSI/SI/PESの周期多重、PID remap、continuity管理 |
| `PsiSiGenerator` | section generator共通基盤、version/current-next |
| `PatGenerator` / `PmtGenerator` | program対応、PCR/ES loop生成 |
| `NitGenerator` / `SdtGenerator` | network、serviceとdescriptor生成 (M3) |
| `EitGenerator` | p/f、schedule、start_time/duration (M4) |
| `TotGenerator` | Windows時刻とJST offset descriptor (M3) |
| `CdtGenerator` | logo ID/version/download data ID整合とCDT (M5) |
| `AribStringEncoder` | UTF-8からARIB文字符号への変換境界 (M3) |
| `DataCarousel` | BML等のDSM-CC carousel化 (M7) |
| `ObsInput` | localhost UDP TS受信と入力program/ES解析 |
| `Encoder` | 将来のraw入力向け符号化境界。M1はOBS encoderを利用 |
| `ChannelManager` | 複数channelとpipeline管理 (M6) |
| `IdManager` | 仮想ID候補生成と既存情報との衝突検査 (M6) |
| `ConfigManager` | UTF-8 TOMLの読み込み、検証、snapshot更新 |
| `NamedPipeServer` | channel別TSと制御をDLLへ提供 (M2) |
| `PipeClient` / `TsBuffer` / `ChannelController` | 薄いBonDriver側の通信、FIFO、SetChannel制御 (M2) |

BonDriverは符号化、多重化、PSI/SI生成を一切行いません。

## 4. Named Pipeプロトコル (Milestone 2)

`\\.\pipe\VirtualBroadcast\control` を双方向message modeで使用します。固定ヘッダーはlittle-endianです。

```text
FrameHeader: magic u32=0x31504256 (VBP1), version u16=1,
             type u16, payloadSize u32, requestId u32
```

typeは `HELLO`、`ENUM_CHANNELS`、`CHANNEL_LIST`、`SET_CHANNEL`、`SET_CHANNEL_RESULT`、`TS_DATA`、`PURGE`、`STATUS`、`PING/PONG`。`TS_DATA` は188の整数倍かつ64 KiB未満です。切替成功後の最初のTS_DATAはPATから始めます。遅いclientは上限付きqueueから切断し、送出本体を止めません。

## 5. TS PID割り当て案

PC内仮想環境専用の既定値であり、公式な予約・割当値とは主張しません。

| PID | 用途 | 導入 |
|---:|---|---:|
| `0x0000` | PAT | M1 |
| `0x0001` | CAT（必要時） | 拡張 |
| `0x0010` / `0x0011` | NIT / SDT | M3 |
| `0x0012` | EIT | M4 |
| `0x0014` | TOT | M3 |
| `0x0029` | CDT | M5 |
| `0x0100` | PMT | M1 |
| `0x0101` | PCR + 映像ES | M1 |
| `0x0110` | 音声ES | M1 |
| `0x0120` | DSM-CC | M7 |
| `0x1FFF` | null | 必要時 |

複数serviceではPID blockを分け、予約PID・重複を検証します。

## 6. SI/PSI送出周期

| Table | 既定周期 |
|---|---:|
| PAT / PMT | 100 ms |
| NIT / SDT | 1 s |
| EIT p/f | 500 ms |
| EIT schedule | 10 s/table segment |
| TOT (PID `0x0014`) | 5 s |
| CDT | 5 s |
| DSM-CC | 1 s/carousel cycle目標 |

TOTは送出時ごとにWindows現在時刻を再取得します。全周期は設定可能にします。

## 7. 設定ファイル仕様

UTF-8 TOMLです。文字列はUTF-8で保持し、SI格納時だけ `AribStringEncoder` を通します。

```toml
[service]
name = "松山自主放送"
provider = "Virtual Broadcasting"
service_id = 101
remote_control_key_id = 8 # 1..12
[network]
original_network_id = 32760
network_id = 32760
transport_stream_id = 32513
auto_id = false
collision_check_files = []
[region]
prefecture = "愛媛県"
area = "松山"
timezone = "Asia/Tokyo"
[channel]
physical_channel = 20
center_frequency_mhz = 515.142857
[input]
uri = "udp://127.0.0.1:12340"
[output]
debug_output = true
debug_path = "output/debug.ts"
[epg]
mode = "FixedInterval" # FixedInterval | Schedule
interval_minutes = 60
default_title = "自主放送"
default_description = "Virtual Broadcast Program"
[[epg.events]]
start = "06:00"
duration_minutes = 120
title = "朝の自主放送"
description = "朝の番組"
[clock]
source = "System"
timezone = "Asia/Tokyo"
[logo]
enabled = true
path = "logo.png"
logo_id = 1
logo_version = 1
download_data_id = 1
[data_broadcast]
enabled = true
root = "data"
[period_ms]
pat = 100
pmt = 100
nit = 1000
sdt = 1000
eit_pf = 500
eit_schedule = 10000
tot = 5000
cdt = 5000
```

TVTestのサービス名を変更する場合は `example.toml` の `[service] name` を編集します。配布版の `VirtualBroadcaster.exe` は、カレントディレクトリに `config/example.toml` がなければEXEと同じディレクトリの `example.toml` を自動的に使用します。複数チャンネル設定では各 `[[channels]]` の `name` がBonDriverのチャンネル一覧名、`service_name` がSDT/TVTest上のサービス名です。

ScheduleはISO 8601と日付なし `HH:mm` を受理し、後者は地域timezoneの当日です。EPG総期間に固定上限は設けず、EITのsection/segment/tableへ分割します。

## 8. Milestone 1の具体的実装方針

OBSのカスタムFFmpeg出力を `udp://127.0.0.1:12340?pkt_size=1316`、format `mpegts` とします。映像は `mpeg2video`（互換性優先）またはH.264、音声はAACです。

具体的なOBSの画面操作、AACが選択できない場合、音声ハウリングの対処は冒頭の「OBSからUDP/MPEG-TSを送る」を参照してください。

`ObsInput` は188-byte同期を回復し、入力PAT/PMTからPCR PID、最初の映像/音声stream typeとPIDを検出します。入力PSI/SIとnull packetは破棄します。`TsMuxer` はservice ID `101`、PMT `0x0100`、映像/PCR `0x0101`、音声 `0x0110` でPAT/PMTを100 ms周期生成します。入力PESのPIDとcontinuity counterだけを置換し、adaptation field、PCR、payload、PTS/DTSは保持します。

出力は `output/debug.ts` へ逐次書き込みます。M1はOBSの実時間bitrateに従い、CBR用null packetは挿入しません。

検証対象はPAT/PMT CRC、section_length/table_id/version/current-next、188-byte/0x47、continuity、PCR単調増加、remap前後のPTS/DTS不変です。確認コマンドは `ffprobe -show_programs -show_streams output/debug.ts` です。

## 採用した既定判断

- ARIB文字、地上デジタルdescriptor、CDT logo、DSM-CCは該当Milestoneでfixtureと受信試験を追加して有効化します。
- M1はOBS自身のencoder/muxerを利用し、VirtualBroadcaster内で再エンコードしません。
- ID `32760`、`32513`、`101` は仮想環境向け候補にすぎず、公式に予約済みとは断定しません。
- 仮想周波数メタデータ、BonDriver channel index、実RF周波数制御は相互に独立です。

## Milestone 1 ビルドと実行

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
build/x64-release/VirtualBroadcaster.exe --port 12340 --output output/debug.ts
```

Win32は `win32-release` presetを使います。OBS出力開始後、Ctrl+Cで終了します。

## Milestone 2 実装状況

Milestone 2では `TsOutputRouter`、`NamedPipeServer`、`ClientSession`、`PipeClient`、`TsRingBuffer`、`ChannelController` と薄い `BonDriverVirtual` を追加しました。Milestone 1のmux出力は同じpacket列をファイルとpipeへ分岐するため、PAT/PMT、PID再配置、PCR/PTS/DTS、`debug.ts`の経路は変更していません。

当初案では制御frameとTS frameを一接続に多重化していましたが、Milestone 2の要件に合わせて次の2本へ分離しました。TS側が生byte streamになり、制御messageの混入で188-byte境界を壊す可能性がなくなるためです。

- Control: `\\.\pipe\VirtualBroadcastControl`、双方向byte stream
- TS: `\\.\pipe\VirtualBroadcastTS`、serverからclientへのbyte stream

VirtualBroadcasterは256 packet、48,128 byteを標準chunkとしてTS pipeへ渡します。停止時には未満のchunkもflushします。WriteFileはClientSessionの専用threadが行うため、mux threadを直接ブロックしません。Milestone 2は同時に一つのTVTest clientを対象とし、新しいTS clientが接続した場合は旧sessionを終了します。

### Control protocol version 1（Milestone 2当時）

各request/responseは16-byte headerと `payload_size` byteのpayloadです。全整数は符号なしlittle-endianで明示的にencodeし、C++構造体のpaddingやx86/x64のnative `sizeof` はwire formatに使いません。

```text
offset  size  field
0       4     magic = 0x31504256 ("VBP1" as little-endian integer)
4       2     protocol_version = 1
6       2     message_type
8       4     payload_size
12      4     request_id
```

request typeは `HELLO=1`、`GET_VERSION=2`、`OPEN=3`、`CLOSE=4`、`PURGE=5`、`GET_STATUS=6` です。responseはrequestと同じtype/request IDで、payload先頭4 byteにerror code、続く4 byteにserver protocol versionを返します。最大payloadは4096 byteです。

error codeは `OK=0`、`BAD_MAGIC=1`、`UNSUPPORTED_VERSION=2`、`UNKNOWN_MESSAGE=3`、`INVALID_PAYLOAD=4`、`UNAVAILABLE=5`、`INTERNAL=6` です。現在のOPEN/CLOSE/PURGEは単一channel sessionの状態通知で、将来 `ENUM_CHANNELS`、`SET_CHANNEL`、service情報を追加できるtype空間を残しています。

### BonDriver ABI

一般的な `IBonDriver` / `IBonDriver2` の仮想関数順とシグネチャを使用し、MSVCでDLLとTVTestのC++ ABIを一致させます。`CreateBonDriver`だけをC linkageで `.def` exportします。参照した公開実装は [BonDriver_Pipe](https://github.com/xtne6f/TvtPlay/blob/work/BonDriver_Pipe_src/BonDriver_Pipe.h) と [BonDriver_mirakc](https://github.com/stuayu/BonDriver_mirakc) です。

Win32 buildは `BonDriver_Virtual_x86.dll`、x64 buildは `BonDriver_Virtual_x64.dll` を生成します。`scripts/verify-exports.bat` はPE machine typeと装飾なし `CreateBonDriver` exportを検証します。

### Buffer、同期、再接続

DLLは既定16 MiBのpacket ringを持ちます。overflow時はリアルタイム視聴の遅延を増やさないよう最古packetを破棄します。`PurgeTsStream()`は全packetを破棄しControl PipeにもPURGEを通知します。

TS受信は専用threadです。接続直後と通常受信中に `0x47` を検査し、ずれた場合は候補位置、その+188、その+376のsync byteを確認して再同期します。切断時はbufferを消去し、300 msのbackoffと短い`WaitNamedPipe`でTS Pipeを再接続します。`CloseTuner()`はstop、`CancelIoEx`、thread join、handle closeの順です。VirtualBroadcaster未起動時の`OpenTuner()`はFALSEを返し、TVTest processを終了させません。後で再度OpenTunerを呼ぶと接続を再試行します。

`GetSignalLevel()`は接続中30.0、切断中0.0の仮想値であり、RF品質ではありません。Milestone 2の有効channelはSpace 0 / Channel 0だけです。

## Milestone 2 設定

Broadcasterの `config/example.toml` は以下の組み合わせをサポートします。

```toml
[output]
file_enabled = true
pipe_enabled = true
file_path = "output/debug.ts"
max_file_size_mb = 50
[pipe]
enabled = true
name = "VirtualBroadcastTS"
control_name = "VirtualBroadcastControl"
```

`file_enabled` / `pipe_enabled` によりFile only、Pipe only、File + Pipeを選べます。`max_file_size_mb` は1～4096 MiBで、既定値は50 MiBです。188-byte packet境界を守るため実ファイルは指定値以下の最大packet境界で停止し、Named Pipe送出はそのまま継続します。次回起動時にファイルをtruncateして新しい診断TSを記録します。BonDriverはDLLと同じdirectoryの `BonDriver_Virtual.toml` を読み、未配置時は安全な既定値を使います。

```toml
[connection]
control_pipe = "VirtualBroadcastControl"
ts_pipe = "VirtualBroadcastTS"
[buffer]
size_mb = 16
[tuner]
name = "Virtual Broadcast Tuner"
signal_level = 30.0
```

設定parserはMilestone 2で使用するscalar keyに限定したUTF-8 TOML subsetです。完全なTOML 1.0 parserへの差し替え境界は `LoadRuntimeConfig` に集約しています。

## TVTest確認手順

導入と再生の手順は冒頭の「TVTestで再生する」を参照してください。この章以降は実装詳細と診断情報です。

## Milestone 2の診断ログ

実行directoryへ `VirtualBroadcaster.log` と、TVTestのworking directoryへ `BonDriver_Virtual.log` を出力します。pipe作成、接続/切断、16 MiBごとの送信量、Open/Close、SetChannel、Purge、overflow、resync、再接続を記録し、packet単位では記録しません。

TVTest実機および実OBS映像での確認はこの開発環境では未実施です。代わりに両bitnessで実DLLをLoadLibraryし、Named Pipe経由のGetTsStreamまで統合テストしています。Milestone 2当時の単一channel構成はMilestone 6で複数channel/clientへ、限定的な再接続はMilestone 8でControl/TS両方の自動再接続へ拡張済みです。

## Milestone 3: NIT / SDT / TOT

### 実装構成

`PsiSiGenerator`がPAT/PMT/NIT/SDT/TOTの周期判断をまとめ、各generatorはsectionだけを生成します。共通`SectionPacketizer`がpointer field、複数packetへの分割、`0xFF` stuffing、PID別continuity counterを担当します。PIDはPAT `0x0000`、NIT `0x0010`、SDT `0x0011`、TOT `0x0014`で、既存PMT `0x0100`、映像 `0x0101`、音声 `0x0110`は変更していません。

NIT actualはtable ID `0x40`で、network ID、network name descriptor、単一TSのTSID/ONIDを送ります。SDT actualはtable ID `0x42`で、単一service、service descriptor、running status 4、free CA falseを送ります。Milestone 4未実装のためSDTのEIT schedule/present-following flagはfalseです。TOTはtable ID `0x73`で、生成するたびに`SystemClock`を読み直します。テストでは`IClock`/`FakeClock`を使用するためPC時計を変更しません。

既定周期はPAT/PMT 100 ms、NIT/SDT 1000 ms、TOT 5000 msです。20から60000 msの範囲外または不正な周期値はtableごとの既定値へ戻します。SI packetは入力TSへ追加する方式で、既存映像・音声packetのpayload、PCR、PTS、DTSは変更しません。このため出力bitrateはSI分だけわずかに増え、null packet置換によるCBR化は行いません。

### 日本語文字列

設定はUTF-8で保持し、NIT/SDT生成時だけ`AribStringEncoder`を通します。encoderはARIB 8-unit codeの初期G0 Kanjiとして、Windows CP932で得たShift-JISのJIS X 0208対応文字をrow/cellへ変換します。ASCIIはLS1 (`0x0E`)で英数字集合へ切り替え、必要時LS0 (`0x0F`)でKanjiへ戻します。これによりASCII、数字、ひらがな、カタカナ、一般的なJIS漢字を扱います。

完全なARIB追加記号、外字、DRCS、結合文字は未実装です。CP932/JIS X 0208へ変換できない文字はJISの疑問符へ置換し、結果の`replaced` flagで検出できます。UTF-8 byte列をそのままdescriptorへ格納するfallbackはありません。

### 時刻とJST offset

日本向けARIB STD-B10に従い、TOTの40-bit time fieldにはJSTを格納します。`SystemClock`が返すUTCの瞬時刻へ9時間を加えた後、そのJST暦日をMJD化し、JSTの時分秒を`BcdEncoder`でBCD化します。この順序によりUTC 15:00以降の日付繰り上がりと年越しもJST日付へ正しく反映します。

local time offset descriptorは設定要件に従いcountry `JPN`、region 0、polarity 0、current/next offset `09:00`を格納します。`time_of_change`もJSTのMJD/BCDで符号化し、日本に通常DSTがないため生成時刻と同一offsetを使うno-op遷移としています。ただしARIB STD-B10 Part 3の運用指針は、次の時刻変更が明確な場合のみdescriptorを配置し、それ以外は配置しないとしています。常時配置と`09:00`は本システムの明示設定要件による互換モードであり、国内実放送のDST運用を表すものではありません。

参照した公開文書は、ARIB STD-B10 Version 5.13-E1（2019-12-05英訳版）のNIT/SDT/TOT、local time offset descriptorおよびPart 3運用指針と、ARIB公式のSTD-B10 Version 5.14（2025-03-25）改定概要です。日本向けTOT時刻処理ではARIB STD-B10を優先します。

### Milestone 3手動確認

1. `VirtualBroadcaster.exe --config config/example.toml`を起動します。
2. OBSから`udp://127.0.0.1:12340?pkt_size=1316`へMPEG-TSを送ります。
3. TVTestと同じbitnessのBonDriver DLLを配置し、Space 0 / Channel 0を選びます。
4. 映像・音声、サービス名「松山自主放送」と文字化けの有無を確認します。
5. TOT由来の時刻を確認し、PC時計変更後は次のTOT周期で値が変わることを確認します。

この開発環境ではTVTestと実OBSによる手動確認は未実施です。自動テストはNIT/SDT/TOTの構文・length・CRC、日本語encode、MJD/BCD、UTC+9 descriptor、packetization/scheduler、および既存Named Pipe/BonDriver経路を検証します。

## Milestone 4: EIT / EPG

EITは専用PID `0x0012`で送出します。actual TSのpresent/followingはtable ID `0x4E`、scheduleは`0x50`～`0x5F`を使用し、other TS用tableは生成しません。SDTのEIT schedule/present-following flagはEPG有効時に1です。

`EpgEvent`はevent ID、UTC瞬時刻として保持するstart、duration、title、short/extended description、running status、free CA modeを持ちます。EIT格納時刻はTOTと同じ`MjdTimeEncoder::EncodeJst`を使い、UTCからJSTへ変換してからJST暦日をMJD、時分秒をBCD化します。durationは最大`99:59:59`の24-bit BCDで、`24:00:00`も扱います。

### p/f

ARIB STD-B10に従いsection 0はpresent、section 1はfollowingです。番組がない場合も対応sectionを空で送ります。`start <= now < end`をpresentとし、その後に開始する最初のeventをfollowingとします。presentのrunning statusはrunning、followingはnot running、free CA modeは0です。同じ`IClock`をTOTとEITが参照するため、境界を越えると再起動なしでp/fが切り替わります。ログはイベントID組が変わった時だけ出力します。

### schedule分割

scheduleはJSTの直近午前0時を`t0`とし、3時間ごとにsegmentへ配置します。1 table IDは32 segments、つまり4日を担当し、7日設定では`0x50`と`0x51`を使います。各segmentはsection番号`segment * 8`から始まり、4093-byte section length上限の前にevent境界で分割します。空segmentも1つの空sectionで表し、`segment_last_section_number`、`last_section_number`、`last_table_id`を実際の分割結果から設定します。8 sectionsを超えるsegmentは明示的エラーです。

### 番組設定

FixedIntervalは`fixed_anchor`を基準に任意の正整数分で安定した枠を作り、現在時刻からの相対枠にはしません。`{HH}`はJST開始時刻の2桁時へ置換します。event IDはservice ID、開始UTC秒、枠indexのFNV-1a hashから決定し、同じscheduleでは再現可能です。衝突時は未使用IDまで順に進めます。

Schedule modeは`[[epg.events]]`のISO-8601 `+09:00`開始時刻、正のduration、任意または自動event IDを読みます。無効日時、非JST timezone、0 duration、16-bit超過ID、event重複、4096件超過は設定エラーです。例は`config/schedule-example.toml`にあります。

short event descriptorはlanguage `jpn`、番組名、説明を格納し、両文字列を`AribStringEncoder`へ通します。descriptorの255-byte上限はARIB encoded byte数で判定し、UTF-8 code point境界で安全に切り詰めます。extended event descriptorは今回未実装で、長文はshort descriptor内でtruncateされます。ARIB追加記号、DRCS、外字の既存制限も継続します。

既定送出周期はp/f 500 ms、schedule 2000 msです。番組境界を素早く反映しつつ、7日分scheduleを過度に再送しない値として採用しました。versionは5-bitで、p/fのevent ID組またはschedule内容が変わった場合だけ増加し、31の次は0へ戻ります。

### TVTest確認

1. VirtualBroadcasterを起動し、OBSからUDP TSを送ります。
2. TVTestでbitnessの合うBonDriver_Virtualを選択し、松山自主放送を受信します。
3. EPG画面で日本語番組名、説明、現在番組、次番組を確認します。
4. 番組境界を跨ぎ、p/fが自動更新されることを確認します。
5. `interval_minutes`を60、120、1440へ変え、番組枠と24時間durationを確認します。

TVTest実機確認はこの開発環境では未実施です。`scripts/inspect-ts.ps1 -Path output/debug.ts -Epg`でEIT table ID、section、service/event ID、MJD/BCD開始時刻、durationを確認できます。参照仕様はARIB STD-B10 Version 5.13-E1 Part 2 5.2.7およびPart 3 1.4です。

## Milestone 5: CDT局ロゴ

局ロゴを有効にすると、SDT service loopへlogo transmission descriptor（tag `0xCF`、transmission type `0x01`）を追加し、同じTSのPID `0x0029`へCDT（table ID `0xC8`、data type `0x01`）を送ります。`logo_id`（9 bit）、`logo_version`（12 bit）、`download_data_id`（16 bit）は一つの`LogoSettings`からSDTとCDTの両方を生成するため不一致になりません。`download_data_id=0x8001`はPC内仮想局用の利用者設定であり、公式な仮想用途予約値ではありません。

CDTの5-bit `version_number`はsub-table内容の版、12-bit `logo_version`はservice logoの版で、別々に管理します。生成のたびには増加させません。`CdtVersionTracker`は内容変更通知時だけ0～31で循環でき、Milestone 5の実行設定では`cdt_version`を使用します。hot reloadとファイル監視は未実装です。

### ロゴ形式

既定`logo_type=5`はHD大ロゴの64 x 36 pixelsです。TR-B14の6種はtype 0=48x24、1=36x24、2=48x27、3=72x36、4=54x36、5=64x36として検証します。typeとCDT `section_number`を一致させ、type 5を選んだ場合は規定の連続sectionを満たすため0～4をdata size 0、section 5を実データとして送ります。`last_section_number`は5です。

`LogoEncoder`はWindows Imaging Componentで入力PNGをRGBA decodeし、`contain`で縦横比を保って規定寸法へresizeし、余白は透明にします。その後ARIB受信機共通の固定128色（RGB各0/85/170/255、opaque/half-alphaおよびtransparent）へ最近傍量子化し、8-bit indexed PNGへ圧縮します。ARIBロゴdataとして許されるIHDR、IDAT、IENDだけを残し、PLTEを送らない制約付きPNGを`data_byte`へ格納します。したがって入力PNGの生byteをそのまま格納していません。WICはWindows標準COM APIであり、追加の第三者ライブラリやruntime licenseはありません。

`auto_resize=false`では入力寸法が選択typeと一致しなければ起動エラーです。alphaは捨てず、透明・半透明・不透明の固定paletteへ量子化します。任意alpha値と任意RGBは固定paletteの最近傍値になるため、元画像と完全な色一致は保証しません。1 logo typeのみを設定でき、section上限4093 byteまたは16-bit `data_size`を超える結果は独自分割せずエラーにします。

CDT既定周期は30,000 msです。TR-B14の再送間隔上限10分以内を満たしつつ、PC内の受信互換性試験で待ち時間を短くする設定可能な既定値であり、実放送局の帯域運用を模倣する値ではありません。`logo.enabled=false`ならSDT descriptorもCDTも出力しません。

### 設定と診断

```toml
[logo]
enabled = true
source = "sample-logo.png"
logo_id = 1
logo_version = 0
download_data_id = 32769
logo_type = 5
cdt_version = 0
auto_resize = true
resize_mode = "contain"

[si]
cdt_interval_ms = 30000
```

画像は設定ファイルのdirectory基準で一度だけ読み込み、immutable moduleとして保持します。`scripts/inspect-ts.ps1 -Path output/debug.ts`はCDT header/moduleとSDT descriptorを表示し、`-ExtractLogo -LogoOutput logo-data.bin`は制約付きPNGのraw logo dataを抽出します。

TVTestでは、sample logoを配置してVirtualBroadcasterとOBSを起動し、対応bitnessのBonDriverで松山自主放送を受信します。チャンネル情報を更新してPID `0x0029`と局ロゴを確認します。キャッシュが残る場合は`logo_version`を増やしてVirtualBroadcasterを再起動します。本環境ではTVTest実機表示は未確認です。また、受信機によってはBITのSI伝送パラメータをロゴ取得判定に使うため、Milestone 5のCDT/SDTだけでは表示されない可能性があります。BIT追加は要求範囲外であり未実装です。

参照文書はARIB STD-B10 Version 5.13-E1 Part 2 §6.2.44（logo transmission descriptor）、ARIB STD-B21 Version 5.12-E2 Chapter 12 / Appendix 3（受信機ロゴ・固定色PNG）、ARIB TR-B14 Version 2.8-E2 Fascicle 1 Part 3 §4 Table 4-1および§5.4 Tables 5-14/5-15（サイズ、CDT、data module）です。公開英訳を優先して実装し、現行日本語版との改定差はTVTest実機確認時の確認事項とします。

## Milestone 6: 複数仮想チャンネル

`VirtualChannel`はchannel index、表示名、service/network/TS名、UDP入力、ONID/NID/TSID/SID、remote key、地域、仮想物理ch・中心周波数、EPG、ロゴ、debug出力を一単位にします。`ChannelManager`は列挙・検索・validationを担当し、各workerは独立した`ObsInput`、`TsMuxer`、`PsiSiGenerator`、scheduler、continuity counter、EPG、CDT moduleを持ちます。3局ならUDP 12340～12342から別々に受け、`output/channel-0.ts`～`channel-2.ts`へ出力します。固定上限はありません。

旧形式は廃止していません。`[[channels]]`がなければ従来の`[service]`、`[network]`、`[epg]`、`[logo]`をChannel 0へ昇格します。複数局の例は`config/multi-channel-example.toml`です。

### Control protocol v2

Milestone 2の16-byte little-endian headerと最大4096-byte payloadを維持し、versionを2へ更新しました。既存HELLO、GET_VERSION、OPEN、CLOSE、PURGE、GET_STATUSに、`ENUM_CHANNELS=7`、`GET_CHANNEL_INFO=8`、`SET_CHANNEL=9`を追加しています。native structやpaddingはwireへ出しません。

channel list payloadはcountの後にspace/channel（各u32）、SID/TSID（u16）、remote key/physical channel（u8）、center frequency Hz（u64）、virtual signal floatのIEEE-754 bits（u32）、UTF-8名length（u16）とUTF-8 bytesを並べます。BonDriverは一覧をserverから取得し、UTF-8からWindows UTF-16へ変換して`EnumChannelName`へ返します。Space 0だけが有効です。

Control接続ごとにclient IDとselected channelを持ち、OPEN順に次のTS Pipe接続と関連付けます。`SET_CHANNEL`は該当sessionだけを変更します。BonDriverは成功時にring bufferとpacket resync pendingを破棄します。複数ClientSessionは別channelを同時受信でき、一方の切替は他方へ影響しません。Control/TSの関連付けは同一processがOPEN直後にTS Pipeを開く通常BonDriver手順を前提とするFIFO方式です。暗号学的session tokenによる任意順接続は未実装です。

### ID管理

`IdManager`はONID、NID、TSID、SID、download data ID、logo IDをまとめます。衝突scopeはONID+TSID、TSID+SID、logo有効局のdownload data IDです。manualは衝突を設定エラーにし、autoはchannel名のFNV-1a hashからPC内仮想局向け候補を決定的に生成し、import済みIDと先に割り当てたauto候補を避けます。同一名・同一既存ID集合なら再起動後も同じ結果です。候補値を公式予約値とは扱いません。

確認済み形式として単純なJSON配列の`original_network_id`、`transport_stream_id`、`service_id`、`download_data_id`をimportします。TVTest固有ファイルは版やpluginによる差があるため推測parseせず、未対応です。例は`config/existing-ids-example.json`です。

### 地デジSIと仮想RF

NIT TS loopへservice list descriptor（tag `0x41`）とTS information descriptor（tag `0xCD`）を追加しました。service listのSID/typeはPAT/SDTと共有し、TS information descriptorの先頭8 bitへ1～12の`remote_control_key_id`、6-bit lengthの後へARIB符号化したTS名を格納します。transmission type loopはMilestone 6の単一service仮想TSでは0です。

`RegionManager`はcountry `JPN`、timezone `Asia/Tokyo`と47都道府県名をvalidationします。都道府県一覧は行政名称presetであり、ARIB area codeではありません。TR-B14はterrestrial delivery descriptorのarea codeにEWS 52地域コードを使うと規定しますが、行政都道府県との完全な対応をこの実装で推測していないため、prefecture/areaは内部metadataのままです。

`physical_channel`と整数`center_frequency_hz`も仮想metadataで、BonDriver channel indexとは別です。既定`rf.mode`相当はvirtualです。実RF、ISDB-T変調、guard interval、transmission modeが存在しないためterrestrial delivery system descriptor（tag `0xFA`）を送っていません。周波数のmanual設定はprotocol/list表示に反映されます。物理ch自動換算とbroadcast-compatible RF metadata modeは未実装です。虚偽の物理条件をNITへ送らない判断です。

### 実行と確認

```powershell
VirtualBroadcaster.exe --config config/multi-channel-example.toml --list-channels
VirtualBroadcaster.exe --config config/multi-channel-example.toml
```

OBS出力を12340、12341、12342へ分け、TVTest A/Bで同じBonDriverを選択して別channelを選びます。各debug TSは`inspect-ts.ps1`でPAT/PMT/NIT/SDT/EIT/TOT/CDTを検査できます。signal levelは設定された仮想値でありRF受信強度ではありません。

参照した仕様はARIB STD-B10 Version 5.13-E1 §6.2.31とFigure 6.2-49、ARIB TR-B14 Version 2.8-E2 Fascicle 1/3のNIT、service list、TS information、remote control key、EWS area code運用です。ARIB公式サイト上の現行STD-B10は5.14、TR-B14は6.12ですが、公開英訳でfield syntaxを照合しました。

BITはTR-B14でCDTの取得判断・SI伝送パラメータに関係することを再確認しました。ただし受信機運用値を推測したBITは生成せず、Milestone 5と同じ既知制限として残します。TVTest実機でのロゴ表示・channel scanは未確認です。Milestone 7のBML/DSM-CCは実装していません。
