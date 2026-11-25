// SPDX-License-Identifier: GPL-3.0-or-later
// The two pictures on the FILTERS page (0.3.30).
//
// Each filter has a private copy of the engine's filter module that never carries audio.
// When a control of the filter changed while the page is on screen, the service thread
// sends one impulse through that copy and reads the level at kFilterColumns frequencies
// from 20 Hz to 20 kHz, once the control has rested for kFilterRestMs (not while it moves,
// see refreshFilterPicture). The picture therefore shows what the filter does at the values of
// its controls, for every model and style, and not an estimate of it. Not part of the
// picture: modulation, key tracking, the mix and the on switch.
//
// Order of one measurement (tested against all eight models on the desktop): set the
// controls, two blocks so that the model and style are switched in, hard reset, two blocks
// for the coefficients, then the impulse. With the reset after the switch the result does
// not depend on what the copy measured before. The cutoff and the formant position are
// smoothed controls in the engine; here they are set at once.
inline void Vitaleon::createFilterProbes(){
    static const char* followed[]={"cutoff","resonance","drive","blend","blend_transpose","style","model",
        "formant_x","formant_y","formant_transpose","formant_resonance","formant_spread"};
    for(int f=0;f<2;++f){
        const std::string prefix="filter_"+std::to_string(f+1);
        auto probe=std::make_unique<FilterProbe>(prefix);
        for(int i=0;i<vital::kMaxBufferSize;++i){probe->audio.buffer[i]=0;probe->keytrack.buffer[i]=0;probe->midi.buffer[i]=0;}
        probe->module.plug(&probe->audio,vital::FilterModule::kAudio);
        probe->module.plug(&probe->keytrack,vital::FilterModule::kKeytrack);
        probe->module.plug(&probe->midi,vital::FilterModule::kMidi);
        probe->module.init();probe->module.setSampleRate(kFilterProbeRate);
        auto controls=probe->module.getControls();
        for(const char* key:followed){
            const int index=controlIndex(prefix+"_"+key);auto found=controls.find(prefix+"_"+key);
            if(index<0||found==controls.end())continue;
            probe->follow.push_back({index,found->second,dynamic_cast<vital::SmoothValue*>(found->second)});
        }
        auto fix=[&](const char* key,float value){auto found=controls.find(prefix+"_"+key);if(found!=controls.end())found->second->set(value);};
        fix("on",1);fix("mix",1);fix("keytrack",0);
        probe->response.assign(size_t(kFilterProbeLength),0.0f);
        filterProbes_[size_t(f)]=std::move(probe);
    }
}
// Service thread, no lock held. Parameter reads are atomic; the copies belong to this thread.
inline void Vitaleon::refreshFilterPicture(){
    if(filtersView_<0||uiPage_<0||filterReadout_[0]<0||!filterProbes_[0]||getInt(uiPage_)!=filtersView_)return;
    // filterPictureNow_ (0.3.56): the page was just opened, so the picture is measured at once
    // and goes out with the redraw of the page; nothing is being turned then.
    const long long now=nowMs();if(!filterPictureNow_&&now-filterPictureMs_<kFilterPictureMs)return;
    // Twice the cosine of each column's frequency, for the single-frequency sums below.
    static const std::array<double,kFilterColumns> turn=[]{std::array<double,kFilterColumns> t{};
        for(int c=0;c<kFilterColumns;++c)t[size_t(c)]=2*std::cos(2*3.14159265358979323846*20.0*std::pow(1000.0,(c+.5)/kFilterColumns)/kFilterProbeRate);return t;}();
    const bool live=getBool(visualEnabled_);bool changed=false,measured=false;
    for(int f=0;f<2;++f){
        auto& probe=*filterProbes_[size_t(f)];
        uint64_t key=1469598103934665603ull;std::array<float,16> values{};size_t count=0;
        for(const auto& item:probe.follow){const float value=get(item.index);values[count++]=value;uint32_t bits;std::memcpy(&bits,&value,4);key=(key^bits)*1099511628211ull;}
        if(key==probe.shown)continue;
        // The picture follows once the controls of the filter rest (device, 0.3.30: with a new
        // picture ten times a second every control on the page stuttered while it was turned;
        // a display update redraws the page, as found before in 0.3.21). So nothing is measured
        // and nothing is redrawn while a value still changes.
        if(key!=probe.seen){probe.seen=key;probe.seenMs=now;}
        if(!filterPictureNow_&&now-probe.seenMs<kFilterRestMs)continue;
        for(size_t k=0;k<count;++k){const auto& item=probe.follow[k];if(item.smooth)item.smooth->setHard(values[k]);else item.value->set(values[k]);}
        const int block=vital::kMaxBufferSize;
        for(int b=0;b<2;++b)probe.module.process(block);
        probe.module.hardReset();
        for(int b=0;b<2;++b)probe.module.process(block);
        for(int n=0;n<kFilterProbeLength;n+=block){
            probe.audio.buffer[0]=n==0?kFilterImpulse:0.0f;
            probe.module.process(block);
            const vital::poly_float* out=probe.module.output()->buffer;
            for(int i=0;i<block;++i)probe.response[size_t(n+i)]=out[i][0]*(1.0f/kFilterImpulse);
        }
        probe.audio.buffer[0]=0;
        // The second half fades out, so a filter that still rings at the end leaves no floor of its own.
        const int half=kFilterProbeLength/2;
        for(int n=half;n<kFilterProbeLength;++n)probe.response[size_t(n)]*=.5f+.5f*std::cos(3.14159265f*float(n-half)/float(half));
        for(int c=0;c<kFilterColumns;++c){
            const double k=turn[size_t(c)];double s1=0,s2=0;
            for(int n=0;n<kFilterProbeLength;++n){const double s=probe.response[size_t(n)]+k*s1-s2;s2=s1;s1=s;}
            const double power=s1*s1+s2*s2-k*s1*s2;
            float db=std::isfinite(power)&&power>1e-18?float(10*std::log10(power)):kFilterFloorDb;
            const float level=mpc::clamp((db-kFilterFloorDb)/(kFilterTopDb-kFilterFloorDb),0.0f,1.0f);
            const float value=std::round(level*(kFilterLevels-1))/(kFilterLevels-1);
            if(getNormalised(filterReadout_[size_t(f)]+c)!=value){publishNormalised(filterReadout_[size_t(f)]+c,value);changed=true;}
        }
        probe.shown=key;measured=true;filterPictures_.fetch_add(1,std::memory_order_relaxed);
    }
    if(measured)filterPictureMs_=nowMs();
    if(changed){if(live)visualVersion_.fetch_add(1,std::memory_order_relaxed);else browserDisplayDirty_.store(true);}
}
