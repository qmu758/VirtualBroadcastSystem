#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace vb {
inline constexpr std::size_t kTsPacketSize = 188;
using TsPacket = std::array<std::uint8_t, kTsPacketSize>;

std::uint32_t MpegCrc32(std::span<const std::uint8_t> data);
bool HasValidMpegCrc(std::span<const std::uint8_t> section);
std::uint16_t PacketPid(const TsPacket& packet);
bool PayloadUnitStart(const TsPacket& packet);
std::span<const std::uint8_t> PacketPayload(const TsPacket& packet);

class SectionAssembler {
 public:
  std::vector<std::vector<std::uint8_t>> Push(const TsPacket& packet);
 private:
  std::vector<std::uint8_t> bytes_;
  std::size_t expected_ = 0;
};

class SectionPacketizer {
 public:
  std::vector<TsPacket> Packetize(std::uint16_t pid, std::span<const std::uint8_t> section);
 private:
  std::array<std::uint8_t, 8192> continuity_{};
};
}

