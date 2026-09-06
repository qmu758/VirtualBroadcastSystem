#include "virtual_broadcast/bondriver/ts_ring_buffer.hpp"
#include <algorithm>
#include <cstring>

namespace vb {
TsRingBuffer::TsRingBuffer(std::size_t bytes)
    : packets_(std::max<std::size_t>(1, bytes / kTsPacketSize)) {}
std::size_t TsRingBuffer::Write(std::span<const TsPacket> input) {
  std::lock_guard lock(mutex_);
  std::size_t dropped = 0;
  for (const auto &p : input) {
    if (size_ == packets_.size()) {
      head_ = (head_ + 1) % packets_.size();
      --size_;
      ++overflow_;
      ++dropped;
    }
    packets_[(head_ + size_) % packets_.size()] = p;
    ++size_;
    maximum_fill_ = std::max(maximum_fill_, size_);
  }
  ready_.notify_all();
  return dropped;
}
std::size_t TsRingBuffer::Read(std::span<std::uint8_t> out) {
  std::lock_guard lock(mutex_);
  const auto count = std::min(size_, out.size() / kTsPacketSize);
  for (std::size_t i = 0; i < count; ++i)
    std::memcpy(out.data() + i * kTsPacketSize,
                packets_[(head_ + i) % packets_.size()].data(), kTsPacketSize);
  head_ = (head_ + count) % packets_.size();
  size_ -= count;
  return count * kTsPacketSize;
}
void TsRingBuffer::Purge() {
  std::lock_guard lock(mutex_);
  head_ = size_ = 0;
}
std::size_t TsRingBuffer::PacketCount() const {
  std::lock_guard lock(mutex_);
  return size_;
}
std::uint64_t TsRingBuffer::overflow_packets() const {
  std::lock_guard lock(mutex_);
  return overflow_;
}
std::size_t TsRingBuffer::MaximumFill() const {
  std::lock_guard lock(mutex_);
  return maximum_fill_;
}
bool TsRingBuffer::Wait(std::uint32_t ms) {
  std::unique_lock lock(mutex_);
  return ready_.wait_for(lock, std::chrono::milliseconds(ms),
                         [this] { return size_ != 0; });
}
} // namespace vb
