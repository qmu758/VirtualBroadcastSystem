#include "virtual_broadcast/broadcaster/psi.hpp"
#include "virtual_broadcast/broadcaster/ts_muxer.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <span>

namespace {
vb::TsPacket PesPacket(std::uint16_t pid,std::uint64_t pcr,std::uint8_t marker,
                       std::uint8_t continuity=0) {
  vb::TsPacket p{}; p.fill(0xff); p[0]=0x47; p[1]=0x40|((pid>>8)&0x1f); p[2]=pid; p[3]=static_cast<std::uint8_t>(0x30|(continuity&0x0f));
  p[4]=7; p[5]=0x10;
  const auto base=pcr/300; const auto ext=pcr%300;
  p[6]=base>>25; p[7]=base>>17; p[8]=base>>9; p[9]=base>>1;
  p[10]=static_cast<std::uint8_t>((base<<7)|0x7e|(ext>>8)); p[11]=static_cast<std::uint8_t>(ext);
  const std::array<std::uint8_t,14> pes{0,0,1,0xe0,0,0,0x80,0xc0,10,0x21,0,1,marker};
  std::copy(pes.begin(),pes.end(),p.begin()+12); return p;
}
const vb::TsPacket& FindPid(const std::vector<vb::TsPacket>& packets,std::uint16_t pid) {
  const auto it=std::find_if(packets.begin(),packets.end(),[pid](const auto& p){return vb::PacketPid(p)==pid;});
  assert(it!=packets.end()); return *it;
}
}

int main() {
  const auto pat=vb::PatGenerator::Generate(32513,101,0x100);
  assert(pat[0]==0x00); assert((((pat[1]&0x0f)<<8)|pat[2])==pat.size()-3); assert(vb::HasValidMpegCrc(pat));
  std::array<vb::ElementaryStream,2> streams{{{0x02,0x200},{0x0f,0x201}}};
  const auto pmt=vb::PmtGenerator::Generate(101,0x200,streams);
  assert(pmt[0]==0x02); assert((((pmt[1]&0x0f)<<8)|pmt[2])==pmt.size()-3); assert(vb::HasValidMpegCrc(pmt));

  vb::SectionPacketizer packetizer;
  const auto pat_packets=packetizer.Packetize(0,pat);
  const auto pat_again=packetizer.Packetize(0,pat);
  assert(pat_packets[0].size()==188 && pat_packets[0][0]==0x47);
  assert((pat_again[0][3]&0x0f)==((pat_packets[0][3]+1)&0x0f));

  vb::TsMuxer muxer;
  const auto now=std::chrono::steady_clock::now();
  for (const auto& packet:pat_packets) muxer.Process(packet,now);
  for (const auto& packet:packetizer.Packetize(0x100,pmt)) muxer.Process(packet,now);
  assert(muxer.ready());

  const auto video1=PesPacket(0x200,27'000'000,0x31);
  const auto video2=PesPacket(0x200,54'000'000,0x32,1);
  const auto output1=muxer.Process(video1,now);
  const auto output2=muxer.Process(video2,now+std::chrono::milliseconds(10));
  const auto& remapped1=FindPid(output1,0x101);
  const auto& remapped2=FindPid(output2,0x101);
  assert(remapped1[0]==0x47 && remapped2[0]==0x47);
  assert((remapped2[3]&0x0f)==((remapped1[3]+1)&0x0f));
  assert(std::equal(video1.begin()+4,video1.end(),remapped1.begin()+4));
  assert(std::equal(video2.begin()+4,video2.end(),remapped2.begin()+4));
  // PCR bytes and PES PTS/DTS bytes remain unchanged, so increasing input PCR stays increasing.
  assert(std::lexicographical_compare(remapped1.begin()+6,remapped1.begin()+12,
                                      remapped2.begin()+6,remapped2.begin()+12));
  assert(std::equal(video1.begin()+12,video1.begin()+26,remapped1.begin()+12));

  const auto video_gap=PesPacket(0x200,67'500'000,0x33,3);
  const auto gap_output=muxer.Process(video_gap,now+std::chrono::milliseconds(15));
  const auto& remapped_gap=FindPid(gap_output,0x101);
  assert((remapped_gap[3]&0x0f)==((remapped2[3]+2)&0x0f));

  auto audio=PesPacket(0x201,81'000'000,0x41); audio[13]=0xc0;
  const auto audio_output=muxer.Process(audio,now+std::chrono::milliseconds(20));
  assert(vb::PacketPid(FindPid(audio_output,0x110))==0x110);
  return 0;
}
