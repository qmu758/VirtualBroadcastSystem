#include "virtual_broadcast/bondriver/channel_controller.hpp"
#include "virtual_broadcast/bondriver/ibon_driver.hpp"
#include "virtual_broadcast/bondriver/pipe_client.hpp"
#include "virtual_broadcast/common/log.hpp"
#include "virtual_broadcast/common/runtime_config.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <mutex>

namespace {
HMODULE module_handle = nullptr;
}
namespace {
std::wstring Wide(const std::string &s) {
  if (s.empty())
    return {};
  const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                    static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(n, 0);
  if (n)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                        static_cast<int>(s.size()), w.data(), n);
  return w;
}
} // namespace

class BonDriverVirtual final : public IBonDriver2 {
public:
  BonDriverVirtual() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module_handle, path, MAX_PATH);
    config_ = vb::LoadRuntimeConfig(std::filesystem::path(path).parent_path() /
                                    L"BonDriver_Virtual.toml");
    ring_ = std::make_unique<vb::TsRingBuffer>(config_.buffer_size_mb * 1024 *
                                               1024);
    client_ = std::make_unique<vb::PipeClient>(config_.control_pipe,
                                               config_.ts_pipe, *ring_);
    vb::Log("BonDriver_Virtual",std::string("DLL/CreateBonDriver architecture=")+(sizeof(void*)==8?"x64":"x86")+", process="+(sizeof(void*)==8?"64-bit":"32-bit"));
  }
  ~BonDriverVirtual() { CloseTuner(); }
  const BOOL OpenTuner() override {
    std::lock_guard lock(mutex_);
    if (open_)
      return TRUE;
    vb::Log("BonDriver_Virtual", "OpenTuner");
    open_ = client_->Open();
    if (open_) {
      std::vector<vb::pipe::ChannelInfo> list;
      if (client_->EnumerateChannels(list)) {
        channels_.SetChannels(std::move(list));
        channel_names_.clear();
        for (const auto &c : channels_.channels())
          channel_names_.push_back(Wide(c.name));
      }
    }
    return open_ ? TRUE : FALSE;
  }
  void CloseTuner() override {
    std::lock_guard lock(mutex_);
    if (!open_)
      return;
    vb::Log("BonDriver_Virtual", "CloseTuner");
    client_->Close();
    open_ = false;
  }
  const BOOL SetChannel(const BYTE channel) override {
    return SetChannel(0, channel);
  }
  const float GetSignalLevel() override {
    return client_->connected() ? config_.signal_level : 0.0F;
  }
  const DWORD WaitTsStream(const DWORD timeout = 0) override {
    if (!open_)
      return WAIT_ABANDONED;
    return ring_->Wait(timeout) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
  }
  const DWORD GetReadyCount() override {
    return static_cast<DWORD>(
        std::min<std::size_t>(ring_->PacketCount(), MAXDWORD));
  }
  const BOOL GetTsStream(BYTE *destination, DWORD *size,
                         DWORD *remain) override {
    if (!destination || !size || !open_)
      return FALSE;
    const auto capacity = *size ? *size : static_cast<DWORD>(stream_.size());
    const auto read = ring_->Read({destination, capacity});
    LogFirstRead(read);
    *size = static_cast<DWORD>(read);
    if (remain)
      *remain = GetReadyCount();
    return TRUE;
  }
  const BOOL GetTsStream(BYTE **destination, DWORD *size,
                         DWORD *remain) override {
    if (!destination || !size || !open_)
      return FALSE;
    const auto read = ring_->Read(stream_);
    LogFirstRead(read);
    *destination = stream_.data();
    *size = static_cast<DWORD>(read);
    if (remain)
      *remain = GetReadyCount();
    return TRUE;
  }
  void PurgeTsStream() override {
    ring_->Purge();
    client_->SendControl(vb::pipe::Message::Purge);
    vb::Log("BonDriver_Virtual", "PurgeTsStream");
  }
  void Release() override { delete this; }
  LPCTSTR GetTunerName() override { return config_.tuner_name.c_str(); }
  const BOOL IsTunerOpening() override { return open_ ? TRUE : FALSE; }
  LPCTSTR EnumTuningSpace(const DWORD space) override {
    return space == 0 ? L"Virtual" : nullptr;
  }
  LPCTSTR EnumChannelName(const DWORD space, const DWORD channel) override {
    if (space || channel >= channel_names_.size())
      return nullptr;
    return channel_names_[channel].c_str();
  }
  const BOOL SetChannel(const DWORD space, const DWORD channel) override {
    if (!channels_.Select(space, channel) ||
        (open_ && !client_->SetChannel(channel))) {
      vb::Log("BonDriver_Virtual", "SetChannel rejected");
      return FALSE;
    }
    ring_->Purge();
    vb::Log("BonDriver_Virtual", "SetChannel 0/" + std::to_string(channel));
    return TRUE;
  }
  const DWORD GetCurSpace() override { return channels_.space(); }
  const DWORD GetCurChannel() override { return channels_.channel(); }

private:
  void LogFirstRead(std::size_t bytes){if(bytes&&!first_read_logged_){first_read_logged_=true;vb::Log("BonDriver_Virtual","GetTsStream first success bytes="+std::to_string(bytes));}}
  std::mutex mutex_;
  vb::RuntimeConfig config_;
  std::unique_ptr<vb::TsRingBuffer> ring_;
  std::unique_ptr<vb::PipeClient> client_;
  vb::ChannelController channels_;
  std::vector<std::wstring> channel_names_{L"Virtual Broadcast"};
  bool open_ = false;
  bool first_read_logged_ = false;
  std::array<std::uint8_t, vb::kTsPacketSize * 256> stream_{};
};

extern "C" __declspec(dllexport) IBonDriver *CreateBonDriver() {
  try {
    return new BonDriverVirtual();
  } catch (...) {
    return nullptr;
  }
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    module_handle = module;
    DisableThreadLibraryCalls(module);
  }
  return TRUE;
}
