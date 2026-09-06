#pragma once
#include "virtual_broadcast/common/ts.hpp"
#include <WinSock2.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vb {
class ObsInput {
 public:
  ~ObsInput();
  bool Open(std::uint16_t port, std::wstring& error);
  std::span<const TsPacket> Receive(std::wstring& error);
 private:
  SOCKET socket_ = INVALID_SOCKET;
  bool wsa_started_ = false;
  std::vector<std::uint8_t> pending_;
  std::vector<TsPacket> packets_;
};
}

