#include "virtual_broadcast/broadcaster/si.hpp"
#include <Windows.h>

namespace vb {
AribEncodedString AribStringEncoder::Encode(std::string_view utf8)const{
  AribEncodedString result;if(utf8.empty())return result;
  const int wn=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
  if(!wn){result.bytes={'?'};result.replaced=true;return result;}std::wstring wide(wn,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),wide.data(),wn);
  BOOL replaced=FALSE;const int sn=WideCharToMultiByte(932,0,wide.data(),wn,nullptr,0,"?",&replaced);std::string sjis(sn,0);WideCharToMultiByte(932,0,wide.data(),wn,sjis.data(),sn,"?",&replaced);result.replaced=replaced!=FALSE;bool alnum=false;
  for(std::size_t i=0;i<sjis.size();++i){const auto c=static_cast<std::uint8_t>(sjis[i]);const bool lead=(c>=0x81&&c<=0x9f)||(c>=0xe0&&c<=0xef);if(lead&&i+1<sjis.size()){
      if(alnum){result.bytes.push_back(0x0f);alnum=false;}const auto trail=static_cast<std::uint8_t>(sjis[++i]);if(!((trail>=0x40&&trail<=0x7e)||(trail>=0x80&&trail<=0xfc))){result.bytes.push_back(0x22);result.bytes.push_back(0x2e);result.replaced=true;continue;}
      int row=c<0xa0?(c-0x81)*2+0x21:(c-0xc1)*2+0x21;int cell;if(trail>=0x9f){++row;cell=trail-0x7e;}else cell=trail-(trail>0x7e?0x20:0x1f);result.bytes.push_back(static_cast<std::uint8_t>(row));result.bytes.push_back(static_cast<std::uint8_t>(cell));
    }else if(c>=0x20&&c<=0x7e){if(!alnum){result.bytes.push_back(0x0e);alnum=true;}result.bytes.push_back(c);}else{if(alnum){result.bytes.push_back(0x0f);alnum=false;}result.bytes.push_back(0x22);result.bytes.push_back(0x2e);result.replaced=true;}}
  if(alnum)result.bytes.push_back(0x0f);return result;
}
}
