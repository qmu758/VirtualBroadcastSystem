#include "virtual_broadcast/broadcaster/ts_muxer.hpp"
#include "virtual_broadcast/common/log.hpp"
#include <algorithm>

namespace vb {
namespace { SiSettings MakeSi(const TsMuxer::Settings&s){auto si=s.si;si.transport_stream_id=s.transport_stream_id;si.service_id=s.service_id;si.pat_interval=s.psi_interval;si.pmt_interval=s.psi_interval;return si;} }
TsMuxer::TsMuxer(Settings settings):settings_(settings),psi_si_generator_(MakeSi(settings_)) {}

void TsMuxer::InspectPsi(const TsPacket& packet) {
  const auto pid=PacketPid(packet);
  if (pid==0) {
    for (const auto& section:pat_assembler_.Push(packet)) {
      if (const auto program=ParseFirstPatProgram(section)) {
        input_program_.service_id=program->first; input_program_.pmt_pid=program->second;
      }
    }
  } else if (pid==input_program_.pmt_pid) {
    for (const auto& section:pmt_assembler_.Push(packet)) {
      if (auto p=ParsePmt(section)) { p->pmt_pid=input_program_.pmt_pid; input_program_=*p; }
    }
  }
}

bool TsMuxer::TrackInputContinuity(const TsPacket& input) {
  const auto pid=PacketPid(input);const auto afc=(input[3]>>4)&3;
  if((afc&1)==0)return false;
  const bool declared_discontinuity=(afc&2)&&input[4]>0&&(input[5]&0x80);
  const auto cc=static_cast<std::uint8_t>(input[3]&0x0f);
  const bool gap=input_seen_[pid]&&!declared_discontinuity&&cc!=input_expected_[pid];
  input_seen_[pid]=true;input_expected_[pid]=static_cast<std::uint8_t>((cc+1)&0x0f);
  if(gap){++input_continuity_errors_;if(input_continuity_errors_==1||input_continuity_errors_%100==0)Log("VirtualBroadcaster","OBS input continuity loss count="+std::to_string(input_continuity_errors_)+" PID="+std::to_string(pid));}
  return gap;
}

TsPacket TsMuxer::Remap(const TsPacket& input,std::uint16_t output_pid,bool input_gap) {
  TsPacket output=input;
  output[1]=static_cast<std::uint8_t>((output[1]&0xe0)|((output_pid>>8)&0x1f));
  output[2]=static_cast<std::uint8_t>(output_pid);
  if (((output[3]>>4)&1)!=0) {if(input_gap)++continuity_[output_pid];output[3]=static_cast<std::uint8_t>((output[3]&0xf0)|(continuity_[output_pid]++&0x0f));}
  return output;
}

std::vector<TsPacket> TsMuxer::Process(const TsPacket& input,std::chrono::steady_clock::time_point now) {
  InspectPsi(input); std::vector<TsPacket> out;
  if (!ready()) return out;
  out=psi_si_generator_.GenerateDue(input_program_,settings_.pmt_pid,settings_.video_pid,settings_.audio_pid,now);
  const auto pid=PacketPid(input);
  if (pid==input_program_.video->pid) out.push_back(Remap(input,settings_.video_pid,TrackInputContinuity(input)));
  else if (pid==input_program_.audio->pid) out.push_back(Remap(input,settings_.audio_pid,TrackInputContinuity(input)));
  return out;
}
}
