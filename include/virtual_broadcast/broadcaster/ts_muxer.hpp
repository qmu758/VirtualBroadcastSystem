#pragma once
#include "virtual_broadcast/broadcaster/psi.hpp"
#include "virtual_broadcast/broadcaster/si.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace vb {
class TsMuxer {
 public:
  struct Settings {
    std::uint16_t transport_stream_id = 32513;
    std::uint16_t service_id = 101;
    std::uint16_t pmt_pid = 0x0100;
    std::uint16_t video_pid = 0x0101;
    std::uint16_t audio_pid = 0x0110;
    std::chrono::milliseconds psi_interval{100};
    SiSettings si;
  };
  explicit TsMuxer(Settings settings = {});
  std::vector<TsPacket> Process(const TsPacket& input,
                                std::chrono::steady_clock::time_point now);
  const InputProgram& input_program() const { return input_program_; }
  bool ready() const { return input_program_.video && input_program_.audio; }
 private:
  void InspectPsi(const TsPacket& packet);
  bool TrackInputContinuity(const TsPacket& input);
  TsPacket Remap(const TsPacket& input, std::uint16_t output_pid,
                 bool input_gap);
  Settings settings_;
  InputProgram input_program_;
  SectionAssembler pat_assembler_;
  SectionAssembler pmt_assembler_;
  PsiSiGenerator psi_si_generator_;
  std::array<std::uint8_t, 8192> continuity_{};
  std::array<std::uint8_t, 8192> input_expected_{};
  std::array<bool, 8192> input_seen_{};
  std::uint64_t input_continuity_errors_=0;
};
}
