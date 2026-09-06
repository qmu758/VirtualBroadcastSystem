#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include "virtual_broadcast/broadcaster/si.hpp"
#include "virtual_broadcast/broadcaster/channel.hpp"

namespace vb {
struct RuntimeConfig {
  bool file_enabled=true;
  bool pipe_enabled=true;
  std::filesystem::path file_path=L"output/debug.ts";
  std::size_t max_file_size_mb=50;
  std::wstring control_pipe=L"VirtualBroadcastControl";
  std::wstring ts_pipe=L"VirtualBroadcastTS";
  std::size_t buffer_size_mb=16;
  std::wstring tuner_name=L"Virtual Broadcast Tuner";
  float signal_level=30.0F;
  SiSettings si;
  std::vector<VirtualChannel> channels;
  std::filesystem::path existing_ids_file;
};
RuntimeConfig LoadRuntimeConfig(const std::filesystem::path& path);
std::wstring FullPipeName(const std::wstring& name);
}
