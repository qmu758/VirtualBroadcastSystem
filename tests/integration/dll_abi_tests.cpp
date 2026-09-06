#include "virtual_broadcast/bondriver/ibon_driver.hpp"
#include "virtual_broadcast/broadcaster/named_pipe_server.hpp"
#include "virtual_broadcast/common/ts.hpp"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

int wmain(int argc,wchar_t**argv){
  assert(argc==2);HMODULE dll=LoadLibraryW(argv[1]);assert(dll);
  using Create=IBonDriver* (__cdecl*)();const auto create=reinterpret_cast<Create>(GetProcAddress(dll,"CreateBonDriver"));assert(create);
  IBonDriver* base=create();assert(base);auto* driver=dynamic_cast<IBonDriver2*>(base);assert(driver);
  assert(driver->GetTunerName()!=nullptr);assert(driver->EnumTuningSpace(0)!=nullptr);assert(driver->EnumChannelName(0,0)!=nullptr);
  assert(driver->SetChannel(0,0));assert(!driver->SetChannel(0,1));assert(!driver->SetChannel(1,0));
  assert(!driver->OpenTuner());assert(!driver->IsTunerOpening());
  vb::NamedPipeServer server(L"VirtualBroadcastControl",L"VirtualBroadcastTS");assert(server.Start());std::this_thread::sleep_for(std::chrono::milliseconds(100));
  for(int i=0;i<20&&!driver->OpenTuner();++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));assert(driver->IsTunerOpening());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));std::vector<std::uint8_t>chunk(vb::kTsPacketSize*8);for(std::size_t i=0;i<8;++i)chunk[i*vb::kTsPacketSize]=0x47;server.Broadcast(chunk);
  assert(driver->WaitTsStream(2000)==WAIT_OBJECT_0);BYTE*data=nullptr;DWORD size=0,remain=0;assert(driver->GetTsStream(&data,&size,&remain));assert(data&&size==chunk.size());for(std::size_t i=0;i<8;++i)assert(data[i*vb::kTsPacketSize]==0x47);
  server.Broadcast(chunk);assert(driver->WaitTsStream(2000)==WAIT_OBJECT_0);driver->PurgeTsStream();assert(driver->GetReadyCount()==0);
  driver->CloseTuner();assert(!driver->IsTunerOpening());server.Stop();driver->Release();
  FreeLibrary(dll);return 0;
}
