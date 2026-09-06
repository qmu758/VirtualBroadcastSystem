#pragma once
#include "virtual_broadcast/common/ts.hpp"
#include "virtual_broadcast/common/pipe_protocol.hpp"
#include <Windows.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>

namespace vb {
class ClientSession {
 public:
  ClientSession(std::uint64_t id,HANDLE pipe,std::uint32_t channel); ~ClientSession();
  void Start(); void Stop(); void Enqueue(std::span<const std::uint8_t> bytes);
  bool connected() const{return connected_;}
  std::uint64_t client_id()const{return client_id_;}std::uint32_t selected_channel()const{return selected_channel_;}void Select(std::uint32_t channel){selected_channel_=channel;}
 private:
  void Run(); std::uint64_t client_id_;std::atomic_uint32_t selected_channel_;HANDLE pipe_=INVALID_HANDLE_VALUE; std::atomic_bool stop_=false,connected_=true;
  std::mutex mutex_; std::condition_variable ready_; std::deque<std::vector<std::uint8_t>> queue_; std::thread thread_;
};

class NamedPipeServer {
 public:
  NamedPipeServer(std::wstring control_name,std::wstring ts_name,std::vector<pipe::ChannelInfo> channels={});
  ~NamedPipeServer(); bool Start(); void Stop(); void Broadcast(std::span<const std::uint8_t> bytes);void Broadcast(std::uint32_t channel,std::span<const std::uint8_t> bytes);
  std::uint64_t bytes_sent() const{return bytes_sent_;}
 private:
  void TsAcceptLoop(); void ControlLoop(); void HandleControl(HANDLE pipe);
  std::wstring control_name_,ts_name_; std::atomic_bool stop_=false;
  std::thread ts_accept_thread_,control_thread_;std::vector<std::thread>control_sessions_;std::vector<HANDLE>control_handles_; std::mutex session_mutex_; std::vector<std::shared_ptr<ClientSession>> sessions_;std::deque<std::uint64_t>pending_ts_;std::unordered_map<std::uint64_t,std::uint32_t>selections_;std::vector<pipe::ChannelInfo>channels_;std::atomic_uint64_t next_client_id_=1;
  std::atomic_uint64_t bytes_sent_=0;
  std::atomic<HANDLE> control_client_=INVALID_HANDLE_VALUE;
};
}
