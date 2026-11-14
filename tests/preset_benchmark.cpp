// SPDX-License-Identifier: GPL-3.0-or-later
// Desktop timing of real Vital presets through the exported VST entry point.
// usage: preset_benchmark <oversampling -1|0|1|2> <notes> <preset.vital>...
// Prints microseconds per 128-frame callback. Desktop numbers only: they rank
// costs and compare builds, they do not predict the MPC's CPU meter.
#define VITALEON_TEST 1
#include "../src/Vitaleon.cpp"
#include <iostream>
#include <sstream>
MPC_PLUGIN_ENTRY(Vitaleon)
static VstIntPtr host(AEffect*,VstInt32 opcode,VstInt32,VstIntPtr,void*,float){return opcode==audioMasterVersion?2400:0;}
int main(int argc,char** argv){
 if(argc<4)return 2;
 int oversampling=std::atoi(argv[1]),count=std::min(8,std::atoi(argv[2]));const int block=std::getenv("BENCH_BLOCK")?std::atoi(std::getenv("BENCH_BLOCK")):128;
 AEffect* e=VSTPluginMain(host);auto* synth=static_cast<Vitaleon*>(e->object);
 e->dispatcher(e,effOpen,0,0,nullptr,0);e->dispatcher(e,effSetSampleRate,0,0,nullptr,44100);e->dispatcher(e,effSetBlockSize,0,block,nullptr,0);e->dispatcher(e,effMainsChanged,0,1,nullptr,0);
 std::vector<float> l(block),r(block);float* out[]={l.data(),r.data()};
 auto settle=[&]{for(int i=0;i<20000&&synth->busy();++i){e->processReplacing(e,nullptr,out,block);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 struct Events{int32_t n;intptr_t reserved;VstEvent* events[8];};
 const int pitches[]={48,55,60,64,67,72,76,79};
 double sum=0;int files=0;
 for(int a=3;a<argc;++a){
  nlohmann::json preset;try{std::ifstream in(argv[a]);in>>preset;synth->testImport(preset);}catch(const std::exception& ex){std::cout<<"SKIP,"<<argv[a]<<","<<ex.what()<<"\n";continue;}
  int os=synth->controlIndex("oversampling");
  if(oversampling>=0){e->setParameter(e,os,synth->param(os).toNormalised(float(oversampling)));settle();}
  // BENCH_SET="key=value,key=value" overrides engine controls after the import.
  if(const char* sets=std::getenv("BENCH_SET")){std::stringstream list(sets);std::string item;while(std::getline(list,item,',')){auto eq=item.find('=');if(eq==std::string::npos)continue;int index=synth->controlIndex(item.substr(0,eq));if(index<0){std::puts(("unknown "+item).c_str());continue;}e->setParameter(e,index,synth->param(index).toNormalised(std::stof(item.substr(eq+1))));}settle();}
  auto send=[&](int status,int velocity){Events events{};VstMidiEvent notes[8]{};for(int i=0;i<count;++i){notes[i].type=kVstMidiType;notes[i].byteSize=sizeof(VstMidiEvent);notes[i].midiData[0]=char(status);notes[i].midiData[1]=char(pitches[i]);notes[i].midiData[2]=char(velocity);events.events[i]=reinterpret_cast<VstEvent*>(&notes[i]);}events.n=count;e->dispatcher(e,effProcessEvents,0,0,&events,0);};
  send(0x90,100);for(int i=0;i<(std::getenv("BENCH_WARM")?std::atoi(std::getenv("BENCH_WARM")):100);++i)e->processReplacing(e,nullptr,out,block);
  std::vector<double> times(1500);double total=0,energy=0;bool finite=true;
  for(auto& dt:times){auto t=std::chrono::steady_clock::now();e->processReplacing(e,nullptr,out,block);dt=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();total+=dt;
   for(int i=0;i<block;++i){finite&=std::isfinite(l[i])&&std::isfinite(r[i]);energy+=double(l[i])*l[i]+double(r[i])*r[i];}}
  std::sort(times.begin(),times.end());
  std::printf("%.1f,%.1f,%.1f,%d,%d,%.5f,%s\n",total/times.size(),times[times.size()/2],times[times.size()*99/100],synth->getInt(os),int(finite),std::sqrt(energy/(2.0*block*times.size())),std::filesystem::path(argv[a]).stem().string().c_str());
  sum+=total/times.size();++files;
  send(0x80,0);e->dispatcher(e,effProcessEvents,0,0,nullptr,0);
  {Events off{};VstMidiEvent cc{};cc.type=kVstMidiType;cc.byteSize=sizeof(VstMidiEvent);cc.midiData[0]=char(0xb0);cc.midiData[1]=120;off.events[0]=reinterpret_cast<VstEvent*>(&cc);off.n=1;e->dispatcher(e,effProcessEvents,0,0,&off,0);}
  for(int i=0;i<50;++i)e->processReplacing(e,nullptr,out,block);
 }
 std::printf("MEAN,%.1f,files,%d,oversampling,%d,notes,%d\n",files?sum/files:0,files,oversampling,count);
 e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);return 0;
}
