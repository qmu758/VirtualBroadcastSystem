#include "virtual_broadcast/broadcaster/ts_output_router.hpp"
#include "virtual_broadcast/common/log.hpp"
#include <cstring>
#include <iostream>

namespace vb {
bool TsOutputRouter::Open(const RuntimeConfig& c){file_bytes_=0;file_limit_logged_=false;max_file_bytes_=(static_cast<std::uint64_t>(c.max_file_size_mb)*1024*1024/kTsPacketSize)*kTsPacketSize;if(c.file_enabled){if(c.file_path.has_parent_path())std::filesystem::create_directories(c.file_path.parent_path());file_.open(c.file_path,std::ios::binary|std::ios::trunc);if(!file_)return false;}if(c.pipe_enabled){pipe_=std::make_unique<NamedPipeServer>(c.control_pipe,c.ts_pipe);if(!pipe_->Start())return false;}pipe_chunk_.reserve(kTsPacketSize*256);return true;}
void TsOutputRouter::Write(std::span<const TsPacket> packets){for(const auto&p:packets){if(file_&&file_bytes_+p.size()<=max_file_bytes_){file_.write(reinterpret_cast<const char*>(p.data()),p.size());file_bytes_+=p.size();}else if(file_&&!file_limit_logged_){file_limit_logged_=true;file_.flush();Log("VirtualBroadcaster","Debug TS size limit reached; file output paused");}if(pipe_){const auto old=pipe_chunk_.size();pipe_chunk_.resize(old+p.size());std::memcpy(pipe_chunk_.data()+old,p.data(),p.size());if(pipe_chunk_.size()>=kTsPacketSize*256)FlushPipe();}}}
void TsOutputRouter::FlushPipe(){if(pipe_&&!pipe_chunk_.empty()){pipe_->Broadcast(pipe_chunk_);pipe_chunk_.clear();}}
void TsOutputRouter::Close(){FlushPipe();if(file_)file_.flush();if(pipe_)pipe_->Stop();pipe_.reset();file_.close();}
}
