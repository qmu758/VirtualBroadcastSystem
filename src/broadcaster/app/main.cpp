#include "virtual_broadcast/broadcaster/virtual_broadcaster.hpp"
#include "virtual_broadcast/broadcaster/diagnostics.hpp"
#include <Windows.h>
#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
std::atomic_bool running = true;
void Stop(int) { running = false; }
std::wstring Wide(const std::string &s) {
  const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                    static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(n, 0);
  if (n)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                        static_cast<int>(s.size()), w.data(), n);
  return w;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  std::uint16_t port = 12340;
  bool list_channels = false, diagnostics = false, self_check = false,
       config_explicit = false;
  std::filesystem::path analyze_ts;
  std::filesystem::path config_path = L"config/example.toml";
  std::filesystem::path output_override;
  for (int i = 1; i < argc; ++i) {
    const std::wstring arg = argv[i];
    try {
      if (arg == L"--port" && i + 1 < argc)
        port = static_cast<std::uint16_t>(std::stoul(argv[++i]));
      else if (arg == L"--output" && i + 1 < argc)
        output_override = argv[++i];
      else if (arg == L"--config" && i + 1 < argc) {
        config_path = argv[++i];
        config_explicit = true;
      }
      else if (arg == L"--list-channels")
        list_channels = true;
      else if (arg == L"--diagnostics")
        diagnostics = true;
      else if (arg == L"--self-check") {
        self_check = true;
        if (i + 1 < argc && argv[i + 1][0] != L'-') config_path = argv[++i];
      } else if (arg == L"--analyze-ts" && i + 1 < argc)
        analyze_ts = argv[++i];
      else {
        std::wcerr << L"Usage: VirtualBroadcaster.exe [--port 12340] [--config "
                      L"config/example.toml] [--output output/debug.ts] "
                      L"[--diagnostics|--self-check [config]|--analyze-ts file]\n";
        return 2;
      }
    } catch (...) {
      std::wcerr << L"Invalid command line value\n";
      return 2;
    }
  }
  if (!analyze_ts.empty()) {
    try { const auto report=vb::CompatibilityDiagnostics::AnalyzeTs(analyze_ts);vb::CompatibilityDiagnostics::PrintReport(report,std::cout);return report.continuity_errors||!report.referenced_missing_pids.empty()?1:0; }
    catch(const std::exception&e){std::cerr<<"TS analysis error: "<<e.what()<<"\n";return 2;}
  }
  if (!config_explicit && !std::filesystem::exists(config_path)) {
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    const auto beside_executable =
        std::filesystem::path(executable).parent_path() / L"example.toml";
    if (std::filesystem::exists(beside_executable))
      config_path = beside_executable;
  }
  if (config_explicit && !std::filesystem::exists(config_path)) {
    std::wcerr << L"Configuration file not found: " << config_path << L"\n";
    return 2;
  }
  vb::RuntimeConfig config;
  try {
    config = vb::LoadRuntimeConfig(config_path);
  } catch (const std::exception &e) {
    std::cerr << "Configuration error: " << e.what() << "\n";
    return 2;
  }
  if (!output_override.empty()) {
    config.file_enabled = true;
    config.file_path = output_override;
  }
  if (diagnostics) {vb::CompatibilityDiagnostics::PrintChannels(config,std::cout);return 0;}
  if (self_check) {const auto issues=vb::CompatibilityDiagnostics::SelfCheck(config);bool failed=false;for(const auto&i:issues){std::cout<<(i.error?"ERROR ":"OK ")<<i.code<<": "<<i.message<<"\n";failed|=i.error;}return failed?1:0;}
  if (list_channels) {
    std::wcout << L"Space 0\n";
    for (const auto &c : config.channels)
      std::wcout << c.channel_index << L": " << Wide(c.si.service_name)
                 << L" TSID=" << c.ids.transport_stream_id << L" SID="
                 << c.ids.service_id << L" Remote="
                 << unsigned(c.remote_control_key_id) << L" Physical="
                 << unsigned(c.physical_channel) << L" Frequency="
                 << (c.center_frequency_hz / 1000000.0) << L" MHz\n";
    return 0;
  }
  std::signal(SIGINT, Stop);
  vb::VirtualBroadcaster app;
  return app.Run(port, config, running);
}
