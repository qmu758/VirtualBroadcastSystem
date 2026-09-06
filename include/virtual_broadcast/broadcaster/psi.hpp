#pragma once
#include "virtual_broadcast/common/ts.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace vb {
struct ElementaryStream { std::uint8_t stream_type; std::uint16_t pid; std::vector<std::uint8_t> descriptors{}; };
struct InputProgram {
  std::uint16_t service_id = 0;
  std::uint16_t pmt_pid = 0x1fff;
  std::uint16_t pcr_pid = 0x1fff;
  std::optional<ElementaryStream> video;
  std::optional<ElementaryStream> audio;
};

class PatGenerator {
 public:
  static std::vector<std::uint8_t> Generate(std::uint16_t transport_stream_id,
                                             std::uint16_t service_id,
                                             std::uint16_t pmt_pid,
                                             std::uint8_t version = 0);
};
class PmtGenerator {
 public:
  static std::vector<std::uint8_t> Generate(std::uint16_t service_id,
                                             std::uint16_t pcr_pid,
                                             std::span<const ElementaryStream> streams,
                                             std::uint8_t version = 0);
};

std::optional<std::pair<std::uint16_t, std::uint16_t>> ParseFirstPatProgram(
    std::span<const std::uint8_t> section);
std::optional<InputProgram> ParsePmt(std::span<const std::uint8_t> section);
}
