#include "virtual_broadcast/broadcaster/psi.hpp"
#include <stdexcept>

namespace vb {
namespace {
void Put16(std::vector<std::uint8_t>& b, std::uint16_t v) { b.push_back(v >> 8); b.push_back(v); }
void AddCrc(std::vector<std::uint8_t>& b) { const auto c=MpegCrc32(b); b.push_back(c>>24); b.push_back(c>>16); b.push_back(c>>8); b.push_back(c); }
bool Video(std::uint8_t t) { return t==0x01||t==0x02||t==0x10||t==0x1b||t==0x24; }
bool Audio(std::uint8_t t) { return t==0x03||t==0x04||t==0x0f||t==0x11||t==0x81; }
}
std::vector<std::uint8_t> PmtGenerator::Generate(std::uint16_t sid, std::uint16_t pcr,
                                                 std::span<const ElementaryStream> streams,
                                                 std::uint8_t version) {
  std::size_t es_bytes=0;for(const auto&es:streams)es_bytes+=5+es.descriptors.size();
  if(es_bytes>0xff0)throw std::runtime_error("PMT ES descriptors too large");
  const auto length = static_cast<std::uint16_t>(13 + es_bytes);
  std::vector<std::uint8_t> b{0x02, static_cast<std::uint8_t>(0xb0 | (length >> 8)), static_cast<std::uint8_t>(length)};
  Put16(b,sid); b.push_back(static_cast<std::uint8_t>(0xc1|((version&0x1f)<<1))); b.push_back(0); b.push_back(0);
  b.push_back(static_cast<std::uint8_t>(0xe0|(pcr>>8))); b.push_back(pcr); b.push_back(0xf0); b.push_back(0);
  for (const auto& es:streams) { const auto n=es.descriptors.size();b.push_back(es.stream_type); b.push_back(static_cast<std::uint8_t>(0xe0|(es.pid>>8))); b.push_back(es.pid); b.push_back(static_cast<std::uint8_t>(0xf0|(n>>8))); b.push_back(static_cast<std::uint8_t>(n));b.insert(b.end(),es.descriptors.begin(),es.descriptors.end()); }
  AddCrc(b); return b;
}

std::optional<InputProgram> ParsePmt(std::span<const std::uint8_t> s) {
  if (s.size()<16 || s[0]!=0x02 || !HasValidMpegCrc(s)) return std::nullopt;
  InputProgram p; p.service_id=(s[3]<<8)|s[4]; p.pcr_pid=((s[8]&0x1f)<<8)|s[9];
  std::size_t pos=12+(((s[10]&0x0f)<<8)|s[11]);
  while (pos+5<=s.size()-4) {
    ElementaryStream es{s[pos],static_cast<std::uint16_t>(((s[pos+1]&0x1f)<<8)|s[pos+2])};
    if (!p.video && Video(es.stream_type)) p.video=es;
    if (!p.audio && Audio(es.stream_type)) p.audio=es;
    pos += 5 + (((s[pos+3]&0x0f)<<8)|s[pos+4]);
  }
  return p;
}
}
