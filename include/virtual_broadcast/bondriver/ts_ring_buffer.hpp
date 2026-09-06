#pragma once
#include "virtual_broadcast/common/ts.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace vb {
class TsRingBuffer {
public:
  explicit TsRingBuffer(std::size_t capacity_bytes);
  std::size_t Write(std::span<const TsPacket> packets);
  std::size_t Read(std::span<std::uint8_t> destination);
  void Purge();
  std::size_t PacketCount() const;
  std::uint64_t overflow_packets() const;
  std::size_t Capacity() const { return packets_.size(); }
  std::size_t MaximumFill() const;
  bool Wait(std::uint32_t timeout_ms);

private:
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::vector<TsPacket> packets_;
  std::size_t head_ = 0, size_ = 0;
  std::uint64_t overflow_ = 0;
  std::size_t maximum_fill_ = 0;
};
} // namespace vb
