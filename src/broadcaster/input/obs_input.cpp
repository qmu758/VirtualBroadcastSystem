#include <WinSock2.h>
#include <WS2tcpip.h>
#include "virtual_broadcast/broadcaster/obs_input.hpp"
#include "virtual_broadcast/common/log.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace vb {
ObsInput::~ObsInput() {
  if (socket_ != INVALID_SOCKET) closesocket(socket_);
  if (wsa_started_) WSACleanup();
}
bool ObsInput::Open(std::uint16_t port, std::wstring& error) {
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2,2), &data) != 0) { error=L"WSAStartup failed"; return false; }
  wsa_started_=true; socket_=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (socket_==INVALID_SOCKET) { error=L"socket failed"; return false; }
  int receive_buffer=8*1024*1024;
  setsockopt(socket_,SOL_SOCKET,SO_RCVBUF,reinterpret_cast<const char*>(&receive_buffer),sizeof(receive_buffer));
  int actual_buffer=0;int option_length=sizeof(actual_buffer);
  getsockopt(socket_,SOL_SOCKET,SO_RCVBUF,reinterpret_cast<char*>(&actual_buffer),&option_length);
  Log("VirtualBroadcaster","UDP receive buffer="+std::to_string(actual_buffer)+" bytes");
  DWORD timeout=500;
  setsockopt(socket_,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
  sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(port); InetPtonW(AF_INET,L"127.0.0.1",&a.sin_addr);
  if (bind(socket_,reinterpret_cast<sockaddr*>(&a),sizeof(a))==SOCKET_ERROR) {
    error=L"bind failed on localhost UDP port "+std::to_wstring(port); return false;
  }
  return true;
}

std::span<const TsPacket> ObsInput::Receive(std::wstring& error) {
  std::array<std::uint8_t,65536> incoming{};
  const int count=recv(socket_,reinterpret_cast<char*>(incoming.data()),static_cast<int>(incoming.size()),0);
  packets_.clear();
  if (count==SOCKET_ERROR) {
    if (WSAGetLastError()!=WSAETIMEDOUT) error=L"UDP receive failed: "+std::to_wstring(WSAGetLastError());
    return packets_;
  }
  pending_.insert(pending_.end(),incoming.begin(),incoming.begin()+count);
  std::size_t pos=0;
  while (pending_.size()-pos>=kTsPacketSize) {
    if (pending_[pos]!=0x47 || (pending_.size()-pos>=kTsPacketSize*2 && pending_[pos+kTsPacketSize]!=0x47)) { ++pos; continue; }
    TsPacket packet{}; std::memcpy(packet.data(),pending_.data()+pos,kTsPacketSize); packets_.push_back(packet); pos+=kTsPacketSize;
  }
  if (pos) pending_.erase(pending_.begin(),pending_.begin()+pos);
  return packets_;
}
}
