#include "virtual_broadcast/broadcaster/virtual_broadcaster.hpp"
#include "virtual_broadcast/broadcaster/ts_output_router.hpp"
#include "virtual_broadcast/common/log.hpp"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace {
std::wstring Utf8Wide(const std::string &s) {
  if (s.empty())
    return {};
  const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                    static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(n, 0);
  if (n)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                        static_cast<int>(s.size()), w.data(), n);
  return w;
}
} // namespace

namespace vb {
int VirtualBroadcaster::Run(std::uint16_t port,
                            const std::filesystem::path &output,
                            std::atomic_bool &running) {
  RuntimeConfig config;
  config.file_path = output;
  return Run(port, config, running);
}
int VirtualBroadcaster::Run(std::uint16_t port, const RuntimeConfig &config,
                            std::atomic_bool &running) {
  if (config.channels.size() > 1) {
    std::vector<pipe::ChannelInfo> infos;
    for (const auto &ch : config.channels)
      infos.push_back({0, ch.channel_index, ch.ids.service_id,
                       ch.ids.transport_stream_id, ch.remote_control_key_id,
                       ch.physical_channel, ch.center_frequency_hz,
                       ch.signal_level, ch.name});
    NamedPipeServer server(config.control_pipe, config.ts_pipe,
                           std::move(infos));
    if (config.pipe_enabled && !server.Start())
      return 1;
    std::atomic_bool failed = false;
    std::vector<std::thread> workers;
    for (const auto &channel : config.channels)
      workers.emplace_back([&, channel] {
        std::wstring error;
        ObsInput input;
        if (!input.Open(channel.udp_port, error)) {
          std::wcerr << error << L"\n";
          failed = true;
          running = false;
          return;
        }
        std::ofstream file;
        std::uint64_t file_bytes = 0;
        const auto max_file_bytes =
            (static_cast<std::uint64_t>(config.max_file_size_mb) * 1024 * 1024 /
             kTsPacketSize) *
            kTsPacketSize;
        bool file_limit_logged = false;
        if (config.file_enabled) {
          if (channel.debug_path.has_parent_path())
            std::filesystem::create_directories(
                channel.debug_path.parent_path());
          file.open(channel.debug_path, std::ios::binary | std::ios::trunc);
        }
        TsMuxer::Settings ms;
        ms.transport_stream_id = channel.si.transport_stream_id;
        ms.service_id = channel.si.service_id;
        ms.psi_interval = channel.si.pat_interval;
        ms.si = channel.si;
        TsMuxer muxer(ms);
        std::vector<std::uint8_t> chunk;
        chunk.reserve(kTsPacketSize * 256);
        while (running) {
          error.clear();
          const auto received = input.Receive(error);
          if (!error.empty()) {
            failed = true;
            running = false;
            break;
          }
          for (const auto &p : received) {
            const auto generated =
                muxer.Process(p, std::chrono::steady_clock::now());
            for (const auto &out : generated) {
              if (file && file_bytes + out.size() <= max_file_bytes) {
                file.write(reinterpret_cast<const char *>(out.data()),
                           out.size());
                file_bytes += out.size();
              } else if (file && !file_limit_logged) {
                file_limit_logged = true;
                file.flush();
                Log("VirtualBroadcaster",
                    "Channel " + std::to_string(channel.channel_index) +
                        " debug TS size limit reached; file output paused");
              }
              chunk.insert(chunk.end(), out.begin(), out.end());
              if (chunk.size() >= kTsPacketSize * 256) {
                if (config.pipe_enabled)
                  server.Broadcast(channel.channel_index, chunk);
                chunk.clear();
              }
            }
          }
        }
        if (!chunk.empty() && config.pipe_enabled)
          server.Broadcast(channel.channel_index, chunk);
      });
    for (auto &t : workers)
      t.join();
    server.Stop();
    return failed ? 1 : 0;
  }
  std::wstring error;
  ObsInput input;
  if (!input.Open(port, error)) {
    std::wcerr << error << L"\n";
    return 1;
  }
  TsOutputRouter output;
  if (!output.Open(config)) {
    std::wcerr << L"Cannot open configured TS outputs\n";
    return 1;
  }
  TsMuxer::Settings mux_settings;
  mux_settings.transport_stream_id = config.si.transport_stream_id;
  mux_settings.service_id = config.si.service_id;
  mux_settings.psi_interval = config.si.pat_interval;
  mux_settings.si = config.si;
  TsMuxer muxer(mux_settings);
  bool announced = false;
  std::uint64_t packets = 0;
  const auto &si = config.si;
  std::wcout << L"VirtualBroadcaster Milestone 7\nService Name: "
             << Utf8Wide(si.service_name) << L"\nProvider Name: "
             << Utf8Wide(si.provider_name) << L"\n"
             << L"ONID: " << si.original_network_id << L", NID: "
             << si.network_id << L", TSID: " << si.transport_stream_id
             << L", SID: " << si.service_id << L"\n"
             << L"PIDs PAT=0x0000 PMT=0x0100 NIT=0x0010 SDT=0x0011 EIT=0x0012 "
                L"TOT=0x0014 CDT=0x0029 Video=0x0101 Audio=0x0110\n"
             << L"Clock Source: System, Country: "
             << std::wstring(si.country_code.begin(), si.country_code.end())
             << L", Local offset: " << si.local_offset_minutes << L" minutes\n"
             << L"Intervals ms PAT=" << si.pat_interval.count() << L" PMT="
             << si.pmt_interval.count() << L" NIT=" << si.nit_interval.count()
             << L" SDT=" << si.sdt_interval.count() << L" TOT="
             << si.tot_interval.count() << L" EIT p/f="
             << si.eit_pf_interval.count() << L" EIT schedule="
             << si.eit_schedule_interval.count() << L" CDT="
             << si.cdt_interval.count() << L"\n"
             << L"EPG enabled: " << (si.epg.enabled ? L"true" : L"false")
             << L", mode: "
             << (si.epg.mode == EpgMode::FixedInterval ? L"FixedInterval"
                                                       : L"Schedule")
             << L"\n"
             << L"EPG interval minutes: " << si.epg.interval_minutes
             << L", anchor minutes: " << si.epg.fixed_anchor_minutes
             << L", schedule days: " << si.epg.schedule_days
             << L", configured events: " << si.epg.events.size() << L"\n"
             << L"AribStringEncoder: CP932 to ARIB 8-unit JIS row/cell, "
                L"replacement enabled\n"
             << L"Logo: " << (si.logo.enabled ? L"enabled" : L"disabled")
             << L", type=" << unsigned(si.logo.logo_type) << L", id="
             << si.logo.logo_id << L", logo version=" << si.logo.logo_version
             << L", download data id=" << si.logo.download_data_id << L"\n"
             << L"RemoteControlKeyID: 8 (M6)\nRegion: Ehime / Matsuyama / "
                L"Asia/Tokyo (M3/M6)\n"
             << L"Virtual physical channel: 20, frequency: 515.142857 MHz (M6 "
                L"metadata only)\n"
             << L"EPG: enabled, TOT: enabled, CDT: "
             << (si.logo.enabled ? L"enabled" : L"disabled")
             << L", DataCarousel: "
             << (si.data_broadcast.enabled ? L"enabled" : L"disabled")
             << L"\nBML: A-profile 1.0, DSM-CC PID=0x" << std::hex
             << si.data_broadcast.pid << std::dec
             << L", entry=" << Utf8Wide(si.data_broadcast.entry)
             << L", block=" << si.data_broadcast.block_size << L" bytes\n"
             << L"OBS input: udp://127.0.0.1:" << port << L"\nDebugOutput: "
             << (config.file_enabled ? config.file_path.wstring() : L"disabled")
             << L" (max " << config.max_file_size_mb << L" MiB)"
             << L"\nTS Pipe: "
             << (config.pipe_enabled ? FullPipeName(config.ts_pipe)
                                     : L"disabled")
             << L"\n";
  Log("VirtualBroadcaster", "Started; TS output router active");
  while (running) {
    error.clear();
    const auto received = input.Receive(error);
    if (!error.empty()) {
      std::wcerr << error << L"\n";
      return 1;
    }
    for (const auto &packet : received) {
      const auto generated =
          muxer.Process(packet, std::chrono::steady_clock::now());
      if (!announced && muxer.ready()) {
        const auto &p = muxer.input_program();
        std::wcout << L"OBS program detected: video PID=" << p.video->pid
                   << L", audio PID=" << p.audio->pid << L", PCR PID="
                   << p.pcr_pid << L"\n";
        announced = true;
      }
      output.Write(generated);
      packets += generated.size();
    }
  }
  output.Close();
  Log("VirtualBroadcaster", "Stopped");
  std::wcout << L"Stopped. Wrote " << packets << L" TS packets.\n";
  return 0;
}
} // namespace vb
