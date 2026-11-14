#include "mpc_vst2_abi.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
static int block=128;
static VstIntPtr host(AEffect*,VstInt32 op,VstInt32,VstIntPtr,void*,float){
    if(op==audioMasterVersion)return 2400;if(op==audioMasterGetSampleRate)return 44100;
    if(op==audioMasterGetBlockSize)return block;return 0;
}
int main(int argc,char** argv){
    if(argc<3)return 2;block=std::atoi(argv[2]);auto lib=LoadLibraryA(argv[1]);if(!lib)return 3;
    auto entry=reinterpret_cast<AEffect*(*)(audioMasterCallback)>(GetProcAddress(lib,"VSTPluginMain"));if(!entry)return 4;auto e=entry(host);if(!e)return 5;
    auto call=[&](int op,intptr_t value=0,void* ptr=nullptr,float opt=0){return e->dispatcher(e,op,0,value,ptr,opt);};
    call(effOpen);call(effSetSampleRate,0,nullptr,44100);call(effSetBlockSize,block);call(effSetProgram);
    e->setParameter(e,0,24.f/54);e->setParameter(e,1,7.f/31);e->setParameter(e,2,1);e->setParameter(e,3,1);
    // Release 1000 ms, normalized on the SDK's logarithmic 5..3000 ms range.
    e->setParameter(e,11,std::log(1000.f/5)/std::log(3000.f/5));call(effMainsChanged,1);
    std::vector<float> left(block),right(block);float* out[]={left.data(),right.data()};
    struct Events {int32_t n;intptr_t reserved;VstEvent* events[8];} ev{};VstMidiEvent midi[8]{};
    auto chord=[&](int status,int transpose){ev.n=8;for(int i=0;i<8;++i){auto& m=midi[i];m={};m.type=kVstMidiType;m.byteSize=sizeof(m);m.midiData[0]=char(status);m.midiData[1]=char(48+i*3+transpose);m.midiData[2]=status==0x90?83:0;ev.events[i]=reinterpret_cast<VstEvent*>(&m);}call(effProcessEvents,0,&ev);};
    auto render=[&](){e->processReplacing(e,nullptr,out,block);};
    for(int i=0;i<5;++i)render(); // Clear pending state/reset before measuring.
    FILE* capture=argc>3?std::fopen(argv[3],"wb"):nullptr;if(argc>3&&!capture)return 6;
    double before=0,after=0,onset=0;int before_n=0,after_n=0;float peak=0,max_jump=0,last=0;
    std::vector<double> times;
    for(int turn=0;turn<12;++turn){
        chord(0x90,turn%2);
        for(int b=0;b<int(std::ceil(.12*44100/block));++b){
            auto t=std::chrono::steady_clock::now();render();double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();
            if(turn>0 && b*block<.05*44100){after+=us;++after_n;times.push_back(us);if(b==0)onset+=us;}
            if(turn==0 && b*block>.07*44100){before+=us;++before_n;}
            for(int i=0;i<block;++i){if(!std::isfinite(left[i])||!std::isfinite(right[i]))return 7;peak=std::max(peak,std::abs(left[i]));max_jump=std::max(max_jump,std::abs(left[i]-last));last=left[i];}
            if(capture){std::fwrite(left.data(),sizeof(float),block,capture);std::fwrite(right.data(),sizeof(float),block,capture);}
        }
        chord(0x80,turn%2);render(); // Release tail remains when next chord starts.
    }
    if(capture)std::fclose(capture);std::sort(times.begin(),times.end());
    std::printf("%d,%d,%.3f,%.3f,%.3f,%.3f,%.7f,%.7f\n",e->version,block,before/before_n,after/after_n,onset/11,times[size_t(.99*(times.size()-1))],peak,max_jump);
    call(effMainsChanged,0);call(effClose);FreeLibrary(lib);return peak>1e-5f&&peak<1?0:8;
}
