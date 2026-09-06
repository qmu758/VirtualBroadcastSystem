#pragma once
#include "virtual_broadcast/broadcaster/psi.hpp"
#include "virtual_broadcast/broadcaster/epg.hpp"
#include "virtual_broadcast/broadcaster/logo.hpp"
#include "virtual_broadcast/broadcaster/data_broadcast.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vb {
inline constexpr std::uint16_t kPidNit=0x0010,kPidSdt=0x0011,kPidTot=0x0014;
inline constexpr std::uint8_t kTableIdNitActual=0x40,kTableIdSdtActual=0x42,kTableIdTot=0x73;
inline constexpr std::uint8_t kDescriptorNetworkName=0x40,kDescriptorServiceList=0x41,kDescriptorService=0x48,kDescriptorLocalTimeOffset=0x58,kDescriptorTsInformation=0xcd,kDescriptorTerrestrialDelivery=0xfa;

struct SiSettings {
  std::string service_name="Matsuyama Virtual Broadcast";
  std::string provider_name="Virtual Broadcasting";
  std::string network_name="Virtual Broadcast Network";
  std::string ts_name="Virtual Broadcast TS";
  std::uint16_t original_network_id=32760,network_id=32760,transport_stream_id=32513,service_id=101;
  std::uint8_t service_type=1;
  std::uint8_t remote_control_key_id=8;
  std::string country_code="JPN";
  int local_offset_minutes=540;
  bool nit_enabled=true,sdt_enabled=true,tot_enabled=true;
  EpgSettings epg;
  LogoSettings logo;
  DataBroadcastSettings data_broadcast;
  bool local_time_offset_descriptor_enabled=true;
  std::chrono::milliseconds pat_interval{100},pmt_interval{100},nit_interval{1000},sdt_interval{1000},tot_interval{5000},eit_pf_interval{500},eit_schedule_interval{2000},cdt_interval{30000};
};

struct AribEncodedString { std::vector<std::uint8_t> bytes; bool replaced=false; };
class AribStringEncoder { public: AribEncodedString Encode(std::string_view utf8)const; };
class BcdEncoder {
 public: static std::uint8_t Encode(int value); static std::array<std::uint8_t,2> EncodeHoursMinutes(int minutes); static std::array<std::uint8_t,3> EncodeTime(int hour,int minute,int second);
};
class MjdTimeEncoder {
 public:
  static std::uint16_t Mjd(std::chrono::sys_days day);
  static std::array<std::uint8_t,5> Encode(std::chrono::sys_seconds local_time);
  static std::array<std::uint8_t,5> EncodeJst(std::chrono::sys_seconds utc);
};
class IClock { public: virtual ~IClock()=default; virtual std::chrono::sys_seconds Now()const=0; };
class SystemClock final:public IClock { public: std::chrono::sys_seconds Now()const override; };

class DescriptorWriter {
 public:
  static std::vector<std::uint8_t> Service(std::uint8_t type,std::span<const std::uint8_t> provider,std::span<const std::uint8_t> name);
  static std::vector<std::uint8_t> NetworkName(std::span<const std::uint8_t> name);
  static std::vector<std::uint8_t> ServiceList(std::uint16_t service_id,std::uint8_t service_type);
  static std::vector<std::uint8_t> TsInformation(std::uint8_t remote_key,std::span<const std::uint8_t> ts_name);
  static std::vector<std::uint8_t> LocalTimeOffset(const std::string& country,int offset_minutes,std::chrono::sys_seconds change);
};
class NitGenerator { public: static std::vector<std::uint8_t> Generate(const SiSettings& settings,const AribStringEncoder& encoder,std::uint8_t version=0); };
class SdtGenerator { public: static std::vector<std::uint8_t> Generate(const SiSettings& settings,const AribStringEncoder& encoder,std::uint8_t version=0); };
class TotGenerator {
 public: TotGenerator(const SiSettings& settings,const IClock& clock):settings_(settings),clock_(clock){} std::vector<std::uint8_t> Generate()const;
 private: const SiSettings& settings_;const IClock& clock_;
};

enum class SiTable {Pat,Pmt,Nit,Sdt,Tot,EitPf,EitSchedule,Cdt};
class SiScheduler {
 public: explicit SiScheduler(const SiSettings& settings); bool Due(SiTable table,std::chrono::steady_clock::time_point now);
 private: std::array<std::chrono::milliseconds,8> intervals_;std::array<std::chrono::steady_clock::time_point,8> next_{};
};
class PsiSiGenerator {
 public:
  PsiSiGenerator(SiSettings settings,std::shared_ptr<IClock> clock=std::make_shared<SystemClock>());
  std::vector<TsPacket> GenerateDue(const InputProgram& input,std::uint16_t pmt_pid,std::uint16_t video_pid,std::uint16_t audio_pid,std::chrono::steady_clock::time_point now);
  const SiSettings& settings()const{return settings_;}
 private: SiSettings settings_;std::shared_ptr<IClock> clock_;AribStringEncoder encoder_;SectionPacketizer packetizer_;SiScheduler scheduler_;LogoManager logo_;DataBroadcastManager data_broadcast_;EitVersionTracker eit_pf_version_,eit_schedule_version_;std::optional<std::uint16_t>current_event_,following_event_;
};
}
