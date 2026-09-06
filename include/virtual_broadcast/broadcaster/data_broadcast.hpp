#pragma once
#include "virtual_broadcast/common/ts.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace vb {
inline constexpr std::uint8_t kStreamTypeDsmcc=0x0d,kTableIdDii=0x3b,kTableIdDdb=0x3c;
inline constexpr std::uint16_t kDataComponentIdBml=0x000c,kDsmccMessageIdDii=0x1002,kDsmccMessageIdDdb=0x1003;
inline constexpr std::uint8_t kBmlComponentTag=0x40;
struct DataBroadcastSettings {
  bool enabled=false; std::filesystem::path root; std::string entry="index.bml";
  std::uint16_t pid=0x0200; std::uint32_t download_id=0xf0000001; std::uint8_t data_event_id=0x0f;
  std::uint16_t block_size=4066; std::chrono::milliseconds dii_interval{1000},carousel_cycle{5000};
  std::uint32_t max_bitrate_kbps=256; std::size_t max_modules=256,max_total_bytes=16*1024*1024,max_module_bytes=1024*1024;
};
struct CarouselModule {std::uint16_t id=0;std::uint8_t version=0;std::string path,mime;std::vector<std::uint8_t> data,module_info;};
struct BmlApplication {std::string profile="A-profile",version="1.0",entry;std::vector<CarouselModule> modules;};
struct DataCarousel {std::uint32_t download_id=0;std::uint16_t block_size=4066;BmlApplication application;};
class BmlResourceScanner {public:static std::vector<std::filesystem::path> Scan(const DataBroadcastSettings&);static void ValidateBml(std::span<const std::uint8_t>,std::string_view);};
class CarouselModuleBuilder {public:static DataCarousel Build(const DataBroadcastSettings&);};
class DiiGenerator {public:static std::vector<std::uint8_t> Generate(const DataCarousel&,std::uint32_t transaction_id=1);};
class DdbGenerator {public:static std::vector<std::vector<std::uint8_t>> Generate(const DataCarousel&);};
class DsmccGenerator {public:static std::vector<std::vector<std::uint8_t>> GenerateAll(const DataCarousel&);};
class DataBroadcastDescriptorWriter {public:static std::vector<std::uint8_t> PmtDescriptors(std::uint8_t data_event_id=0x0f);};
class CarouselScheduler {
 public: explicit CarouselScheduler(const DataBroadcastSettings&);std::vector<std::vector<std::uint8_t>> Due(const DataCarousel&,std::chrono::steady_clock::time_point);
 private:DataBroadcastSettings settings_;std::chrono::steady_clock::time_point next_dii_{},next_block_{};std::size_t block_=0;
};
class DataBroadcastManager {
 public: explicit DataBroadcastManager(DataBroadcastSettings);bool enabled()const{return carousel_.has_value();}const DataCarousel& carousel()const{return *carousel_;}const DataBroadcastSettings& settings()const{return settings_;}std::vector<std::vector<std::uint8_t>> Due(std::chrono::steady_clock::time_point now);
 private:DataBroadcastSettings settings_;std::optional<DataCarousel>carousel_;CarouselScheduler scheduler_;
};
}
