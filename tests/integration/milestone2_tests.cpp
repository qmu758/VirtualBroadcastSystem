#include "virtual_broadcast/bondriver/channel_controller.hpp"
#include "virtual_broadcast/bondriver/ibon_driver.hpp"
#include "virtual_broadcast/bondriver/pipe_client.hpp"
#include "virtual_broadcast/bondriver/ts_ring_buffer.hpp"
#include "virtual_broadcast/broadcaster/named_pipe_server.hpp"
#include "virtual_broadcast/common/pipe_protocol.hpp"
#include <Windows.h>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
std::vector<std::uint8_t> Bytes(std::size_t count,std::uint8_t marker) {
  std::vector<std::uint8_t>b(count*vb::kTsPacketSize);
  for(std::size_t i=0;i<count;++i){b[i*vb::kTsPacketSize]=0x47;b[i*vb::kTsPacketSize+1]=marker;}
  return b;
}
bool WaitPackets(vb::TsRingBuffer&ring,std::size_t count,int milliseconds=3000){const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);while(std::chrono::steady_clock::now()<until){if(ring.PacketCount()>=count)return true;std::this_thread::sleep_for(std::chrono::milliseconds(20));}return false;}
}

int main() {
  const auto encoded=vb::pipe::EncodeHeader({vb::pipe::kVersion,vb::pipe::Message::Open,0,42});vb::pipe::Header decoded{};
  assert(vb::pipe::DecodeHeader(encoded,decoded)&&decoded.request_id==42&&decoded.type==vb::pipe::Message::Open);

  vb::TsRingBuffer small(vb::kTsPacketSize*3);std::vector<vb::TsPacket>five(5);for(auto&p:five){p.fill(0);p[0]=0x47;}
  assert(small.Write(five)==2);assert(small.PacketCount()==3);assert(small.overflow_packets()==2);
  std::array<std::uint8_t,vb::kTsPacketSize*2>read{};assert(small.Read(read)==read.size());assert(read[0]==0x47&&read[vb::kTsPacketSize]==0x47);
  small.Purge();assert(small.PacketCount()==0);

  vb::ChannelController channels;assert(channels.Select(0,0));assert(!channels.Select(0,1));assert(!channels.Select(1,0));
  vb::TsRingBuffer absent_ring(1024*1024);vb::PipeClient absent(L"VBAbsentControlM2",L"VBAbsentTsM2",absent_ring);assert(!absent.Open());

  const std::wstring suffix=std::to_wstring(GetCurrentProcessId());const auto control=L"VBTestControl"+suffix;const auto ts=L"VBTestTs"+suffix;
  vb::NamedPipeServer server(control,ts);assert(server.Start());std::this_thread::sleep_for(std::chrono::milliseconds(100));
  vb::TsRingBuffer ring(1024*1024);vb::PipeClient client(control,ts,ring);for(int i=0;i<20&&!client.Open();++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));assert(client.connected());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));auto clean=Bytes(8,0x20);server.Broadcast(clean);assert(WaitPackets(ring,8));
  std::vector<std::uint8_t>misaligned{1,2,3,4,5};auto triple=Bytes(3,0x30);misaligned.insert(misaligned.end(),triple.begin(),triple.end());server.Broadcast(misaligned);assert(WaitPackets(ring,11));
  std::array<std::uint8_t,vb::kTsPacketSize*11>received{};assert(ring.Read(received)==received.size());for(std::size_t i=0;i<11;++i)assert(received[i*vb::kTsPacketSize]==0x47);

  server.Stop();std::this_thread::sleep_for(std::chrono::milliseconds(300));assert(!client.connected());assert(ring.PacketCount()==0);
  assert(server.Start());for(int i=0;i<30&&!client.connected();++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));assert(client.connected());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));server.Broadcast(clean);assert(WaitPackets(ring,8));
  ring.Purge();assert(ring.PacketCount()==0);client.Close();server.Stop();
  return 0;
}
