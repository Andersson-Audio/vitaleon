// SPDX-License-Identifier: GPL-3.0-or-later
// Device measurement, compiled only with VITALEON_HOSTLOG and never part of a release.
//
// Question (user on the device, 0.3.46): the Q-Link square does not move when the Q-Link
// button of the MPC is pressed. The square follows the plugin's own 'UI Bank'; nothing in the
// SDK's documents says that the MPC tells a plugin which group of knobs is active. This build
// writes down what the MPC calls, so that a press of the button can be looked for in it.
//
// Every tenth of a second with any call becomes one line of Content/hostlog.txt:
//   t      milliseconds since the plugin was created
//   bank   the plugin's 'UI Bank', page / sec: the page and the section on screen
//   G S    the Q-Link slots (bit n = slot n+1, hexadecimal) whose value the MPC read / wrote
//   D N L P the slots whose value text / name / unit / properties it asked for
//   og os ot  the same calls for all other parameters, counted
//   calls  every other call: opcode:index:value
//   us     microseconds the plugin spent answering in that tenth of a second (0.3.54)
// With VITALEON_AUTOBENCH as well (side-by-side test copies, 0.3.54): 12 seconds after the
// MPC last read every value, the copy changes its own page six times and writes how long the
// MPC took each time until it read every value again: lines "AUTO page=.. ms=..".
//   rss swap  resident memory and swapped memory of the MPC process, avail: memory the
//          system can still hand out, all in MB (0.3.54: every page of the plugin became
//          slow on the device; this says whether memory is short)
// A line that repeats the one before is counted, not written again.
#ifdef VITALEON_HOSTLOG
namespace vitaleon_hostlog{
inline std::atomic<int> knobFirst{-1};
inline std::atomic<uint32_t> readMask{0},writeMask{0},displays{0},names{0},labels{0},props{0};
inline std::atomic<unsigned> otherGets{0},otherSets{0},otherTexts{0},callCount{0};
inline std::array<std::atomic<uint64_t>,24> calls{};
inline std::atomic<uint64_t> spentUs{0};
inline long long benchRestMs=0,benchAskedMs=0,benchNextMs=0;inline int benchStep=-1,benchPage=0,benchExtra=0;inline std::vector<int> benchTimes;inline std::string benchWhat;
struct Spent{std::chrono::steady_clock::time_point from=std::chrono::steady_clock::now();
    ~Spent(){spentUs.fetch_add(uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-from).count()),std::memory_order_relaxed);}};
// A number of kB from a file of /proc, in MB; -1 where there is no such file.
inline long procMb(const char* file,const char* key){std::ifstream in(file);std::string line;const size_t n=std::strlen(key);
    while(std::getline(in,line))if(line.compare(0,n,key)==0)return std::atol(line.c_str()+n)/1024;return -1;}
