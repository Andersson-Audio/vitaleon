// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only ZIP/Vitalbank import; nothing is extracted into the filesystem.
#pragma once
#include "miniz/miniz_tinfl.h"
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <array>
class BankArchive {
public:
 struct Entry {std::string name;uint32_t offset,compressed,size,crc;uint16_t method,flags;};
 static uint16_t u16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
 static uint32_t u32(const uint8_t* p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
 static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
 explicit BankArchive(const std::string& path):input_(path,std::ios::binary){
  check(bool(input_),"Cannot open bank");input_.seekg(0,std::ios::end);size_=uint64_t(input_.tellg());check(size_>=22&&size_<=2147483648ULL,"Bank size unsupported");
  size_t tailSize=size_t(std::min<uint64_t>(65557,size_));auto tail=read(size_-tailSize,tailSize);size_t end=tail.size();
  for(size_t p=tail.size()-22;;--p){if(u32(tail.data()+p)==0x06054b50&&p+22+u16(tail.data()+p+20)==tail.size()){end=p;break;}if(p==0)break;}
  check(end!=tail.size(),"ZIP directory missing");const auto* e=tail.data()+end;
  check(u16(e+4)==0&&u16(e+6)==0&&u16(e+8)==u16(e+10),"Split banks unsupported");
  unsigned count=u16(e+10);uint32_t bytes=u32(e+12),offset=u32(e+16);check(count<=8192&&bytes<=8*1024*1024&&uint64_t(offset)+bytes<=size_-tailSize+end,"Invalid bank directory");
  auto cd=read(offset,bytes);size_t p=0;
  for(unsigned i=0;i<count;++i){check(p+46<=cd.size()&&u32(cd.data()+p)==0x02014b50,"Truncated bank directory");const auto* h=cd.data()+p;
   size_t n=u16(h+28),extra=u16(h+30),comment=u16(h+32);check(n>0&&n<=1024&&p+46+n+extra+comment<=cd.size(),"Invalid bank filename");
   Entry entry{std::string(reinterpret_cast<const char*>(h+46),n),u32(h+42),u32(h+20),u32(h+24),u32(h+16),u16(h+10),u16(h+8)};
   check(entry.name.find('\0')==std::string::npos,"Invalid bank filename");entries_.push_back(std::move(entry));p+=46+n+extra+comment;
  }
 }
 const std::vector<Entry>& entries()const{return entries_;}
 std::vector<uint8_t> extract(const std::string& name){
  const Entry* found=nullptr;for(const auto& e:entries_)if(e.name==name){check(found==nullptr,"Duplicate bank entry");found=&e;}check(found!=nullptr,"Bank entry missing");const auto& e=*found;
  check((e.flags&1)==0&&(e.method==0||e.method==8),"Encrypted/unsupported bank entry");check(e.size<=64*1024*1024&&e.compressed<=64*1024*1024,"Bank entry too large");
  auto header=read(e.offset,30);check(u32(header.data())==0x04034b50&&u16(header.data()+8)==e.method,"Invalid bank entry header");
  uint64_t start=uint64_t(e.offset)+30+u16(header.data()+26)+u16(header.data()+28);auto compressed=read(start,e.compressed);std::vector<uint8_t> output(e.size);
  if(e.method==0){check(e.compressed==e.size,"Invalid stored entry");output=std::move(compressed);}
  else check(tinfl_decompress_mem_to_mem(output.data(),output.size(),compressed.data(),compressed.size(),0)==output.size(),"Bank decompression failed");
  static const auto crcTable=[] {std::array<uint32_t,256> table{};for(uint32_t n=0;n<256;++n){uint32_t c=n;for(int k=0;k<8;++k)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));table[n]=c;}return table;}();
  uint32_t crc=0xffffffff;for(uint8_t b:output)crc=(crc>>8)^crcTable[(crc^b)&255];
  check((crc^0xffffffff)==e.crc,"Bank CRC mismatch");return output;
 }
private:
 std::ifstream input_;uint64_t size_=0;std::vector<Entry> entries_;
 std::vector<uint8_t> read(uint64_t offset,size_t count){check(offset<=size_&&count<=size_-offset,"Bank offset outside file");std::vector<uint8_t> b(count);input_.seekg(std::streamoff(offset));if(count)input_.read(reinterpret_cast<char*>(b.data()),std::streamsize(count));check(bool(input_),"Cannot read bank");return b;}
};
