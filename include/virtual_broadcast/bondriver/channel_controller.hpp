#pragma once
#include "virtual_broadcast/common/pipe_protocol.hpp"
#include <cstdint>
#include <vector>
namespace vb {
class ChannelController {
 public:
  ChannelController(){channels_.push_back({0,0,101,32513,8,20,515142857,30.0F,"Virtual Broadcast"});}
  void SetChannels(std::vector<pipe::ChannelInfo> channels){if(!channels.empty())channels_=std::move(channels);}
  const std::vector<pipe::ChannelInfo>&channels()const{return channels_;}
  bool Select(std::uint32_t space,std::uint32_t channel) { if(space)return false;for(const auto&c:channels_)if(c.space==space&&c.channel==channel){space_=space;channel_=channel;return true;}return false; }
  std::uint32_t space() const{return space_;} std::uint32_t channel() const{return channel_;}
 private: std::vector<pipe::ChannelInfo>channels_;std::uint32_t space_=0,channel_=0;
};
}
