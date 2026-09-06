#include "virtual_broadcast/broadcaster/si.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <set>

namespace {
class FakeClock final:public vb::IClock { public:std::chrono::sys_seconds value;std::chrono::sys_seconds Now()const override{return value;} };
std::uint16_t U16(const std::vector<std::uint8_t>&b,std::size_t p){return static_cast<std::uint16_t>((b[p]<<8)|b[p+1]);}
}
int main(){
  vb::SiSettings s;s.service_name="松山自主放送";s.provider_name="Virtual Broadcasting";s.network_name="松山仮想放送ネットワーク";
  vb::AribStringEncoder encoder;const auto jp=encoder.Encode(s.service_name);assert(!jp.bytes.empty()&&!jp.replaced);assert(std::all_of(jp.bytes.begin(),jp.bytes.end(),[](auto c){return c<0x80;}));const std::array<std::uint8_t,3>utf8_matsu{0xe6,0x9d,0xbe};assert(std::search(jp.bytes.begin(),jp.bytes.end(),utf8_matsu.begin(),utf8_matsu.end())==jp.bytes.end());
  const auto ascii=encoder.Encode("ABC123");assert(ascii.bytes.front()==0x0e&&ascii.bytes.back()==0x0f);

  const auto sdt=vb::SdtGenerator::Generate(s,encoder);assert(sdt[0]==vb::kTableIdSdtActual);assert((((sdt[1]&15)<<8)|sdt[2])==sdt.size()-3);assert(vb::HasValidMpegCrc(sdt));assert(U16(sdt,3)==s.transport_stream_id);assert(U16(sdt,8)==s.original_network_id);assert(U16(sdt,11)==s.service_id);assert(sdt[16]==vb::kDescriptorService);const auto desc_len=sdt[17];assert(desc_len>jp.bytes.size());assert(std::search(sdt.begin()+18,sdt.end(),jp.bytes.begin(),jp.bytes.end())!=sdt.end());

  const auto nit=vb::NitGenerator::Generate(s,encoder);assert(nit[0]==vb::kTableIdNitActual);assert((((nit[1]&15)<<8)|nit[2])==nit.size()-3);assert(vb::HasValidMpegCrc(nit));assert(U16(nit,3)==s.network_id);const auto network_desc_len=((nit[8]&15)<<8)|nit[9];assert(nit[10]==vb::kDescriptorNetworkName);const auto tsloop=10+network_desc_len;assert(U16(nit,tsloop+2)==s.transport_stream_id);assert(U16(nit,tsloop+4)==s.original_network_id);

  using namespace std::chrono;const auto known=sys_days{year{2000}/1/1};assert(vb::MjdTimeEncoder::Mjd(known)==51544);const auto encoded=vb::MjdTimeEncoder::Encode(sys_seconds{known}+12h+34min+56s);assert(encoded[0]==0xc9&&encoded[1]==0x58&&encoded[2]==0x12&&encoded[3]==0x34&&encoded[4]==0x56);assert(vb::BcdEncoder::Encode(59)==0x59);assert((vb::BcdEncoder::EncodeHoursMinutes(540)==std::array<std::uint8_t,2>{0x09,0x00}));
  const auto utc1=sys_seconds{sys_days{year{2026}/8/29}}+15h;const auto jst1=vb::MjdTimeEncoder::EncodeJst(utc1);assert(U16(std::vector<std::uint8_t>(jst1.begin(),jst1.end()),0)==vb::MjdTimeEncoder::Mjd(sys_days{year{2026}/8/30}));assert(jst1[2]==0x00&&jst1[3]==0x00&&jst1[4]==0x00);
  const auto utc2=sys_seconds{sys_days{year{2026}/12/31}}+15h;const auto jst2=vb::MjdTimeEncoder::EncodeJst(utc2);assert(U16(std::vector<std::uint8_t>(jst2.begin(),jst2.end()),0)==vb::MjdTimeEncoder::Mjd(sys_days{year{2027}/1/1}));assert(jst2[2]==0x00&&jst2[3]==0x00&&jst2[4]==0x00);
  const auto jst3=vb::MjdTimeEncoder::EncodeJst(sys_seconds{sys_days{year{2026}/8/30}});assert(U16(std::vector<std::uint8_t>(jst3.begin(),jst3.end()),0)==vb::MjdTimeEncoder::Mjd(sys_days{year{2026}/8/30}));assert(jst3[2]==0x09&&jst3[3]==0x00&&jst3[4]==0x00);
  const auto before_midnight=vb::MjdTimeEncoder::EncodeJst(sys_seconds{sys_days{year{2026}/8/30}}+14h+59min+59s);const auto after_midnight=vb::MjdTimeEncoder::EncodeJst(sys_seconds{sys_days{year{2026}/8/30}}+15h);assert(before_midnight[2]==0x23&&before_midnight[3]==0x59&&before_midnight[4]==0x59);assert(U16(std::vector<std::uint8_t>(after_midnight.begin(),after_midnight.end()),0)==vb::MjdTimeEncoder::Mjd(sys_days{year{2026}/8/31}));assert(after_midnight[2]==0x00&&after_midnight[3]==0x00&&after_midnight[4]==0x00);
  auto clock=std::make_shared<FakeClock>();clock->value=utc1;vb::TotGenerator totgen(s,*clock);const auto tot1=totgen.Generate();assert(tot1[0]==vb::kTableIdTot);assert((((tot1[1]&15)<<8)|tot1[2])==tot1.size()-3);assert(vb::HasValidMpegCrc(tot1));assert(U16(tot1,3)==vb::MjdTimeEncoder::Mjd(sys_days{year{2026}/8/30}));assert(tot1[5]==0x00&&tot1[6]==0x00&&tot1[7]==0x00);assert(tot1[10]==vb::kDescriptorLocalTimeOffset&&tot1[11]==13);assert(tot1[12]=='J'&&tot1[13]=='P'&&tot1[14]=='N');assert((tot1[15]&1)==0);assert(tot1[16]==0x09&&tot1[17]==0x00);clock->value+=1h;const auto tot2=totgen.Generate();assert(!std::equal(tot1.begin()+3,tot1.begin()+8,tot2.begin()+3));

  vb::SectionPacketizer packetizer;const auto one=packetizer.Packetize(vb::kPidSdt,sdt);assert(one.size()==1&&one[0][0]==0x47&&(one[0][1]&0x40)&&one[0][4]==0);std::vector<std::uint8_t>large(400,0x55);large[0]=0x99;const auto length=large.size()-3;large[1]=0xb0|(length>>8);large[2]=length;const auto many=packetizer.Packetize(0x30,large);assert(many.size()==3);for(std::size_t i=1;i<many.size();++i)assert(!(many[i][1]&0x40));vb::SectionAssembler assembler;std::vector<std::vector<std::uint8_t>>complete;for(const auto&p:many){auto x=assembler.Push(p);complete.insert(complete.end(),x.begin(),x.end());}assert(complete.size()==1&&complete[0]==large);const auto again=packetizer.Packetize(vb::kPidSdt,sdt);assert((again[0][3]&15)==((one[0][3]+1)&15));

  vb::SiScheduler scheduler(s);const auto start=steady_clock::now();for(int i=0;i<5;++i)assert(scheduler.Due(static_cast<vb::SiTable>(i),start));assert(!scheduler.Due(vb::SiTable::Pat,start+50ms));assert(scheduler.Due(vb::SiTable::Pat,start+100ms));assert(!scheduler.Due(vb::SiTable::Nit,start+999ms));assert(scheduler.Due(vb::SiTable::Nit,start+1000ms));
  vb::InputProgram input;input.video=vb::ElementaryStream{2,0x200};input.audio=vb::ElementaryStream{15,0x201};vb::PsiSiGenerator generator(s,clock);const auto packets=generator.GenerateDue(input,0x100,0x101,0x110,start);std::set<std::uint16_t>pids;for(const auto&p:packets){assert(p.size()==188&&p[0]==0x47);pids.insert(vb::PacketPid(p));}assert(pids.contains(0)&&pids.contains(0x100)&&pids.contains(vb::kPidNit)&&pids.contains(vb::kPidSdt)&&pids.contains(vb::kPidEit)&&pids.contains(vb::kPidTot));
  return 0;
}
