#pragma once
#include "virtual_broadcast/common/runtime_config.hpp"
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>
namespace vb {
struct DiagnosticIssue {bool error=false;std::string code,message;};
struct TsDiagnosticReport {std::uint64_t packets=0,continuity_errors=0,scrambled_packets=0,pcr_count=0,pcr_backward=0,pcr_large_gap=0;std::vector<std::pair<std::uint16_t,std::uint64_t>>pid_packets;std::vector<std::uint16_t>crc_error_pids,referenced_missing_pids;bool pat=false,pmt=false,nit=false,sdt=false,eit=false,tot=false,cdt=false,dsmcc=false;};
class CompatibilityDiagnostics {
 public:static std::vector<DiagnosticIssue> SelfCheck(const RuntimeConfig&);static void PrintChannels(const RuntimeConfig&,std::ostream&);static TsDiagnosticReport AnalyzeTs(const std::filesystem::path&);static void PrintReport(const TsDiagnosticReport&,std::ostream&);
};
}
