#pragma once
#include "virtual_broadcast/broadcaster/obs_input.hpp"
#include "virtual_broadcast/broadcaster/ts_muxer.hpp"
#include "virtual_broadcast/common/runtime_config.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>

namespace vb {
class VirtualBroadcaster {
 public:
  int Run(std::uint16_t port, const std::filesystem::path& output, std::atomic_bool& running);
  int Run(std::uint16_t port, const RuntimeConfig& config, std::atomic_bool& running);
};
}
