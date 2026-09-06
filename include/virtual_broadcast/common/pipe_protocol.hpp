#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vb::pipe {
inline constexpr std::uint32_t kMagic = 0x31504256;
inline constexpr std::uint16_t kVersion = 2;
inline constexpr std::uint32_t kMaxPayload = 4096;
enum class Message : std::uint16_t { Hello=1, GetVersion=2, Open=3, Close=4, Purge=5, GetStatus=6,EnumChannels=7,GetChannelInfo=8,SetChannel=9 };
enum class Error : std::uint32_t { Ok=0, BadMagic=1, UnsupportedVersion=2, UnknownMessage=3, InvalidPayload=4, Unavailable=5, Internal=6 };
struct Header { std::uint16_t version; Message type; std::uint32_t payload_size; std::uint32_t request_id; };
std::array<std::uint8_t,16> EncodeHeader(const Header& header);
bool DecodeHeader(std::span<const std::uint8_t> bytes,Header& header);
std::array<std::uint8_t,4> EncodeU32(std::uint32_t value);
std::uint32_t DecodeU32(std::span<const std::uint8_t> bytes);
struct ChannelInfo {std::uint32_t space=0,channel=0;std::uint16_t service_id=0,transport_stream_id=0;std::uint8_t remote_control_key_id=0,physical_channel=0;std::uint64_t center_frequency_hz=0;float signal_level=30;std::string name;};
std::vector<std::uint8_t> EncodeChannels(std::span<const ChannelInfo> channels);
bool DecodeChannels(std::span<const std::uint8_t> bytes,std::vector<ChannelInfo>& channels);
}
