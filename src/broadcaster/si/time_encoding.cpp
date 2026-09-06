#include "virtual_broadcast/broadcaster/si.hpp"
#include <stdexcept>

namespace vb {
std::uint8_t BcdEncoder::Encode(int value){if(value<0||value>99)throw std::out_of_range("BCD value");return static_cast<std::uint8_t>(((value/10)<<4)|(value%10));}
std::array<std::uint8_t,2>BcdEncoder::EncodeHoursMinutes(int minutes){minutes=std::abs(minutes);return{Encode(minutes/60),Encode(minutes%60)};}
std::array<std::uint8_t,3>BcdEncoder::EncodeTime(int h,int m,int s){return{Encode(h),Encode(m),Encode(s)};}
std::uint16_t MjdTimeEncoder::Mjd(std::chrono::sys_days day){const auto days=day.time_since_epoch().count();const auto mjd=days+40587;if(mjd<0||mjd>65535)throw std::out_of_range("MJD");return static_cast<std::uint16_t>(mjd);}
std::array<std::uint8_t,5>MjdTimeEncoder::Encode(std::chrono::sys_seconds utc){const auto day=std::chrono::floor<std::chrono::days>(utc);const auto tod=utc-day;const auto h=std::chrono::duration_cast<std::chrono::hours>(tod);const auto m=std::chrono::duration_cast<std::chrono::minutes>(tod-h);const auto s=std::chrono::duration_cast<std::chrono::seconds>(tod-h-m);const auto mjd=Mjd(day);const auto bcd=BcdEncoder::EncodeTime(static_cast<int>(h.count()),static_cast<int>(m.count()),static_cast<int>(s.count()));return{static_cast<std::uint8_t>(mjd>>8),static_cast<std::uint8_t>(mjd),bcd[0],bcd[1],bcd[2]};}
std::array<std::uint8_t,5>MjdTimeEncoder::EncodeJst(std::chrono::sys_seconds utc){return Encode(utc+std::chrono::hours{9});}
std::chrono::sys_seconds SystemClock::Now()const{return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());}
}
