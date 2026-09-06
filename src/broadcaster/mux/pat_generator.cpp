#include "virtual_broadcast/broadcaster/psi.hpp"

namespace vb {
namespace {
void Put16(std::vector<std::uint8_t>& b, std::uint16_t v) { b.push_back(v >> 8); b.push_back(v & 0xff); }
void AddCrc(std::vector<std::uint8_t>& b) { const auto c = MpegCrc32(b); b.push_back(c >> 24); b.push_back(c >> 16); b.push_back(c >> 8); b.push_back(c); }
}
std::vector<std::uint8_t> PatGenerator::Generate(std::uint16_t tsid, std::uint16_t sid,
                                                 std::uint16_t pmt_pid, std::uint8_t version) {
  std::vector<std::uint8_t> b{0x00, 0xb0, 0x0d};
  Put16(b, tsid); b.push_back(static_cast<std::uint8_t>(0xc1 | ((version & 0x1f) << 1)));
  b.push_back(0); b.push_back(0); Put16(b, sid);
  b.push_back(static_cast<std::uint8_t>(0xe0 | ((pmt_pid >> 8) & 0x1f))); b.push_back(pmt_pid & 0xff);
  AddCrc(b); return b;
}

std::optional<std::pair<std::uint16_t, std::uint16_t>> ParseFirstPatProgram(std::span<const std::uint8_t> s) {
  if (s.size() < 12 || s[0] != 0x00 || !HasValidMpegCrc(s)) return std::nullopt;
  for (std::size_t p = 8; p + 4 <= s.size() - 4; p += 4) {
    const auto sid = static_cast<std::uint16_t>((s[p] << 8) | s[p + 1]);
    if (sid) return std::pair{sid, static_cast<std::uint16_t>(((s[p + 2] & 0x1f) << 8) | s[p + 3])};
  }
  return std::nullopt;
}
}

