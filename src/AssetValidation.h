// SPDX-License-Identifier: GPL-3.0-or-later
// Bounds checks at the file boundary, before calling Vital's trusted-state loaders.
#pragma once
#include "PortableAssets.h"
namespace vitaleon_assets {
using Json=nlohmann::json;
inline void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
inline double number(const Json& j){require(j.is_number(),"Expected a number");double n=j.get<double>();require(std::isfinite(n),"Non-finite asset value");return n;}
inline void curve(const Json& j){
 require(j.is_object()&&j.count("num_points")&&j.count("points")&&j.count("powers"),"Invalid curve");
 int n=int(number(j["num_points"]));require(n>=2&&n<=100,"Curve must have 2–100 points");
 require(j["points"].is_array()&&j["powers"].is_array()&&j["points"].size()>=size_t(n*2)&&j["powers"].size()>=size_t(n),"Truncated curve");
 require(j["powers"].size()<=100&&j["points"].size()>=j["powers"].size()*2,"Invalid curve storage");
 double prev=-1;for(int i=0;i<n;++i){double x=number(j["points"][2*i]),y=number(j["points"][2*i+1]),p=number(j["powers"][i]);
  require(x>=prev&&x>=0&&x<=1&&y>=0&&y<=1&&std::abs(p)<=100,"Curve point outside bounds");prev=x;}
}
inline void tree(const Json& j,int depth=0){
 require(depth<=32,"Asset nesting too deep");
 if(j.is_number()) {require(std::abs(number(j))<=1e12,"Asset number too large");return;}
 if(j.is_string()){require(j.get_ref<const std::string&>().size()<=64*1024*1024,"Asset string too large");return;}
 if(j.is_array()||j.is_object()){require(j.size()<=16384,"Asset collection too large");for(auto i=j.begin();i!=j.end();++i)tree(i.value(),depth+1);}
 if(j.is_object()&&j.count("num_points")&&(j.count("points")||j.count("powers")))curve(j);
}
inline void table(const Json& j){
 tree(j);require(j.is_object()&&j.count("groups")&&j["groups"].is_array()&&!j["groups"].empty()&&j["groups"].size()<=32,"Invalid wavetable groups");
 const std::vector<std::string> types={"Wave Source","Audio File Source","Line Source","Shepard Tone Source","Frequency Filter","Wave Folder","Wave Warp","Phase Shift","Wave Window","Slew Limiter"};
 for(const auto& g:j["groups"]){require(g.count("components")&&g["components"].is_array()&&!g["components"].empty()&&g["components"].size()<=32,"Invalid wavetable components");
  for(const auto& c:g["components"]){std::string type=c.at("type").get<std::string>();require(std::find(types.begin(),types.end(),type)!=types.end(),"Unsupported wavetable component: "+type);
   require(c.count("keyframes")&&c["keyframes"].is_array()&&!c["keyframes"].empty()&&c["keyframes"].size()<=257,"Invalid wavetable keyframes");
   double prev=-1;for(const auto& k:c["keyframes"]){double p=number(k.at("position"));require(p>=0&&p<=256&&p>=prev&&p==std::floor(p),"Invalid wavetable keyframe position");prev=p;}
   if(c.count("interpolation_style")){double style=number(c["interpolation_style"]);require(style>=0&&style<=2,"Unknown interpolation style");}
  }
 }
}
struct SampleData {std::vector<float> left,right;int rate=44100;std::string name;};
inline SampleData sample(const Json& j){
 SampleData result;int n=int(number(j.at("length")));result.rate=int(number(j.at("sample_rate")));
 require(n>=4&&n<=4194304&&result.rate>=8000&&result.rate<=384000,"Sample length/rate unsupported (maximum 4,194,304 frames)");
 auto decode=[&](const char* key,std::vector<float>& samples){MemoryOutputStream bytes;Base64::convertFromBase64(bytes,j.at(key).get<std::string>());require(bytes.getDataSize()==size_t(n)*2,"Truncated PCM sample");samples.resize(n);for(int i=0;i<n;++i){auto p=&bytes.bytes_[size_t(i)*2];samples[i]=int16_t(uint16_t(p[0])|(uint16_t(p[1])<<8))/32768.0f;}};
 decode("samples",result.left);if(j.count("samples_stereo"))decode("samples_stereo",result.right);result.name=j.value("name",std::string("Sample"));return result;
}
}
