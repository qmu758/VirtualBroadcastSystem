#include "virtual_broadcast/broadcaster/data_broadcast.hpp"
#include "virtual_broadcast/broadcaster/psi.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>

namespace {
std::uint16_t U16(const std::vector<std::uint8_t> &b, std::size_t p) {
  return static_cast<std::uint16_t>((b[p] << 8) | b[p + 1]);
}
std::uint32_t U32(const std::vector<std::uint8_t> &b, std::size_t p) {
  return (std::uint32_t(b[p]) << 24) | (std::uint32_t(b[p + 1]) << 16) |
         (std::uint32_t(b[p + 2]) << 8) | b[p + 3];
}
} // namespace
int main() {
  namespace fs = std::filesystem;
  using namespace vb;
  DataBroadcastSettings s;
  s.enabled = true;
  s.root = fs::path(VB_SOURCE_DIR) / "sample-data/channel0";
  s.entry = "index.bml";
  const auto a = CarouselModuleBuilder::Build(s),
             b = CarouselModuleBuilder::Build(s);
  assert(a.application.profile == "A-profile" &&
         a.application.version == "1.0");
  assert(a.application.modules.size() >= 6 &&
         a.application.modules.size() == b.application.modules.size());
  assert(a.application.modules.front().id == 0);
  for (std::size_t i = 0; i < a.application.modules.size(); ++i) {
    assert(a.application.modules[i].id == b.application.modules[i].id);
    assert(a.application.modules[i].version ==
           b.application.modules[i].version);
    assert(a.application.modules[i].path == b.application.modules[i].path);
  }
  const auto dii = DiiGenerator::Generate(a);
  assert(dii[0] == 0x3b && HasValidMpegCrc(dii));
  assert((((dii[1] & 15) << 8) | dii[2]) + 3 == dii.size());
  assert(U16(dii, 10) == 0x1002);
  assert(U32(dii, 20) == s.download_id);
  assert(U16(dii, 24) == 4066);
  const auto ddb = DdbGenerator::Generate(a);
  assert(!ddb.empty());
  std::map<std::uint16_t, std::vector<std::uint8_t>> restored;
  for (const auto &sec : ddb) {
    assert(sec[0] == 0x3c && HasValidMpegCrc(sec));
    assert(U16(sec, 10) == 0x1003);
    assert(U32(sec, 12) == s.download_id);
    const auto id = U16(sec, 20);
    restored[id].insert(restored[id].end(), sec.begin() + 26, sec.end() - 4);
  }
  for (const auto &m : a.application.modules)
    assert(restored[m.id] == m.data);
  const auto desc = DataBroadcastDescriptorWriter::PmtDescriptors();
  assert(desc ==
         std::vector<std::uint8_t>({0x52, 1, 0x40, 0xfd, 3, 0, 0x0c, 0xf7}));
  std::vector<ElementaryStream> es{
      {2, 0x101}, {15, 0x110}, {0x0d, 0x200, desc}};
  const auto pmt = PmtGenerator::Generate(101, 0x101, es, 1);
  assert(HasValidMpegCrc(pmt));
  bool found = false;
  for (std::size_t p = 12; p + 5 < pmt.size() - 4;) {
    const auto n = ((pmt[p + 3] & 15) << 8) | pmt[p + 4];
    if (pmt[p] == 0x0d) {
      found = true;
      assert((((pmt[p + 1] & 31) << 8) | pmt[p + 2]) == 0x200);
      assert(std::equal(desc.begin(), desc.end(), pmt.begin() + p + 5));
    }
    p += 5 + n;
  }
  assert(found);
  std::vector<ElementaryStream> av{{2, 0x101}, {15, 0x110}};
  const auto plain = PmtGenerator::Generate(101, 0x101, av);
  assert(std::find(plain.begin(), plain.end(), 0xfd) == plain.end());
  SectionPacketizer packetizer;
  auto packets = packetizer.Packetize(0x200, ddb.front());
  assert(packets.size() > 1);
  for (std::size_t i = 0; i < packets.size(); ++i) {
    assert(packets[i][0] == 0x47);
    assert(PacketPid(packets[i]) == 0x200);
    assert((packets[i][3] & 15) == i % 16);
  }
  bool missing = false;
  try {
    auto bad = s;
    bad.entry = "missing.bml";
    (void)CarouselModuleBuilder::Build(bad);
  } catch (...) {
    missing = true;
  }
  assert(missing);
  bool traversal = false;
  try {
    auto bad = s;
    bad.entry = "../index.bml";
    (void)CarouselModuleBuilder::Build(bad);
  } catch (...) {
    traversal = true;
  }
  assert(traversal);
  DataBroadcastSettings off;
  off.enabled = false;
  DataBroadcastManager manager(off);
  assert(!manager.enabled());
  auto s2 = s;
  s2.root = fs::path(VB_SOURCE_DIR) / "sample-data/channel1";
  s2.pid = 0x0201;
  s2.download_id = 0xf0000002;
  const auto channel_b = CarouselModuleBuilder::Build(s2);
  assert(channel_b.download_id != a.download_id);
  const auto entry_a = std::find_if(a.application.modules.begin(), a.application.modules.end(),
                                    [](const auto &m) { return m.id == 0; });
  const auto entry_b = std::find_if(channel_b.application.modules.begin(), channel_b.application.modules.end(),
                                    [](const auto &m) { return m.id == 0; });
  assert(entry_a != a.application.modules.end() && entry_b != channel_b.application.modules.end());
  assert(entry_a->data != entry_b->data);
  std::cout << "Milestone 7 DSM-CC round-trip tests passed: "
            << a.application.modules.size() << " modules, " << ddb.size()
            << " DDB sections\n";
}
