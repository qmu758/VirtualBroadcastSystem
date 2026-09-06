#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace vb {
inline constexpr std::uint16_t kPidCdt=0x0029;
inline constexpr std::uint8_t kTableIdCdt=0xc8,kDescriptorLogoTransmission=0xcf,kCdtDataTypeLogo=0x01,kLogoTransmissionTypeDirect=0x01;
struct LogoSettings {
  bool enabled=false,auto_resize=true;
  std::filesystem::path source="logo.png";
  std::string resize_mode="contain";
  std::uint16_t logo_id=1,logo_version=0,download_data_id=0x8001;
  std::uint8_t logo_type=0x05,cdt_version=0;
};
struct LogoDimensions{std::uint32_t width,height;};
struct LogoDataModule {LogoSettings settings;LogoDimensions dimensions{};std::vector<std::uint8_t>png;std::vector<std::uint8_t>bytes;};
class LogoEncoder {
 public:
  static LogoDimensions DimensionsForType(std::uint8_t logo_type);
  static LogoDataModule Encode(const LogoSettings&);
};
class LogoDataModuleBuilder { public:static std::vector<std::uint8_t>Build(const LogoSettings&,std::span<const std::uint8_t>png);};
class LogoManager {
 public:explicit LogoManager(const LogoSettings&);bool enabled()const{return module_!=nullptr;}const LogoDataModule&module()const;
 private:std::shared_ptr<const LogoDataModule>module_;
};
class CdtVersionTracker {
 public:
  explicit CdtVersionTracker(std::uint8_t version=0):version_(version&0x1f){}
  std::uint8_t version()const{return version_;}
  void ContentChanged(){version_=static_cast<std::uint8_t>((version_+1)&0x1f);}
 private:std::uint8_t version_;
};
class LogoTransmissionDescriptorWriter { public:static std::vector<std::uint8_t>Write(const LogoSettings&);};
class CdtGenerator { public:static std::vector<std::vector<std::uint8_t>>Generate(const LogoDataModule&,std::uint16_t original_network_id);};
}
