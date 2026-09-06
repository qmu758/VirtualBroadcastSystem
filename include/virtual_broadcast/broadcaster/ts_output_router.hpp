#pragma once
#include "virtual_broadcast/broadcaster/named_pipe_server.hpp"
#include "virtual_broadcast/common/runtime_config.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <vector>

namespace vb {
class TsOutputRouter {
 public:
  bool Open(const RuntimeConfig& config); void Write(std::span<const TsPacket> packets); void Close();
 private:
  void FlushPipe(); std::ofstream file_; std::unique_ptr<NamedPipeServer> pipe_; std::vector<std::uint8_t> pipe_chunk_;
  std::uint64_t file_bytes_=0, max_file_bytes_=0;
  bool file_limit_logged_=false;
};
}
