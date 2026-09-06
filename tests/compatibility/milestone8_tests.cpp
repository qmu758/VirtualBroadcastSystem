#include "virtual_broadcast/bondriver/pipe_client.hpp"
#include "virtual_broadcast/broadcaster/diagnostics.hpp"
#include "virtual_broadcast/broadcaster/named_pipe_server.hpp"
#include "virtual_broadcast/broadcaster/si.hpp"
#include "virtual_broadcast/broadcaster/ts_output_router.hpp"
#include <cassert>
#include <fstream>
#include <thread>
class FakeClock final : public vb::IClock {
public:
  explicit FakeClock(std::chrono::sys_seconds n) : now(n) {}
  std::chrono::sys_seconds Now() const override { return now; }
  std::chrono::sys_seconds now;
};
int main() {
  using namespace std::chrono;
  const auto root = std::filesystem::path(VB_SOURCE_DIR);
  const auto config =
      vb::LoadRuntimeConfig(root / "config/multi-channel-example.toml");
  const auto issues = vb::CompatibilityDiagnostics::SelfCheck(config);
  assert(std::none_of(issues.begin(), issues.end(),
                      [](const auto &i) { return i.error; }));
  const auto suffix = std::to_wstring(GetCurrentProcessId());
  std::vector<vb::pipe::ChannelInfo> wire;
  for (const auto &c : config.channels)
    wire.push_back({0, c.channel_index, c.ids.service_id,
                    c.ids.transport_stream_id, c.remote_control_key_id,
                    c.physical_channel, c.center_frequency_hz, c.signal_level,
                    c.si.service_name});
  vb::NamedPipeServer server(L"VBM8Control" + suffix, L"VBM8Ts" + suffix, wire);
  assert(server.Start());
  std::this_thread::sleep_for(100ms);
  vb::TsRingBuffer ring(188 * 128);
  vb::TsRingBuffer ring_b(188 * 128);
  vb::PipeClient client(L"VBM8Control" + suffix, L"VBM8Ts" + suffix, ring);
  vb::PipeClient client_b(L"VBM8Control" + suffix, L"VBM8Ts" + suffix,
                          ring_b);
  assert(client.Open());
  assert(client_b.Open());
  assert(client_b.SetChannel(2));
  vb::TsPacket packet{};
  packet[0] = 0x47;
  for (int pass = 0; pass < 100; ++pass) {
    const auto channel = static_cast<std::uint32_t>(pass % 3);
    assert(client.SetChannel(channel));
    packet[2] = static_cast<std::uint8_t>(0x40 + channel);
    server.Broadcast(channel, packet);
    assert(ring.Wait(1000));
    std::array<std::uint8_t, 188> out{};
    assert(ring.Read(out) == 188 && out[2] == packet[2]);
    if (channel == 2) {
      assert(ring_b.Wait(1000));
      assert(ring_b.Read(out) == 188 && out[2] == packet[2]);
    }

    if (channel != 2) {
      vb::TsPacket packet_b{};
      packet_b[0] = 0x47;
      packet_b[2] = static_cast<std::uint8_t>(0x70 + (pass & 0x0F));
      server.Broadcast(2, packet_b);
      assert(ring_b.Wait(1000));
      assert(ring_b.Read(out) == 188 && out[2] == packet_b[2]);
    }
  }
  server.Stop();
  std::this_thread::sleep_for(300ms);
  assert(!client.connected() && !client_b.connected());
  assert(server.Start());
  for (int i = 0; i < 30 && (!client.connected() || !client_b.connected()); ++i)
    std::this_thread::sleep_for(200ms);
  assert(client.connected() && client.reconnect_count() >= 1);
  assert(client_b.connected() && client_b.reconnect_count() >= 1);
  assert(client.SetChannel(1));
  assert(client_b.SetChannel(2));
  packet[2] = 0x55;
  server.Broadcast(1, packet);
  assert(ring.Wait(1000));
  std::array<std::uint8_t, 188> resumed{};
  assert(ring.Read(resumed) == 188 && resumed[2] == 0x55);
  packet[2] = 0x66;
  server.Broadcast(2, packet);
  assert(ring_b.Wait(1000));
  assert(ring_b.Read(resumed) == 188 && resumed[2] == 0x66);
  client.Close();
  client_b.Close();
  server.Stop();
  auto clock =
      std::make_shared<FakeClock>(sys_days{year{2026} / 12 / 31} + hours{14});
  auto si = config.channels[0].si;
  vb::PsiSiGenerator generator(si, clock);
  vb::InputProgram input;
  input.video = vb::ElementaryStream{2, 0x200};
  input.audio = vb::ElementaryStream{15, 0x201};
  const auto steady = steady_clock::now();
  std::uint64_t eit_packets = 0;
  for (int h = 0; h <= 24 * 7; ++h) {
    clock->now = sys_days{year{2026} / 12 / 31} + hours{14 + h};
    const auto packets =
        generator.GenerateDue(input, 0x100, 0x101, 0x110, steady + hours{h});
    for (const auto &p : packets)
      if (vb::PacketPid(p) == vb::kPidEit)
        ++eit_packets;
  }
  assert(eit_packets > 0);
  const auto jst =
      vb::MjdTimeEncoder::EncodeJst(sys_days{year{2026} / 12 / 31} + hours{15});
  assert(jst[2] == 0x00 && jst[3] == 0x00 && jst[4] == 0x00);
  const auto logo = vb::LogoManager(si.logo);
  const auto cdt1 = vb::CdtGenerator::Generate(logo.module(),
                                               si.original_network_id),
             cdt2 = vb::CdtGenerator::Generate(logo.module(),
                                               si.original_network_id);
  assert(cdt1 == cdt2);
  const auto car1 = vb::CarouselModuleBuilder::Build(si.data_broadcast),
             car2 = vb::CarouselModuleBuilder::Build(si.data_broadcast);
  assert(vb::DiiGenerator::Generate(car1) == vb::DiiGenerator::Generate(car2));
  assert(vb::DdbGenerator::Generate(car1) == vb::DdbGenerator::Generate(car2));
  assert(ring.MaximumFill() <= ring.Capacity());
  assert(ring_b.MaximumFill() <= ring_b.Capacity());

  const auto capped_path =
      std::filesystem::temp_directory_path() / L"vb-capped-debug.ts";
  vb::RuntimeConfig capped_config;
  capped_config.file_enabled = true;
  capped_config.pipe_enabled = false;
  capped_config.file_path = capped_path;
  capped_config.max_file_size_mb = 1;
  vb::TsOutputRouter capped_output;
  assert(capped_output.Open(capped_config));
  std::vector<vb::TsPacket> many_packets(6000);
  for (auto &p : many_packets)
    p[0] = 0x47;
  capped_output.Write(many_packets);
  capped_output.Close();
  const auto expected_size = (1024ULL * 1024 / 188) * 188;
  assert(std::filesystem::file_size(capped_path) == expected_size);
  std::filesystem::remove(capped_path);
}
