// Differential regression test host: deterministic MIDI + parameter/state changes.
// Compare float captures from two separately loaded release DLLs.
#include "mpc_vst2_abi.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static int sample_rate=44100;
static VstIntPtr host(AEffect*,VstInt32 op,VstInt32,VstIntPtr,void*,float) {
    if(op==audioMasterVersion) return 2400;
    if(op==audioMasterGetSampleRate) return sample_rate;
    if(op==audioMasterGetBlockSize) return 512;
    return 0;
}
int main(int argc,char** argv) {
    if(argc<4) return 2;sample_rate=std::atoi(argv[3]);
    auto lib=LoadLibraryA(argv[1]);if(!lib) return 3;
    auto entry=reinterpret_cast<AEffect*(*)(audioMasterCallback)>(GetProcAddress(lib,"VSTPluginMain"));
    if(!entry) return 4;auto e=entry(host);if(!e) return 5;
    auto call=[&](int op,intptr_t val=0,void* ptr=nullptr,float opt=0){return e->dispatcher(e,op,0,val,ptr,opt);};
    auto set=[&](int index,float value){e->setParameter(e,index,value);};
    call(effOpen);call(effSetSampleRate,0,nullptr,float(sample_rate));call(effSetBlockSize,512);
    call(effSetProgram);set(0,24.f/54);set(1,7.f/31);set(2,1);set(3,1);call(effMainsChanged,1);
    FILE* f=std::fopen(argv[2],"wb");if(!f) return 6;
    struct Events {int32_t n;intptr_t reserved;VstEvent* events[40];} ev{};
    VstMidiEvent midi[40]{};
    auto send=[&](int status,int a,int b,int offset){
        auto& m=midi[ev.n];m={};m.type=kVstMidiType;m.byteSize=sizeof(m);m.deltaFrames=offset;
        m.midiData[0]=char(status);m.midiData[1]=char(a);m.midiData[2]=char(b);ev.events[ev.n++]=reinterpret_cast<VstEvent*>(&m);
    };
    const int pitches[]={48,55,60,64,67,72};
    std::vector<uint8_t> state;
    unsigned rng=0x2734127;
    auto random=[&](){rng=rng*1664525u+1013904223u;return float(rng>>8)/16777215.f;};
    float left[512]{},right[512]{};float* out[]={left,right};size_t total=0;double energy=0;float peak=0;
    for(int stage=0;stage<24;++stage) {
        for(int b=0;b<160;++b) {
            const int sizes[]={1,17,32,64,96,128,192,256,512};
            int block=stage<2 ? 128:sizes[b%9];ev.n=0;
            if(b==0) {
                if(stage==0 || stage==3 || stage==8 || stage==13 || stage==22)
                    for(int i=0;i<6;++i) send(0x90+(i%2),pitches[i],83,block/2);
                if(stage==1) {send(0x80,48,0,block/2);send(0x81,55,0,block/2);send(0xb1,64,127,block/2);}
                if(stage==2) {send(0xb0,123,0,0);send(0xb1,64,0,0);send(0xb1,123,0,0);}
                if(stage==3) {set(2,0);set(3,0);}
                if(stage==4) {set(2,1.f/7);set(12,0);}
                if(stage==5) {set(13,1.f/3);set(14,.2f);set(4,.7f);}
                if(stage==6) {set(13,2.f/3);set(14,.8f);}
                if(stage==7) {set(13,1);set(14,1);}
                if(stage==8) {set(13,0);set(2,1);set(3,1);set(12,1);}
                if(stage==12) {call(effMainsChanged,0);call(effMainsChanged,1);}
                if(stage>=14 && stage<=16) call(effSetProgram,stage-13);
                if(stage==17) {set(1,5.f/31);set(15,1);}
                if(stage==18) set(1,1);
                if(stage==19) {set(15,0);send(0x9f,36,100,0);}
                if(stage==21) {void* chunk=nullptr;auto n=call(effGetChunk,0,&chunk);if(n<=0) return 8;
                    state.assign(static_cast<uint8_t*>(chunk),static_cast<uint8_t*>(chunk)+n);call(effSetProgram,0);}
                if(stage==23) {send(0xb0,120,0,0);send(0xbf,120,0,0);set(15,0);}
            }
            if(stage==9) {int bend=(b*101)%16384;send(0xe0,bend&127,bend>>7,block/2);}
            if(stage==10) {set(3,float(b%3)/2);set(2,float(b%8)/7);}
            if(stage==11 && b%4==0) {set(1,float(b%32)/31);send(0x90+b%2,24+b%96,1+b%126,block/2);}
            if(stage==20 && b%4==0) {for(int p=1;p<15;++p) set(p,random());send(0x90+b%2,30+b%80,93,block/2);}
            if(stage==21 && b==64) call(effSetChunk,state.size(),state.data());
            // Hold master output down consistently across built-in program changes.
            set(0,24.f/54);
            if(ev.n) call(effProcessEvents,0,&ev);
            e->processReplacing(e,nullptr,out,block);
            for(int i=0;i<block;++i) {if(!std::isfinite(left[i]) || !std::isfinite(right[i])) return 9;
                peak=std::max(peak,std::max(std::abs(left[i]),std::abs(right[i])));energy+=double(left[i])*left[i]+double(right[i])*right[i];}
            std::fwrite(left,sizeof(float),block,f);std::fwrite(right,sizeof(float),block,f);total+=2*block;
        }
    }
    std::fclose(f);call(effMainsChanged,0);call(effClose);FreeLibrary(lib);
    std::printf("rate=%d samples=%zu peak=%g rms=%g\n",sample_rate,total,peak,std::sqrt(energy/total));
    return peak>1e-5f && peak<1 ? 0:10;
}
