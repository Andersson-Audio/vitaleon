// SPDX-License-Identifier: GPL-3.0-or-later
// Device measurement, compiled only with VITALEON_CPUDIAG and never part of a release.
// Two seconds after the audio starts the plugin plays every installed sound itself:
// a 4-note chord and an 8-note chord, three seconds each. It books how much of every
// block the audio thread and the effect worker needed, how often the host called late,
// and writes the rows to cpu-diag.json in the plugin folder after every sound.
//
// Twin-engine trial (2026-11-18): a third run per sound plays the same chord with the
// voices split over two engines, the second one on its own thread. It answers one
// question before any real work starts: how much does the audio thread gain on the
// device? The second engine is a copy made after each load; nothing else of the plugin
// knows about it (no control changes, no displays, no note handling beyond the chord).
#ifdef VITALEON_CPUDIAG
inline long Vitaleon::cpuRssKb(){
#ifdef __linux__
    std::ifstream status("/proc/self/status");std::string line;
    while(std::getline(status,line))if(line.rfind("VmRSS:",0)==0)return std::atol(line.c_str()+6);
#endif
    return 0;
}
inline void Vitaleon::twinLoop(){
    for(;;){
        twinSignal_.wait();if(twinQuit_.load())return;
        const int frames=twinPending_.load(std::memory_order_acquire);if(frames<=0)continue;
        {mpc::ScopedDenormalsOff denormals;const auto start=std::chrono::steady_clock::now();
         engineB_->processStageA(frames);
         twinLoad_.store(float(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*preparedRate_/frames),std::memory_order_relaxed);}
        twinPending_.store(0,std::memory_order_release);
    }
}
inline void Vitaleon::twinWaitIdle(){for(int spins=0;twinPending_.load(std::memory_order_acquire)!=0;++spins)if(spins>4000)std::this_thread::yield();}
// Service thread, both engines parked: make engine B the same sound as engine A.
inline void Vitaleon::twinCopy(){
    twinWaitIdle();
    auto controls=engineB_->getControls();
    for(const auto& b:bindings_){auto found=controls.find(b.key);if(found!=controls.end())found->second->set(b.value->value());}
    // The effects run once, on the summed voices, in engine A.
    for(const char* fx:{"chorus_on","compressor_on","delay_on","distortion_on","eq_on","filter_fx_on","flanger_on","phaser_on","reverb_on"}){auto found=controls.find(fx);if(found!=controls.end())found->second->set(0);}
    twinPolyphony_=controls.at("polyphony");
    for(int o=0;o<3;++o){WavetableCreator creator(engineB_->getWavetable(o));creator.jsonToState(savedAssets_["wavetables"][size_t(o)]);}
    if(savedAssets_.count("sample")){const auto sample=vitaleon_assets::sample(savedAssets_["sample"]);auto* target=engineB_->getSample();
        if(sample.right.empty())target->loadSample(sample.left.data(),int(sample.left.size()),sample.rate);
        else target->loadSample(sample.left.data(),sample.right.data(),int(sample.left.size()),sample.rate);}
    for(int i=0;i<8;++i)engineB_->getLfoSource(i)->jsonToState(savedAssets_["lfos"][size_t(i)]);
    for(int i=0;i<64;++i)engineB_->getModulationBank().atIndex(i)->modulation_processor->lineMapGenerator()->jsonToState(savedAssets_["line_mappings"][size_t(i)]);
    for(int i=0;i<64;++i){auto& r=twinRoutes_[size_t(i)];if(r.connected){engineB_->disconnectModulation(r.change);r.connected=false;}
        auto* c=engineB_->getModulationBank().atIndex(i);c->source_name.clear();c->destination_name.clear();}
    engineB_->disableUnnecessaryModSources();
    for(int i=0;i<64;++i){const int s=getInt(routes_[size_t(i)].source),d=getInt(routes_[size_t(i)].destination);if(s==0||d==0)continue;
        auto& r=twinRoutes_[size_t(i)];auto* c=engineB_->getModulationBank().atIndex(i);c->source_name=sources_.at(size_t(s));c->destination_name=destinations_.at(size_t(d));
        auto& change=r.change;change={};change.source=engineB_->getModulationSource(c->source_name);
        change.mono_destination=engineB_->getMonoModulationDestination(c->destination_name);change.poly_destination=engineB_->getPolyModulationDestination(c->destination_name);
        change.mono_modulation_switch=engineB_->getMonoModulationSwitch(c->destination_name);change.poly_modulation_switch=engineB_->getPolyModulationSwitch(c->destination_name);
        change.destination_scale=vital::Parameters::getParameterRange(c->destination_name);change.modulation_processor=c->modulation_processor.get();
        engineB_->connectModulation(change);r.connected=true;}
    engineB_->setSampleRate(int(preparedRate_));engineB_->checkOversampling();
    // Build the per-voice graph clones here, not in the first measured block.
    engineB_->allSoundsOff();engineB_->setVoiceLimit(32);twinPolyphony_->set(32);
    for(int n=0;n<32;++n)engineB_->noteOn(36+n,.01f,0,0);
    engineB_->process(128);engineB_->allSoundsOff();engineB_->process(128);engineB_->resetLfoCycles();
    engineB_->setVoiceLimit(getInt(Voices));twinPolyphony_->set(float(getInt(Voices)));
}
// Audio thread, at the start of process() while both workers are idle.
inline void Vitaleon::cpuControl(int frames){
    if(cpuPhase_==7)return;
    asleep_=false;silentFrames_=0;cpuFrames_+=frames;
    static const int notes[8]={36,43,48,55,60,64,67,72};
    const auto seconds=[&](double s){return cpuFrames_>long(preparedRate_*s);};
    const auto begin=[&]{cpuRun_=CpuRun{};cpuRun_.guard=32;cpuFrames_=0;};
    const auto chord=[&](bool eight){for(int i=eight?0:2;i<(eight?8:6);++i)engine_->noteOn(notes[i],.8f,0,0);begin();};
    const auto finish=[&](int chordIndex){
        CpuRow row;const CpuRun& run=cpuRun_;const int n=std::max(1,run.blocks);
        row.mean=float(run.sum/n);row.peak=run.peak;row.fxMean=float(run.fxSum/n);row.fxPeak=run.fxPeak;
        row.level=float(run.levelSum/n);row.twinMean=float(run.twinSum/n);row.twinPeak=run.twinPeak;row.notesA=cpuNotesA_;row.notesB=cpuNotesB_;
        row.blocks=run.blocks;row.over=run.over;row.late=run.late;row.voices=run.voices;row.guard=run.guard;row.maxGapUs=run.maxGapUs;
        int seen=0;const int limit=int(std::ceil(run.blocks*.99));for(size_t b=0;b<run.histogram.size();++b){seen+=run.histogram[b];if(seen>=limit){row.p99=float(b+1)*.025f;break;}}
        cpuRows_[size_t(cpuSound_)][size_t(chordIndex)]=row;
        engine_->allSoundsOff();cpuFrames_=0;};
    switch(cpuPhase_){
        case 0: if(seconds(2)&&!cpuRows_.empty()){cpuSound_=0;cpuRequest_.store(0);cpuPhase_=1;} else if(seconds(2))cpuPhase_=7; break;
        case 1: if(cpuLoaded_.exchange(false)){cpuPhase_=2;cpuFrames_=0;} break;
        case 2: if(seconds(.5)){cpuNotesA_=4;cpuNotesB_=0;chord(false);cpuPhase_=3;} break;
        case 3: if(seconds(3)){finish(0);cpuPhase_=4;} break;
        case 4: if(seconds(.6)){cpuNotesA_=8;cpuNotesB_=0;chord(true);cpuPhase_=5;} break;
        case 5: if(seconds(3)){finish(1);cpuPhase_=8;} break;
        // Twin run: as many notes as the sound has voices, at most eight, in pairs.
        case 8: if(seconds(.6)){
            const int total=std::min(8,getInt(Voices));
            if(total<3||!fxSplit_.load()){cpuRows_[size_t(cpuSound_)][2]=CpuRow{};cpuPhase_=6;cpuFrames_=0;break;} // one pair: nothing to split
            cpuNotesA_=std::min(total-1,((total+1)/2+1)/2*2);cpuNotesB_=total-cpuNotesA_;
            for(int i=0;i<cpuNotesA_;++i)engine_->noteOn(notes[i],.8f,0,0);
            for(int i=0;i<cpuNotesB_;++i)engineB_->noteOn(notes[cpuNotesA_+i],.8f,0,0);
            twinActive_=true;begin();cpuPhase_=9;} break;
        case 9: if(seconds(3)){finish(2);engineB_->allSoundsOff();cpuPhase_=6;} break;
        case 6: if(seconds(.8)){
            twinActive_=false;
            cpuDone_.store(cpuSound_+1);
            if(cpuSound_+1<int(cpuRows_.size())){++cpuSound_;cpuRequest_.store(cpuSound_);cpuPhase_=1;}else cpuPhase_=7;} break;
        default: break;
    }
}
// Audio thread, after a block was rendered. ratio: share of the block time the audio
// thread needed (voices, and the waits for the workers); fx: the effect worker's share.
inline void Vitaleon::cpuMeasure(int frames,float ratio,float fx,float outputPeak,std::chrono::steady_clock::time_point started){
    const bool measuring=(cpuPhase_==3||cpuPhase_==5||cpuPhase_==9)&&cpuFrames_>long(preparedRate_*.25);
    if(measuring&&cpuLastStart_!=std::chrono::steady_clock::time_point{}){
        auto& run=cpuRun_;++run.blocks;run.sum+=ratio;run.fxSum+=fx;run.levelSum+=outputPeak;run.peak=std::max(run.peak,ratio);run.fxPeak=std::max(run.fxPeak,fx);
        if(twinActive_){const float twin=twinLoad_.load(std::memory_order_relaxed);run.twinSum+=twin;run.twinPeak=std::max(run.twinPeak,twin);}
        if(std::max(ratio,fx)>1)++run.over;
        ++run.histogram[size_t(mpc::clamp(int(std::max(ratio,fx)/.025f),0,int(run.histogram.size())-1))];
        const long gap=long(std::chrono::duration<double,std::micro>(started-cpuLastStart_).count()),nominal=long(cpuLastFrames_*1e6/preparedRate_);
        run.maxGapUs=std::max(run.maxGapUs,gap);if(gap>nominal*3/2)++run.late;
        run.voices=std::max(run.voices,engine_->getNumActiveVoices()+(twinActive_?engineB_->getNumActiveVoices():0));run.guard=std::min(run.guard,dynamicVoices_);
    }
    cpuLastStart_=started;cpuLastFrames_=frames;cpuBlock_.store(frames,std::memory_order_relaxed);
}
// Service thread, no lock held: load the next sound, copy it into the second engine,
// tell the audio thread when both are ready, write the finished rows.
inline void Vitaleon::cpuService(){
    const int request=cpuRequest_.exchange(-1);
    if(request>=0&&request<int(filePrograms_.size())){setProgram(factoryProgramCount_+request);cpuLoading_=request;return;}
    if(cpuLoading_>=0&&pendingPreset_.load()<0&&requested_.load()==completed_.load()&&gate_.load()!=2){
        {std::unique_lock<std::mutex> lifecycle(lifecycleMutex_);parkEngine();
         {std::lock_guard<std::mutex> state(stateMutex_);const auto start=std::chrono::steady_clock::now();
          try{twinCopy();}catch(const std::exception& e){cpuTwinError_=e.what();}
          cpuCopyMs_[size_t(cpuLoading_)]=float(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
         if(cpuRssAfterCopy_==0)cpuRssAfterCopy_=cpuRssKb();
         gate_.store(3);}
        cpuLoading_=-1;cpuLoaded_.store(true);}
    const int done=cpuDone_.load();if(done==cpuWritten_)return;cpuWritten_=done;
    Json rows=Json::array();
    auto row=[](const CpuRow& r){return Json{{"mean",r.mean},{"p99",r.p99},{"peak",r.peak},{"fx_mean",r.fxMean},{"fx_peak",r.fxPeak},{"level",r.level},{"twin_mean",r.twinMean},{"twin_peak",r.twinPeak},{"notes_a",r.notesA},{"notes_b",r.notesB},
        {"blocks",r.blocks},{"over",r.over},{"late",r.late},{"max_gap_us",r.maxGapUs},{"voices",r.voices},{"guard",r.guard}};};
    for(int i=0;i<done;++i){const auto& asset=assets_[0][size_t(filePrograms_[size_t(i)])];
        rows.push_back({{"name",asset.name},{"chord4",row(cpuRows_[size_t(i)][0])},{"chord8",row(cpuRows_[size_t(i)][1])},{"twin",row(cpuRows_[size_t(i)][2])},{"twin_copy_ms",cpuCopyMs_[size_t(i)]}});}
    Json report={{"sample_rate",preparedRate_},{"block",cpuBlock_.load()},{"effects_on_second_core",fxSplit_.load()},
        {"sounds_total",int(cpuRows_.size())},{"finished",done==int(cpuRows_.size())},{"sounds",rows},
        {"rss_kb_before_second_engine",cpuRssBefore_},{"rss_kb_after_second_engine",cpuRssAfter_},{"rss_kb_after_first_copy",cpuRssAfterCopy_},{"rss_kb_now",cpuRssKb()},{"twin_error",cpuTwinError_}};
    try{std::ofstream file(mpc::pluginFolder()+"/cpu-diag.json",std::ios::binary|std::ios::trunc);file<<report.dump(1);}catch(...){}
    {std::lock_guard<std::mutex> lock(stateMutex_);loadMessage_=done==int(cpuRows_.size())?"CPU test finished: "+std::to_string(done)+" sounds.":"CPU test "+std::to_string(done)+" of "+std::to_string(cpuRows_.size())+" done.";setLoadStatus(0);}
}
#endif
