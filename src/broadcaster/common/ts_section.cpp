#include "virtual_broadcast/common/ts.hpp"
#include <algorithm>
#include <cstring>

namespace vb {
std::uint16_t PacketPid(const TsPacket& p) { return static_cast<std::uint16_t>(((p[1] & 0x1f) << 8) | p[2]); }
bool PayloadUnitStart(const TsPacket& p) { return (p[1] & 0x40) != 0; }

std::span<const std::uint8_t> PacketPayload(const TsPacket& p) {
  const auto control = (p[3] >> 4) & 3;
  if (control == 0 || control == 2) return {};
  std::size_t offset = 4;
  if (control == 3) {
    offset += 1 + p[4];
    if (offset > p.size()) return {};
  }
  return {p.data() + offset, p.size() - offset};
}

std::vector<std::vector<std::uint8_t>> SectionAssembler::Push(const TsPacket& packet) {
  std::vector<std::vector<std::uint8_t>> complete;
  auto payload = PacketPayload(packet);
  if (payload.empty()) return complete;
  if (PayloadUnitStart(packet)) {
    const auto pointer = payload[0];
    if (1U + pointer > payload.size()) { bytes_.clear(); expected_ = 0; return complete; }
    payload = payload.subspan(1 + pointer);
    bytes_.clear(); expected_ = 0;
  }
  while (!payload.empty()) {
    if (bytes_.empty() && payload[0] == 0xff) break;
    if (!expected_ && bytes_.size() + payload.size() >= 3) {
      while (bytes_.size() < 3) { bytes_.push_back(payload[0]); payload = payload.subspan(1); }
      expected_ = 3 + (((bytes_[1] & 0x0f) << 8) | bytes_[2]);
      if (expected_ < 4 || expected_ > 4096) { bytes_.clear(); expected_ = 0; break; }
    }
    if (!expected_) {
      bytes_.insert(bytes_.end(), payload.begin(), payload.end());
      break;
    }
    const auto take = std::min(payload.size(), expected_ - bytes_.size());
    bytes_.insert(bytes_.end(), payload.begin(), payload.begin() + take);
    payload = payload.subspan(take);
    if (bytes_.size() == expected_) {
      complete.push_back(std::move(bytes_));
      bytes_.clear(); expected_ = 0;
    }
  }
  return complete;
}

std::vector<TsPacket> SectionPacketizer::Packetize(std::uint16_t pid,
                                                    std::span<const std::uint8_t> section) {
  std::vector<TsPacket> result;
  std::size_t offset = 0;
  bool first = true;
  while (offset < section.size()) {
    TsPacket packet{};
    packet.fill(0xff);
    packet[0] = 0x47;
    packet[1] = static_cast<std::uint8_t>(((pid >> 8) & 0x1f) | (first ? 0x40 : 0));
    packet[2] = static_cast<std::uint8_t>(pid);
    packet[3] = static_cast<std::uint8_t>(0x10 | (continuity_[pid]++ & 0x0f));
    std::size_t destination = 4;
    if (first) packet[destination++] = 0;
    const auto count = std::min(section.size() - offset, packet.size() - destination);
    std::memcpy(packet.data() + destination, section.data() + offset, count);
    offset += count;
    first = false;
    result.push_back(packet);
  }
  return result;
}
}

