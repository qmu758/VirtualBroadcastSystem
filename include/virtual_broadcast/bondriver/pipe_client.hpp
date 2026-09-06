#pragma once
#include "virtual_broadcast/bondriver/ts_ring_buffer.hpp"
#include "virtual_broadcast/common/pipe_protocol.hpp"
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vb {
class PipeClient {
public:
  PipeClient(std::wstring control, std::wstring ts, TsRingBuffer &buffer);
  ~PipeClient();
  bool Open();
  void Close();
  bool SendControl(pipe::Message message,
                   std::span<const std::uint8_t> request = {},
                   std::vector<std::uint8_t> *response = nullptr);
  bool EnumerateChannels(std::vector<pipe::ChannelInfo> &channels);
  bool SetChannel(std::uint32_t channel);
  bool connected() const { return connected_; }
  std::uint64_t reconnect_count() const { return reconnect_count_; }

private:
  bool ConnectControl();
  bool ConnectTs();
  void ReceiveLoop();
  void Consume(std::span<const std::uint8_t> bytes);
  std::wstring control_name_, ts_name_;
  TsRingBuffer &buffer_;
  HANDLE control_ = INVALID_HANDLE_VALUE, ts_ = INVALID_HANDLE_VALUE;
  std::atomic_bool stop_ = false, connected_ = false;
  std::atomic_uint64_t reconnect_count_ = 0;
  std::atomic_uint32_t selected_channel_ = 0;
  bool first_packet_ = false;
  std::thread thread_;
  std::vector<std::uint8_t> pending_;
  std::uint32_t request_id_ = 0;
  std::mutex control_mutex_, stream_mutex_;
};
} // namespace vb
