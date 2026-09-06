#include "virtual_broadcast/common/log.hpp"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
namespace vb {
void Log(std::string_view component,std::string_view message){static std::mutex mutex;std::lock_guard lock(mutex);const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());tm local{};localtime_s(&local,&now);char stamp[32]{};strftime(stamp,sizeof(stamp),"%Y-%m-%d %H:%M:%S",&local);const std::string line=std::string(stamp)+" ["+std::string(component)+"] "+std::string(message)+"\n";OutputDebugStringA(line.c_str());std::ofstream(std::string(component)+".log",std::ios::app)<<line;}
}
