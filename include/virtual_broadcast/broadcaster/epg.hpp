#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace vb {
class AribStringEncoder;
inline constexpr std::uint16_t kPidEit=0x0012;
inline constexpr std::uint8_t kTableIdEitPfActual=0x4e,kTableIdEitScheduleActualFirst=0x50,kTableIdEitScheduleActualLast=0x5f;
inline constexpr std::uint8_t kDescriptorShortEvent=0x4d;
enum class RunningStatus:std::uint8_t {Undefined=0,NotRunning=1,StartsSoon=2,Pausing=3,Running=4};
enum class EpgMode {FixedInterval,Schedule};
struct EpgEvent {
  std::uint16_t event_id=0;
  std::chrono::sys_seconds start_time{};
  std::chrono::seconds duration{};
  std::string title,short_description,extended_description;
  RunningStatus running_status=RunningStatus::Undefined;
  bool free_ca_mode=false;
  std::chrono::sys_seconds End()const{return start_time+duration;}
};
struct EpgSettings {
  bool enabled=true;
  EpgMode mode=EpgMode::FixedInterval;
  int interval_minutes=60,fixed_anchor_minutes=0,schedule_days=7;
  std::string title_template="自主放送 {HH}時台";
  std::string description_template="松山自主放送の{HH}時台です";
  std::vector<EpgEvent> events;
};
class DurationEncoder { public:static std::array<std::uint8_t,3>Encode(std::chrono::seconds duration);};
class EventDescriptorWriter { public:static std::vector<std::uint8_t>Short(const AribStringEncoder&,std::string_view title,std::string_view text);};
class EpgSchedule {
 public:
  static EpgSchedule Build(const EpgSettings&,std::uint16_t service_id,std::chrono::sys_seconds now);
  const std::vector<EpgEvent>&events()const{return events_;}
  std::pair<const EpgEvent*,const EpgEvent*>PresentFollowing(std::chrono::sys_seconds now)const;
  std::uint64_t ContentHash()const;
  std::uint64_t PresentFollowingHash(std::chrono::sys_seconds now)const;
 private:std::vector<EpgEvent>events_;
};
class EpgScheduler { public:static std::uint16_t StableEventId(std::uint16_t service_id,std::chrono::sys_seconds start,std::size_t index);};
class EitPresentFollowingGenerator {
 public:static std::vector<std::vector<std::uint8_t>>Generate(const EpgSchedule&,std::chrono::sys_seconds now,std::uint16_t service_id,std::uint16_t tsid,std::uint16_t onid,const AribStringEncoder&,std::uint8_t version=0);
};
class EitScheduleGenerator {
 public:static std::vector<std::vector<std::uint8_t>>Generate(const EpgSchedule&,std::chrono::sys_seconds now,std::uint16_t service_id,std::uint16_t tsid,std::uint16_t onid,const AribStringEncoder&,std::uint8_t version=0);
};
class EitVersionTracker {
 public:std::uint8_t Update(std::uint64_t content_hash){if(initialized_&&hash_!=content_hash)version_=static_cast<std::uint8_t>((version_+1)&31);initialized_=true;hash_=content_hash;return version_;}std::uint8_t version()const{return version_;}
 private:bool initialized_=false;std::uint64_t hash_=0;std::uint8_t version_=0;
};
std::chrono::sys_seconds ParseJstTimestamp(std::string_view value);
}
