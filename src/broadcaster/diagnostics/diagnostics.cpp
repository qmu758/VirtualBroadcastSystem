#include "virtual_broadcast/broadcaster/diagnostics.hpp"
#include "virtual_broadcast/broadcaster/data_broadcast.hpp"
#include "virtual_broadcast/broadcaster/psi.hpp"
#include "virtual_broadcast/broadcaster/si.hpp"
#include <array>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
namespace vb {
namespace {
std::uint16_t Pid(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(((p[1] & 31) << 8) | p[2]);
}
std::uint64_t Pcr(const std::uint8_t *p) {
  return (std::uint64_t(p[6]) << 25) | (std::uint64_t(p[7]) << 17) |
         (std::uint64_t(p[8]) << 9) | (std::uint64_t(p[9]) << 1) | (p[10] >> 7);
}
void Issue(std::vector<DiagnosticIssue> &v, bool error, std::string code,
           std::string message) {
  v.push_back({error, std::move(code), std::move(message)});
}
}
std::vector<DiagnosticIssue>
CompatibilityDiagnostics::SelfCheck(const RuntimeConfig &c) {
  std::vector<DiagnosticIssue> v;
  try {
    IdManager::Validate(c.channels);
  } catch (const std::exception &e) {
    Issue(v, true, "ID_COLLISION", e.what());
  }
  for (const auto &ch : c.channels) {
    const auto prefix = "channel " + std::to_string(ch.channel_index) + ": ";
    std::set<std::uint16_t> pids{0,       kPidNit, kPidSdt, kPidEit, kPidTot,
                                 kPidCdt, 0x0100,  0x0101,  0x0110};
    if (pids.size() != 9)
      Issue(v, true, "PID_COLLISION", prefix + "fixed PID collision");
    if (ch.si.logo.enabled &&
        !std::filesystem::is_regular_file(ch.si.logo.source))
      Issue(v, true, "LOGO_MISSING", prefix + ch.si.logo.source.string());
    if (ch.si.logo.enabled &&
        (ch.si.logo.logo_id != ch.ids.logo_id ||
         ch.si.logo.download_data_id != ch.ids.download_data_id))
      Issue(v, true, "LOGO_ID_MISMATCH", prefix + "SDT/CDT identifiers differ");
    const auto &d = ch.si.data_broadcast;
    if (d.enabled) {
      if (pids.contains(d.pid) || d.pid < 0x20 || d.pid >= 0x1fff)
        Issue(v, true, "DSMCC_PID_COLLISION", prefix + std::to_string(d.pid));
      if (!std::filesystem::is_directory(d.root))
        Issue(v, true, "BML_ROOT_MISSING", prefix + d.root.string());
      else if (!std::filesystem::is_regular_file(d.root / d.entry))
        Issue(v, true, "BML_ENTRY_MISSING", prefix + d.entry);
      try {
        (void)CarouselModuleBuilder::Build(d);
      } catch (const std::exception &e) {
        Issue(v, true, "BML_INVALID", prefix + e.what());
      }
    }
    try {
      auto events =
          EpgSchedule::Build(ch.si.epg, ch.si.service_id, SystemClock{}.Now());
      (void)events;
    } catch (const std::exception &e) {
      Issue(v, true, "EPG_INVALID", prefix + e.what());
    }
    if (ch.si.transport_stream_id != ch.ids.transport_stream_id ||
        ch.si.service_id != ch.ids.service_id ||
        ch.si.original_network_id != ch.ids.original_network_id)
      Issue(v, true, "SI_ID_MISMATCH",
            prefix + "channel IDs differ from SI IDs");
  }
  if (v.empty())
    Issue(v, false, "OK",
          "PAT/PMT/SI, IDs, logo, EPG and DSM-CC configuration are consistent");
  return v;
}
void CompatibilityDiagnostics::PrintChannels(const RuntimeConfig &c,
                                             std::ostream &o) {
  for (const auto &ch : c.channels) {
    const auto &d = ch.si.data_broadcast;
    o << "Channel " << ch.channel_index << " " << ch.si.service_name
      << "\n  PAT=0x0000 PMT=0x0100 NIT=0x0010 SDT=0x0011 EIT=0x0012 "
         "TOT=0x0014 CDT=0x0029 DSM-CC="
      << (d.enabled ? "0x" +
                          [](std::uint16_t p) {
                            std::ostringstream s;
                            s << std::hex << p;
                            return s.str();
                          }(d.pid)
                    : "disabled")
      << "\n  Video=0x0101 Audio=0x0110 PCR=0x0101 SID=" << ch.si.service_id
      << " TSID=" << ch.si.transport_stream_id
      << " ONID=" << ch.si.original_network_id
      << " RemoteKey=" << unsigned(ch.remote_control_key_id)
      << "\n  EPG=" << (ch.si.epg.enabled ? "on" : "off")
      << " Logo=" << (ch.si.logo.enabled ? "on" : "off")
      << " DataBroadcast=" << (d.enabled ? "on" : "off") << "\n";
  }
}
TsDiagnosticReport
CompatibilityDiagnostics::AnalyzeTs(const std::filesystem::path &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot open TS");
  TsDiagnosticReport r;
  std::map<std::uint16_t, std::uint64_t> counts, last_pcr;
  std::map<std::uint16_t, int> cc;
  std::set<std::uint16_t> present, referenced;
  std::array<std::uint8_t, 188> p{};
  while (f.read(reinterpret_cast<char *>(p.data()), p.size())) {
    if (p[0] != 0x47)
      throw std::runtime_error("TS sync lost at packet " +
                               std::to_string(r.packets));
    ++r.packets;
    const auto pid = Pid(p.data());
    present.insert(pid);
    ++counts[pid];
    if ((p[3] >> 6) & 3)
      ++r.scrambled_packets;
    const auto afc = (p[3] >> 4) & 3;
    const bool payload = (afc & 1) != 0;
    if (payload && pid != 0x1fff) {
      const auto cur = p[3] & 15;
      if (cc.contains(pid) && cur != ((cc[pid] + 1) & 15))
        ++r.continuity_errors;
      cc[pid] = cur;
    }
    if ((afc & 2) && p[4] >= 7 && (p[5] & 0x10)) {
      const auto value = Pcr(p.data());
      ++r.pcr_count;
      if (last_pcr.contains(pid)) {
        if (value < last_pcr[pid])
          ++r.pcr_backward;
        else if (value - last_pcr[pid] > 2700000)
          ++r.pcr_large_gap;
      }
      last_pcr[pid] = value;
    }
    std::size_t pos = 4;
    if (afc & 2)
      pos += 1 + p[4];
    if (payload && pos < 188 && (p[1] & 0x40)) {
      pos += 1 + p[pos];
      if (pos + 3 <= 188) {
        const auto tid = p[pos];
        const auto len = 3 + (((p[pos + 1] & 15) << 8) | p[pos + 2]);
        if (pos + len <= 188) {
          std::span<const std::uint8_t> s(p.data() + pos, len);
          if ((p[pos + 1] & 0x80) && !HasValidMpegCrc(s))
            r.crc_error_pids.push_back(pid);
          r.pat |= tid == 0x00;
          r.pmt |= tid == 0x02;
          r.nit |= tid == 0x40;
          r.sdt |= tid == 0x42;
          r.eit |= tid == 0x4e || (tid >= 0x50 && tid <= 0x5f);
          r.tot |= tid == 0x73;
          r.cdt |= tid == 0xc8;
          r.dsmcc |= tid == 0x3b || tid == 0x3c;
          if (tid == 0x00 && len >= 16)
            referenced.insert(
                static_cast<std::uint16_t>(((s[10] & 31) << 8) | s[11]));
          if (tid == 0x02 && len >= 16) {
            referenced.insert(
                static_cast<std::uint16_t>(((s[8] & 31) << 8) | s[9]));
            for (std::size_t q = 12 + (((s[10] & 15) << 8) | s[11]);
                 q + 5 < len - 4;) {
              referenced.insert(static_cast<std::uint16_t>(
                  ((s[q + 1] & 31) << 8) | s[q + 2]));
              q += 5 + (((s[q + 3] & 15) << 8) | s[q + 4]);
            }
          }
        }
      }
    }
  }
  for (const auto &[pid, n] : counts)
  r.pid_packets.push_back({pid, n});
for (auto pid : referenced)
  if (!present.contains(pid))
    r.referenced_missing_pids.push_back(pid);
  return r;
}
void CompatibilityDiagnostics::PrintReport(const TsDiagnosticReport &r,
                                           std::ostream &o) {
  o << "Packets=" << r.packets << " continuity_errors=" << r.continuity_errors
    << " scrambled=" << r.scrambled_packets << " PCR=" << r.pcr_count
    << " backward=" << r.pcr_backward << " large_gap=" << r.pcr_large_gap
    << "\n";
  for (const auto &[pid, n] : r.pid_packets)
    o << "PID 0x" << std::hex << pid << std::dec << " packets=" << n << "\n";
  o << "Tables PAT=" << r.pat << " PMT=" << r.pmt << " NIT=" << r.nit
    << " SDT=" << r.sdt << " EIT=" << r.eit << " TOT=" << r.tot
    << " CDT=" << r.cdt << " DSMCC=" << r.dsmcc
    << "\nCRC errors=" << r.crc_error_pids.size()
    << " referenced missing PIDs=" << r.referenced_missing_pids.size() << "\n";
}
}
