#include "virtual_broadcast/bondriver/pipe_client.hpp"
#include "virtual_broadcast/common/log.hpp"
#include "virtual_broadcast/common/runtime_config.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>

namespace vb {
namespace {
bool ReadExact(HANDLE h, std::span<std::uint8_t> b) {
  std::size_t p = 0;
  while (p < b.size()) {
    DWORD n = 0;
    if (!ReadFile(h, b.data() + p, static_cast<DWORD>(b.size() - p), &n,
                  nullptr) ||
        !n)
      return false;
    p += n;
  }
  return true;
}
bool WriteExact(HANDLE h, std::span<const std::uint8_t> b) {
  DWORD n = 0;
  return WriteFile(h, b.data(), static_cast<DWORD>(b.size()), &n, nullptr) &&
         n == b.size();
}
HANDLE Connect(const std::wstring &name, DWORD access) {
  const auto full = FullPipeName(name);
  for (int i = 0; i < 4; ++i) {
    const auto h = CreateFileW(full.c_str(), access, 0, nullptr, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h != INVALID_HANDLE_VALUE)
      return h;
    WaitNamedPipeW(full.c_str(), 150);
  }
  return INVALID_HANDLE_VALUE;
}
} // namespace
PipeClient::PipeClient(std::wstring control, std::wstring ts,
                       TsRingBuffer &buffer)
    : control_name_(std::move(control)), ts_name_(std::move(ts)),
      buffer_(buffer) {}
PipeClient::~PipeClient() { Close(); }
bool PipeClient::ConnectControl() {
  control_ = Connect(control_name_, GENERIC_READ | GENERIC_WRITE);
  if (control_ != INVALID_HANDLE_VALUE)
    Log("BonDriver_Virtual", "Control Pipe connected");
  return control_ != INVALID_HANDLE_VALUE;
}
bool PipeClient::ConnectTs() {
  ts_ = Connect(ts_name_, GENERIC_READ);
  if (ts_ != INVALID_HANDLE_VALUE)
    Log("BonDriver_Virtual", "TS Pipe connected");
  return ts_ != INVALID_HANDLE_VALUE;
}
bool PipeClient::Open() {
  if (connected_)
    return true;
  stop_ = false;
  if (!ConnectControl()) {
    Log("BonDriver_Virtual", "VirtualBroadcaster unavailable (control pipe)");
    return false;
  }
  if (!SendControl(pipe::Message::Hello) || !SendControl(pipe::Message::Open) ||
      !ConnectTs()) {
    Close();
    Log("BonDriver_Virtual", "VirtualBroadcaster unavailable (TS pipe)");
    return false;
  }
  connected_ = true;
  thread_ = std::thread(&PipeClient::ReceiveLoop, this);
  return true;
}
void PipeClient::Close() {
  stop_ = true;
  if (ts_ != INVALID_HANDLE_VALUE)
    CancelIoEx(ts_, nullptr);
  if (thread_.joinable())
    thread_.join();
  if (control_ != INVALID_HANDLE_VALUE)
    SendControl(pipe::Message::Close);
  for (HANDLE *h : {&ts_, &control_})
    if (*h != INVALID_HANDLE_VALUE) {
      CloseHandle(*h);
      *h = INVALID_HANDLE_VALUE;
    }
  connected_ = false;
  {std::lock_guard lock(stream_mutex_);pending_.clear();buffer_.Purge();}
}
bool PipeClient::SendControl(pipe::Message message,
                             std::span<const std::uint8_t> request,
                             std::vector<std::uint8_t> *body) {
  std::lock_guard lock(control_mutex_);
  if (control_ == INVALID_HANDLE_VALUE)
    return false;
  pipe::Header h{pipe::kVersion, message,
                 static_cast<std::uint32_t>(request.size()), ++request_id_};
  const auto raw = pipe::EncodeHeader(h);
  if (!WriteExact(control_, raw) ||
      (!request.empty() && !WriteExact(control_, request)))
    return false;
  std::array<std::uint8_t, 16> rh{};
  if (!ReadExact(control_, rh))
    return false;
  pipe::Header response{};
  if (!pipe::DecodeHeader(rh, response) ||
      response.request_id != h.request_id ||
      response.payload_size > pipe::kMaxPayload)
    return false;
  std::vector<std::uint8_t> payload(response.payload_size);
  if (!ReadExact(control_, payload) || payload.size() < 8 ||
      pipe::DecodeU32(payload) != static_cast<std::uint32_t>(pipe::Error::Ok))
    return false;
  if (body)
    body->assign(payload.begin() + 8, payload.end());
  return true;
}
bool PipeClient::EnumerateChannels(std::vector<pipe::ChannelInfo> &channels) {
  std::vector<std::uint8_t> b;
  const auto ok=SendControl(pipe::Message::EnumChannels, {}, &b) && pipe::DecodeChannels(b, channels);
  Log("BonDriver_Virtual",ok?"ENUM_CHANNELS count="+std::to_string(channels.size()):"ENUM_CHANNELS failed");
  return ok;
}
bool PipeClient::SetChannel(std::uint32_t channel) {
  const auto b = pipe::EncodeU32(channel);
  if (!SendControl(pipe::Message::SetChannel, b))
    return false;
  {std::lock_guard lock(stream_mutex_);buffer_.Purge();pending_.clear();}
  selected_channel_ = channel;
  Log("BonDriver_Virtual",
      "SetChannel remote=" + std::to_string(channel) + ", buffer purged");
  return true;
}
void PipeClient::ReceiveLoop() {
  std::array<std::uint8_t, 65536> bytes{};
  while (!stop_) {
    DWORD n = 0;
    if (ts_ != INVALID_HANDLE_VALUE &&
        ReadFile(ts_, bytes.data(), static_cast<DWORD>(bytes.size()), &n,
                 nullptr) &&
        n) {
      Consume({bytes.data(), n});
      continue;
    }
    if (stop_)
      break;
    connected_ = false;
    {std::lock_guard lock(stream_mutex_);buffer_.Purge();pending_.clear();}
    if (ts_ != INVALID_HANDLE_VALUE) {
      CloseHandle(ts_);
      ts_ = INVALID_HANDLE_VALUE;
    }
    if (control_ != INVALID_HANDLE_VALUE) {CloseHandle(control_);control_=INVALID_HANDLE_VALUE;}
    Log("BonDriver_Virtual", "Pipe session disconnected; reconnecting");
    for (int i = 0; i < 40 && !stop_; ++i) {
      if (ConnectControl() && SendControl(pipe::Message::Hello) &&
          SendControl(pipe::Message::Open) && ConnectTs() &&
          SendControl(pipe::Message::SetChannel,pipe::EncodeU32(selected_channel_))) {
        connected_ = true;
        ++reconnect_count_;
        Log("BonDriver_Virtual", "Control/TS pipes reconnected and channel restored");
        break;
      }
      if(control_!=INVALID_HANDLE_VALUE){CloseHandle(control_);control_=INVALID_HANDLE_VALUE;}
      if(ts_!=INVALID_HANDLE_VALUE){CloseHandle(ts_);ts_=INVALID_HANDLE_VALUE;}
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  }
  connected_ = false;
  {std::lock_guard lock(stream_mutex_);pending_.clear();buffer_.Purge();}
}
void PipeClient::Consume(std::span<const std::uint8_t> bytes) {
  std::lock_guard lock(stream_mutex_);
  if (!first_packet_ && !bytes.empty()) {first_packet_=true;Log("BonDriver_Virtual","First TS bytes received");}
  pending_.insert(pending_.end(), bytes.begin(), bytes.end());
  while (pending_.size() >= kTsPacketSize) {
    if (pending_[0] != 0x47) {
      std::size_t sync = 1;
      for (; sync < pending_.size(); ++sync) {
        if (pending_[sync] != 0x47)
          continue;
        if (sync + kTsPacketSize * 2 < pending_.size() &&
            pending_[sync + kTsPacketSize] == 0x47 &&
            pending_[sync + kTsPacketSize * 2] == 0x47)
          break;
      }
      if (sync == pending_.size()) {
        if (pending_.size() > kTsPacketSize * 2)
          pending_.erase(pending_.begin(), pending_.end() - kTsPacketSize * 2);
        return;
      }
      pending_.erase(pending_.begin(), pending_.begin() + sync);
      Log("BonDriver_Virtual",
          "TS resync: discarded " + std::to_string(sync) + " bytes");
      continue;
    }
    std::size_t count = 0;
    while ((count + 1) * kTsPacketSize <= pending_.size() &&
           pending_[count * kTsPacketSize] == 0x47)
      ++count;
    if (!count)
      continue;
    std::vector<TsPacket> packets(count);
    std::memcpy(packets.data(), pending_.data(), count * kTsPacketSize);
    const auto dropped = buffer_.Write(packets);
    if (dropped)
      Log("BonDriver_Virtual", "Ring buffer overflow: dropped " +
                                   std::to_string(dropped) + " old packets");
    pending_.erase(pending_.begin(), pending_.begin() + count * kTsPacketSize);
  }
}
} // namespace vb
