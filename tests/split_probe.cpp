// SPDX-License-Identifier: GPL-3.0-or-later
// Compares the two-thread effect pipeline with single-thread rendering:
// the split output must equal the single-thread output delayed by 128 samples.
// usage: split_probe <block> <preset.vital>...
#define VITALEON_TEST 1
#include "../src/Vitaleon.cpp"
#include <iostream>
MPC_PLUGIN_ENTRY(Vitaleon)
static VstIntPtr host(AEffect*,VstInt32 opcode,VstInt32,VstIntPtr,void*,float){return opcode==audioMasterVersion?2400:0;}
struct Run{std::vector<float> left;double micros=0;bool split=false;int latency=0;};
static Run render(const char* file,bool split,int block,int blocks){
 Run run;AEffect* e=VSTPluginMain(host);auto* synth=static_cast<Vitaleon*>(e->object);
 e->dispatcher(e,effOpen,0,0,nullptr,0);e->dispatcher(e,effSetSampleRate,0,0,nullptr,44100);e->dispatcher(e,effSetBlockSize,0,block,nullptr,0);e->dispatcher(e,effMainsChanged,0,1,nullptr,0);
 nlohmann::json preset;std::ifstream in(file);in>>preset;synth->testImport(preset);synth->testSplit(split);run.split=synth->testSplitActive();run.latency=e->initialDelay;
 std::vector<float> l(block),r(block);float* out[]={l.data(),r.data()};
 struct Ev{int32_t n;intptr_t reserved;VstEvent* events[3];};VstMidiEvent m[3]{};Ev ev{};
 for(int i=0;i<3;++i){m[i].type=kVstMidiType;m[i].byteSize=sizeof(VstMidiEvent);m[i].midiData[0]=char(0x90);m[i].midiData[1]=char(48+i*7);m[i].midiData[2]=100;ev.events[i]=reinterpret_cast<VstEvent*>(&m[i]);}ev.n=3;
 e->dispatcher(e,effProcessEvents,0,0,&ev,0);
 for(int b=0;b<blocks;++b){
  if(b==blocks/2){for(auto& x:m)x.midiData[0]=char(0x80);e->dispatcher(e,effProcessEvents,0,0,&ev,0);}
  auto t=std::chrono::steady_clock::now();e->processReplacing(e,nullptr,out,block);
  run.micros+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();
  run.left.insert(run.left.end(),l.begin(),l.end());
  // Pace like a host so the worker runs between callbacks, as on the device.
  std::this_thread::sleep_for(std::chrono::microseconds(300));
 }
 run.micros/=blocks;e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);return run;
}
int main(int argc,char** argv){
 if(argc<3)return 2;int block=std::atoi(argv[1]);int failures=0;
 for(int a=2;a<argc;++a){
  Run single=render(argv[a],false,block,600),again=render(argv[a],false,block,600),split=render(argv[a],true,block,600);
  double repeat=0;for(size_t i=0;i<single.left.size();++i)repeat=std::max(repeat,double(std::abs(single.left[i]-again.left[i])));
  double worst=0,energy=0;size_t n=single.left.size();
  for(size_t i=128;i<n;++i){worst=std::max(worst,double(std::abs(split.left[i]-single.left[i-128])));energy+=double(single.left[i])*single.left[i];}
  bool silentStart=true;for(int i=0;i<128;++i)silentStart&=split.left[size_t(i)]==0;
  bool ok=split.split&&!single.split&&worst<1e-6&&silentStart&&energy>0;failures+=!ok;
  std::printf("%s %-22s single %.1f us  split %.1f us (audio thread)  max diff %.2e  (two single runs differ by %.2e)  latency %d  %s\n",ok?"PASS":"FAIL",std::filesystem::path(argv[a]).stem().string().c_str(),single.micros,split.micros,worst,repeat,split.latency,split.split?"split active":"NOT split");
 }
 return failures?1:0;
}