inline VstIntPtr (*dispatch)(AEffect*,VstInt32,VstInt32,VstIntPtr,void*,float)=nullptr;
inline void (*setter)(AEffect*,VstInt32,float)=nullptr;
inline float (*getter)(AEffect*,VstInt32)=nullptr;
inline bool slot(int index,std::atomic<uint32_t>& mask){
    const int first=knobFirst.load(std::memory_order_relaxed);if(first<0||index<first||index>=first+16)return false;
    mask.fetch_or(1u<<(index-first),std::memory_order_relaxed);return true;
}
inline VstIntPtr onDispatch(AEffect* e,VstInt32 opcode,VstInt32 index,VstIntPtr value,void* ptr,float opt){
    Spent spent;
    switch(opcode){
        case effGetParamLabel:if(!slot(index,labels))otherTexts.fetch_add(1,std::memory_order_relaxed);break;
        case effGetParamDisplay:if(!slot(index,displays))otherTexts.fetch_add(1,std::memory_order_relaxed);break;
        case effGetParamName:if(!slot(index,names))otherTexts.fetch_add(1,std::memory_order_relaxed);break;
        case effCanBeAutomated:case effGetParameterProperties:if(!slot(index,props))otherTexts.fetch_add(1,std::memory_order_relaxed);break;
        case effEditIdle:case effProcessEvents:break;   // the idle call and MIDI from the audio thread
        default:{const unsigned at=callCount.fetch_add(1,std::memory_order_relaxed);
            if(at<calls.size())calls[at].store((uint64_t(uint16_t(opcode))<<48)|(uint64_t(uint16_t(index))<<32)|uint64_t(uint32_t(value)),std::memory_order_relaxed);}
    }
    return dispatch(e,opcode,index,value,ptr,opt);
}
inline void onSet(AEffect* e,VstInt32 index,float value){Spent spent;if(!slot(index,writeMask))otherSets.fetch_add(1,std::memory_order_relaxed);setter(e,index,value);}
inline float onGet(AEffect* e,VstInt32 index){Spent spent;if(!slot(index,readMask))otherGets.fetch_add(1,std::memory_order_relaxed);return getter(e,index);}
// Called once, before the host sees the effect.
inline void install(AEffect* e){
    if(!e||dispatch)return;
    dispatch=e->dispatcher;setter=e->setParameter;getter=e->getParameter;
    e->dispatcher=&onDispatch;e->setParameter=&onSet;e->getParameter=&onGet;
}
}
// On the service thread, every two milliseconds while nothing else is to do.
inline void Vitaleon::hostLogService(){
    using namespace vitaleon_hostlog;
    const long long now=nowMs();
    if(!hostLogStart_){hostLogStart_=now;hostLogBucket_=now;knobFirst.store(uiKnobs_);
        hostLog_="Vitaleon host log. Memory at start: rss "+std::to_string(procMb("/proc/self/status","VmRSS:"))+" MB, swap "+std::to_string(procMb("/proc/self/status","VmSwap:"))+" MB, available "+std::to_string(procMb("/proc/meminfo","MemAvailable:"))+" MB of "+std::to_string(procMb("/proc/meminfo","MemTotal:"))+" MB. Slots: bit 0 = UI Control 1 (Q-Link 13) ... bit 15 = UI Control 16 (Q-Link 4). First slot parameter "+std::to_string(uiKnobs_)+" of "+std::to_string(numParams())+".\n";hostLogDirty_=true;}
    if(now-hostLogBucket_>=100){
        hostLogBucket_=now;
        const uint32_t g=readMask.exchange(0),s=writeMask.exchange(0),d=displays.exchange(0),n=names.exchange(0),l=labels.exchange(0),p=props.exchange(0);
        const unsigned og=otherGets.exchange(0),os=otherSets.exchange(0),ot=otherTexts.exchange(0),count=std::min<unsigned>(callCount.exchange(0),unsigned(calls.size()));
        char text[160];std::snprintf(text,sizeof text,"bank=%d page=%d sec=%d G=%04x S=%04x D=%04x N=%04x L=%04x P=%04x og=%u os=%u ot=%u",
            uiBank_>=0?getInt(uiBank_):-1,getInt(uiPage_),getInt(uiSection_),unsigned(g),unsigned(s),unsigned(d),unsigned(n),unsigned(l),unsigned(p),og,os,ot);
        std::string line=text;const uint64_t us=spentUs.exchange(0);
        if(count){line+=" calls=";for(unsigned i=0;i<count;++i){const uint64_t c=calls[i].load();line+=(i?",":"")+std::to_string(int(c>>48))+":"+std::to_string(int((c>>32)&0xffff))+":"+std::to_string(int32_t(uint32_t(c)));}}
        const bool any=g||s||d||n||l||p||og||os||ot||count;
#ifdef VITALEON_AUTOBENCH
        // Steps 0 to 5 change the page; 6 opens the page of wavetables for oscillator 1, 7 and 8
        // turn it to the next page and back (0.3.56). A redraw counts once: 'redraws' says how
        // many the MPC made within the three seconds after the step before.
        {static const int pages[6]={40,8,16,24,0,3};const bool refresh=og>=1000;   // the MPC read (nearly) every value: a whole redraw
         if(refresh&&!benchAskedMs&&benchStep>0&&benchStep<=9)++benchExtra;
         if(benchStep<0){if(refresh)benchRestMs=now;if(benchRestMs&&now-benchRestMs>=12000){benchStep=0;benchNextMs=now;hostLog_+="AUTO start: "+std::to_string(numParams())+" parameters, "+std::to_string(surfaceViews_.size())+" pages\n";}}
         else if(benchStep<=9){
            if(benchAskedMs&&refresh&&now-benchAskedMs>=150){if(benchStep<=6)benchTimes.push_back(int(now-benchAskedMs));
                hostLog_+="AUTO "+benchWhat+" ms="+std::to_string(now-benchAskedMs)+(benchStep>6?" prepared in "+std::to_string(tablePageMs_.load())+" ms, rows "+std::to_string([&]{int n=0;for(int r=0;r<kTableRows;++r)n+=getInt(tableRowOn_+r);return n;}()):std::string())+"\n";
                benchAskedMs=0;benchNextMs=now+3000;hostLogDirty_=true;}
            else if(benchAskedMs&&now-benchAskedMs>20000){hostLog_+="AUTO "+benchWhat+" no redraw within 20 s\n";benchAskedMs=0;benchNextMs=now+2000;hostLogDirty_=true;}
            if(!benchAskedMs&&now>=benchNextMs){
                if(benchStep>0){hostLog_+="AUTO   redraws after "+benchWhat+": "+std::to_string(1+benchExtra)+"\n";benchExtra=0;}
                if(benchStep<6){benchPage=std::min(pages[benchStep++],int(surfaceViews_.size())-1);benchWhat="page="+std::to_string(benchPage);benchAskedMs=now;navigateTo(benchPage,0);}
                else if(benchStep==6){++benchStep;benchWhat="wavetables open";benchAskedMs=now;set(uiTables_,1);}
                else if(benchStep==7){++benchStep;benchWhat="wavetables next page";benchAskedMs=now;set(tablePageStep_,2);}
                else if(benchStep==8){++benchStep;benchWhat="wavetables page back";benchAskedMs=now;set(tablePageStep_,1);}
                else{std::sort(benchTimes.begin(),benchTimes.end());std::string all;for(int v:benchTimes)all+=" "+std::to_string(v);
                    hostLog_+="AUTO DONE median of the six page changes="+std::to_string(benchTimes.empty()?-1:benchTimes[benchTimes.size()/2])+" ms, sorted:"+all+"\n";benchStep=10;hostLogDirty_=true;}}}}
#endif
        if(any){char more[120];std::snprintf(more,sizeof more," us=%llu rss=%ld swap=%ld avail=%ld",static_cast<unsigned long long>(us),
            procMb("/proc/self/status","VmRSS:"),procMb("/proc/self/status","VmSwap:"),procMb("/proc/meminfo","MemAvailable:"));line+=more;}
        const std::string state=std::string(text).substr(0,std::string(text).find(" G="));   // bank, page, section
        if(any||state!=hostLogState_){
            hostLogState_=state;
            if(line==hostLogLast_)++hostLogRepeat_;
            else{if(hostLogRepeat_){hostLog_+="  (the same "+std::to_string(hostLogRepeat_)+" more times, until t="+std::to_string(hostLogLastMs_)+")\n";hostLogRepeat_=0;}
                hostLog_+="t="+std::to_string(now-hostLogStart_)+" "+line+"\n";hostLogLast_=line;}
            hostLogLastMs_=now-hostLogStart_;hostLogDirty_=true;}
    }
    if(hostLogDirty_&&now-hostLogWritten_>=1000&&hostLog_.size()<2000000){
        hostLogWritten_=now;hostLogDirty_=false;
        std::string all=hostLog_;if(hostLogRepeat_)all+="  (the same "+std::to_string(hostLogRepeat_)+" more times, until t="+std::to_string(hostLogLastMs_)+")\n";
        // A measuring copy is installed without a Content folder: make it, or nothing is written
        // (the first round of the five copies of 0.3.54 was lost to that).
        {std::error_code made;std::filesystem::create_directories(std::filesystem::path(mpc::contentPath("hostlog.txt")).parent_path(),made);}
        std::ofstream out(mpc::contentPath("hostlog.txt"),std::ios::binary|std::ios::trunc);out<<all;
    }
}
#endif
