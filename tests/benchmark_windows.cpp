// Windows VST2 A/B performance harness. Run each DLL in a fresh process.
#include "mpc_vst2_abi.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
static int block = 128;
static VstIntPtr host(AEffect*, VstInt32 op, VstInt32, VstIntPtr, void*, float) {
    if(op==audioMasterVersion) return 2400;
    if(op==audioMasterGetSampleRate) return 44100;
    if(op==audioMasterGetBlockSize) return block;
    return 0;
}
int main(int argc, char** argv) {
    if(argc<5) return 2;
    auto lib=LoadLibraryA(argv[1]); if(!lib) return 3;
    auto entry=reinterpret_cast<AEffect* (*)(audioMasterCallback)>(GetProcAddress(lib,"VSTPluginMain"));
    if(!entry) return 4;
    auto e=entry(host); if(!e || e->magic!=kEffectMagic) return 5;
    int count=std::atoi(argv[2]), unison=std::atoi(argv[3]), spectral=std::atoi(argv[4]);
    if(argc>5) block=std::atoi(argv[5]);
    e->dispatcher(e,effOpen,0,0,nullptr,0);
    e->dispatcher(e,effSetSampleRate,0,0,nullptr,44100);
    e->dispatcher(e,effSetBlockSize,0,block,nullptr,0);
    e->dispatcher(e,effSetProgram,0,0,nullptr,0);
    e->setParameter(e,0,(-36.f+60.f)/54.f);
    e->setParameter(e,1,7.f/31.f); // polyphony limit 8, not held-note count
    e->setParameter(e,2,float(unison-1)/7);
    e->setParameter(e,3,1); // 3 oscillators
    e->setParameter(e,13,float(spectral)/3);
    e->dispatcher(e,effMainsChanged,0,1,nullptr,0);
    struct Events {int32_t n; intptr_t reserved; VstEvent* events[8];} events{};
    VstMidiEvent notes[8]{};
    const int pitches[]={48,55,60,64,67,72,76,79};
    for(int i=0;i<count;++i) {notes[i].type=kVstMidiType; notes[i].byteSize=sizeof(VstMidiEvent);
        notes[i].midiData[0]=char(0x90);notes[i].midiData[1]=char(pitches[i]);notes[i].midiData[2]=83;
        events.events[i]=reinterpret_cast<VstEvent*>(&notes[i]);}
    events.n=count;
    e->dispatcher(e,effProcessEvents,0,0,&events,0);
    std::vector<float> l(block),r(block);float* out[]={l.data(),r.data()};
    // Time note-on separately, then warm up before steady-state timings.
    auto t=std::chrono::steady_clock::now();e->processReplacing(e,nullptr,out,block);
    double onset=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();
    for(int i=0;i<200;++i) e->processReplacing(e,nullptr,out,block);
    std::vector<double> times(3000);double total=0, energy=0;float peak=0;
    FILE* capture=argc>6 ? std::fopen(argv[6],"wb") : nullptr;
    if(argc>6 && !capture) return 8;
    for(auto& dt:times) {
        t=std::chrono::steady_clock::now(); e->processReplacing(e,nullptr,out,block);
        dt=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();total+=dt;
        if(capture) {std::fwrite(l.data(),sizeof(float),block,capture);std::fwrite(r.data(),sizeof(float),block,capture);}
        for(int i=0;i<block;++i) {if(!std::isfinite(l[i]) || !std::isfinite(r[i])) return 6;
            peak=std::max(peak,std::max(std::abs(l[i]),std::abs(r[i])));energy+=double(l[i])*l[i]+double(r[i])*r[i];}
    }
    std::sort(times.begin(),times.end());
    if(capture) std::fclose(capture);
    std::printf("%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.7f,%.7f\n",e->version,count,unison,spectral,block,
        total/times.size(),times[times.size()/2],times[2969],onset,peak,std::sqrt(energy/(2*block*times.size())));
    e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);
    FreeLibrary(lib);return peak>1e-6f && peak<1 ? 0:7;
}
