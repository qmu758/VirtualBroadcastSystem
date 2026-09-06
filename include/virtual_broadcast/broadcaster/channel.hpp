#pragma once
#include "virtual_broadcast/broadcaster/si.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vb {
struct RegionConfig {std::string country="JPN",prefecture="愛媛県",area="松山",timezone="Asia/Tokyo";};
struct AribRegionMapping {std::string prefecture;std::uint16_t area_code;};
class RegionManager {
 public:
  static void Validate(const RegionConfig&);
  static const std::vector<std::string>& Prefectures();
};
struct ChannelIds {std::uint16_t original_network_id=32760,network_id=32760,transport_stream_id=32513,service_id=101,download_data_id=0x8001,logo_id=1;};
struct ExistingIds {std::uint16_t original_network_id=0,transport_stream_id=0,service_id=0,download_data_id=0;};
struct VirtualChannel {
  std::uint32_t channel_index=0;std::string name="Virtual Broadcast",ts_name="松山自主放送";
  std::uint16_t udp_port=12340;std::filesystem::path debug_path="output/channel-0.ts";
  std::uint8_t remote_control_key_id=8,physical_channel=20;std::uint64_t center_frequency_hz=515142857;
  float signal_level=30.0F;bool auto_ids=false;RegionConfig region;ChannelIds ids;SiSettings si;
};
class IdManager {
 public:
  static ChannelIds Generate(std::string_view stable_name,std::span<const ExistingIds> existing={});
  static void Validate(std::span<const VirtualChannel> channels,std::span<const ExistingIds> existing={});
  static std::vector<ExistingIds> LoadExistingJson(const std::filesystem::path& path);
};
class ChannelManager {
 public:
  explicit ChannelManager(std::vector<VirtualChannel> channels,std::vector<ExistingIds> existing={});
  const std::vector<VirtualChannel>& EnumerateChannels()const{return channels_;}
  const VirtualChannel* GetChannel(std::uint32_t space,std::uint32_t channel)const;
 private:std::vector<VirtualChannel> channels_;
};
}
