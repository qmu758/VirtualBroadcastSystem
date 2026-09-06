#include "virtual_broadcast/common/ts.hpp"

namespace vb {
std::uint32_t MpegCrc32(std::span<const std::uint8_t> data) {
  std::uint32_t crc = 0xffffffffU;
  for (const auto byte : data) {
    crc ^= static_cast<std::uint32_t>(byte) << 24;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc & 0x80000000U) ? (crc << 1) ^ 0x04c11db7U : crc << 1;
  }
  return crc;
}
bool HasValidMpegCrc(std::span<const std::uint8_t> section) {
  return section.size() >= 4 && MpegCrc32(section) == 0;
}
}

