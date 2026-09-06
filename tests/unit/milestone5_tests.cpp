#include "virtual_broadcast/broadcaster/logo.hpp"
#include "virtual_broadcast/broadcaster/si.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>

namespace {
std::uint16_t U16(const std::vector<std::uint8_t>& b,std::size_t p){return std::uint16_t((b[p]<<8)|b[p+1]);}
std::uint32_t U32(const std::vector<std::uint8_t>& b,std::size_t p){return(std::uint32_t(b[p])<<24)|(std::uint32_t(b[p+1])<<16)|(std::uint32_t(b[p+2])<<8)|b[p+3];}
vb::LogoSettings Settings(){vb::LogoSettings s;s.enabled=true;s.source=std::filesystem::path(VB_SOURCE_DIR)/"config/sample-logo.png";s.logo_type=5;s.logo_id=1;s.logo_version=0x456;s.download_data_id=0x8001;s.cdt_version=31;return s;}
}

int main(){
  auto settings=Settings();
  const auto module=vb::LogoEncoder::Encode(settings);
  assert(module.dimensions.width==64&&module.dimensions.height==36);
  assert(module.png.size()>33&&module.png[0]==137&&module.png[1]=='P'&&module.png[2]=='N'&&module.png[3]=='G');
  std::set<std::string> chunks;for(std::size_t p=8;p+12<=module.png.size();){const auto n=U32(module.png,p);const std::string type(reinterpret_cast<const char*>(module.png.data()+p+4),4);chunks.insert(type);p+=12+n;}
  assert(chunks==std::set<std::string>({"IDAT","IEND","IHDR"}));
  assert(U32(module.png,16)==64&&U32(module.png,20)==36&&module.png[24]==8&&module.png[25]==3&&module.png[26]==0&&module.png[27]==0&&module.png[28]==0);
  assert(module.bytes[0]==5&&((module.bytes[1]&1)<<8|module.bytes[2])==1);
  assert(((module.bytes[3]&15)<<8|module.bytes[4])==0x456);
  assert(U16(module.bytes,5)==module.png.size()&&module.bytes.size()==module.png.size()+7);

  const auto descriptor=vb::LogoTransmissionDescriptorWriter::Write(settings);
  assert(descriptor.size()==9&&descriptor[0]==vb::kDescriptorLogoTransmission&&descriptor[1]==7&&descriptor[2]==1);
  assert(((descriptor[3]&1)<<8|descriptor[4])==settings.logo_id);
  assert(((descriptor[5]&15)<<8|descriptor[6])==settings.logo_version&&U16(descriptor,7)==settings.download_data_id);

  constexpr std::uint16_t onid=32760;const auto sections=vb::CdtGenerator::Generate(module,onid);assert(sections.size()==6);
  for(std::size_t i=0;i<sections.size();++i){const auto& s=sections[i];assert(s[0]==vb::kTableIdCdt);assert((((s[1]&15)<<8)|s[2])==s.size()-3);assert(vb::HasValidMpegCrc(s));assert(U16(s,3)==settings.download_data_id);assert(((s[5]>>1)&31)==settings.cdt_version);assert(s[6]==i&&s[7]==5&&U16(s,8)==onid&&s[10]==vb::kCdtDataTypeLogo);assert(s[13]==i);}
  const auto& selected=sections[5];assert(((selected[14]&1)<<8|selected[15])==settings.logo_id);assert(((selected[16]&15)<<8|selected[17])==settings.logo_version);assert(U16(selected,18)==module.png.size());assert(selected.size()==20+module.png.size()+4);

  vb::SiSettings si;si.logo=settings;si.original_network_id=onid;vb::AribStringEncoder encoder;const auto sdt=vb::SdtGenerator::Generate(si,encoder);const auto it=std::find(sdt.begin(),sdt.end(),vb::kDescriptorLogoTransmission);assert(it!=sdt.end());const std::size_t p=std::size_t(it-sdt.begin());assert(std::equal(descriptor.begin(),descriptor.end(),sdt.begin()+p));
  assert(((sdt[p+3]&1)<<8|sdt[p+4])==((selected[14]&1)<<8|selected[15]));assert(((sdt[p+5]&15)<<8|sdt[p+6])==((selected[16]&15)<<8|selected[17]));assert(U16(sdt,p+7)==U16(selected,3));
  si.logo.enabled=false;const auto no_logo=vb::SdtGenerator::Generate(si,encoder);assert(std::find(no_logo.begin(),no_logo.end(),vb::kDescriptorLogoTransmission)==no_logo.end());assert(!vb::LogoManager(si.logo).enabled());

  vb::SectionPacketizer packetizer;const auto packets=packetizer.Packetize(vb::kPidCdt,selected);assert(packets.size()>1);for(const auto& packet:packets)assert(packet[0]==0x47&&vb::PacketPid(packet)==vb::kPidCdt);assert((packets[0][1]&0x40)&&packets[0][4]==0);const auto again=packetizer.Packetize(vb::kPidCdt,selected);assert((again[0][3]&15)==((packets.back()[3]+1)&15));
  vb::CdtVersionTracker version(31);version.ContentChanged();assert(version.version()==0);version.ContentChanged();assert(version.version()==1);
  vb::SiSettings sched;sched.cdt_interval=std::chrono::milliseconds(30000);vb::SiScheduler scheduler(sched);const auto start=std::chrono::steady_clock::now();assert(scheduler.Due(vb::SiTable::Cdt,start));assert(!scheduler.Due(vb::SiTable::Cdt,start+std::chrono::milliseconds(29999)));assert(scheduler.Due(vb::SiTable::Cdt,start+std::chrono::milliseconds(30000)));

  bool failed=false;try{auto x=settings;x.logo_id=512;vb::LogoEncoder::Encode(x);}catch(const std::invalid_argument&){failed=true;}assert(failed);
  failed=false;try{auto x=settings;x.source=std::filesystem::path(VB_SOURCE_DIR)/"missing.png";vb::LogoEncoder::Encode(x);}catch(const std::runtime_error&){failed=true;}assert(failed);
  const auto bad=std::filesystem::temp_directory_path()/"vb-invalid-logo.png";{std::ofstream f(bad,std::ios::binary);f<<"not png";}failed=false;try{auto x=settings;x.source=bad;vb::LogoEncoder::Encode(x);}catch(const std::runtime_error&){failed=true;}std::filesystem::remove(bad);assert(failed);
  failed=false;try{auto x=settings;x.auto_resize=false;vb::LogoEncoder::Encode(x);}catch(const std::runtime_error&){failed=true;}assert(failed);
  return 0;
}
