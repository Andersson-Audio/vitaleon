// SPDX-License-Identifier: GPL-3.0-or-later
// Andersson Audio, 2026-11-13. Portable helpers for upstream Vital asset import.
#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <algorithm>
#include "engine/json/json.h"
#include "engine/utils.h"

class String {
 public:
  String(const char* s=""):text_(s){}
  String(std::string s):text_(std::move(s)){}
  const std::string& toStdString()const{return text_;}
 private:std::string text_;
};
class MemoryOutputStream {
 public:
  explicit MemoryOutputStream(size_t reserve=0){if(reserve>64*1024*1024)throw std::runtime_error("Asset too large");bytes_.reserve(reserve);}
  const void* getData()const{return bytes_.data();}
  size_t getDataSize()const{return bytes_.size();}
  std::vector<uint8_t> bytes_;
};
struct Base64 {
 static String toBase64(const void* ptr,size_t size){
  static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const auto* p=static_cast<const uint8_t*>(ptr);std::string s;s.reserve((size+2)/3*4);
  for(size_t i=0;i<size;i+=3){uint32_t n=uint32_t(p[i])<<16;if(i+1<size)n|=uint32_t(p[i+1])<<8;if(i+2<size)n|=p[i+2];
   s+=alphabet[(n>>18)&63];s+=alphabet[(n>>12)&63];s+=i+1<size?alphabet[(n>>6)&63]:'=';s+=i+2<size?alphabet[n&63]:'=';}
  return String(std::move(s));
 }
 static bool convertFromBase64(MemoryOutputStream& out,const std::string& s){
  if(s.size()>96*1024*1024)throw std::runtime_error("Encoded asset too large");
  uint32_t value=0;int bits=0;bool padding=false;
  for(unsigned char c:s){
   if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;
   if(c=='='){padding=true;continue;}if(padding)throw std::runtime_error("Invalid base64 padding");
   int d=c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;
   if(d<0)throw std::runtime_error("Invalid base64 asset");value=(value<<6)|uint32_t(d);bits+=6;
   if(bits>=8){bits-=8;out.bytes_.push_back(uint8_t(value>>bits));}
  }return true;
 }
};
namespace ProjectInfo { inline constexpr const char* versionString="1.0.6"; }
struct LoadSave {
 static void convertBufferToPcm(nlohmann::json& data,const std::string& field){
  if(!data.count(field))return;MemoryOutputStream b;Base64::convertFromBase64(b,data[field].get<std::string>());
  size_t n=b.getDataSize()/sizeof(float);std::vector<float> f(n);std::memcpy(f.data(),b.getData(),n*sizeof(float));std::vector<int16_t> p(n);
  vital::utils::floatToPcmData(p.data(),f.data(),int(n));data[field]=Base64::toBase64(p.data(),n*sizeof(int16_t)).toStdString();
 }
 static void convertPcmToFloatBuffer(nlohmann::json& data,const std::string& field){
  if(!data.count(field))return;MemoryOutputStream b;Base64::convertFromBase64(b,data[field].get<std::string>());
  size_t n=b.getDataSize()/sizeof(int16_t);std::vector<int16_t> p(n);std::memcpy(p.data(),b.getData(),n*sizeof(int16_t));std::vector<float> f(n);
  vital::utils::pcmToFloatData(f.data(),p.data(),int(n));data[field]=Base64::toBase64(f.data(),n*sizeof(float)).toStdString();
 }
 static int compareVersionStrings(const std::string& a,const std::string& b){
  std::istringstream sa(a),sb(b);std::string aa,bb;
  for(int i=0;i<4;++i){int x=std::getline(sa,aa,'.')?std::atoi(aa.c_str()):0;int y=std::getline(sb,bb,'.')?std::atoi(bb.c_str()):0;if(x!=y)return x<y?-1:1;}return 0;
 }
};
