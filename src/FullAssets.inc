// SPDX-License-Identifier: GPL-3.0-or-later
#include "AssetValidation.h"
#include "BankArchive.h"

namespace vitaleon_assets {
// The cycles of a wavetable WAV, each 2048 samples long, as the wavetable loader expects them.
// - A "clm " chunk (written by Serum and Vital) states the cycle length: it is used.
// - Without one, a file shorter than two 2048-sample cycles is ONE cycle of whatever length
//   (single-cycle collections use 600, 1024, 1300 ... samples). Cut at 2048 it would play
//   at the wrong pitch.
// - Everything else is cut at 2048, as before 0.3.25.
inline std::vector<float> wavetableCycles(const std::vector<uint8_t>& file,const std::vector<float>& in){
    size_t cycle=2048;bool stated=false;
    for(size_t p=12;p+8<=file.size();){
        const size_t length=size_t(file[p+4])|size_t(file[p+5])<<8|size_t(file[p+6])<<16|size_t(file[p+7])<<24,body=p+8;
        if(length>file.size()-body)break;
        if(std::memcmp(file.data()+p,"clm ",4)==0&&length>=4&&std::memcmp(file.data()+body,"<!>",3)==0){
            const std::string text(reinterpret_cast<const char*>(file.data())+body+3,std::min<size_t>(length-3,8));const int size=std::atoi(text.c_str());
            if(size>=8&&size<=65536){cycle=size_t(size);stated=true;}
            break;
        }
        p=body+length+(length&1);
    }
    const size_t count=in.size();
    if(!stated&&count<4096&&count!=2048)cycle=count;
    if(cycle==2048||count<cycle||cycle<8)return in;
    const size_t cycles=std::min<size_t>(256,count/cycle);std::vector<float> out(cycles*2048);const double ratio=double(cycle)/2048;
    for(size_t c=0;c<cycles;++c){
        const float* source=in.data()+c*cycle;float* target=out.data()+c*2048;
        auto at=[&](long index){const long size=long(cycle);index%=size;if(index<0)index+=size;return source[index];};
        for(int i=0;i<2048;++i){
            if(ratio<=1){   // fewer samples than 2048: smooth curve through the points, closed to a loop
                const double position=i*ratio;const long k=long(position);const float t=float(position-double(k));
                const float a=at(k-1),b=at(k),d=at(k+1),e=at(k+2);
                target[i]=b+.5f*t*(d-a+t*(2*a-5*b+4*d-e+t*(3*(b-d)+e-a)));
            }else{          // more: the mean of the span each new sample covers
                const double from=i*ratio,to=(i+1)*ratio;double sum=0;
                for(long k=long(from);double(k)<to;++k)sum+=at(k)*(std::min<double>(double(k+1),to)-std::max<double>(double(k),from));
                target[i]=float(sum/ratio);
            }
        }
    }
    return out;
}
}

inline void Vitaleon::scanAssets(){
    for(auto& group:assets_)group.push_back({"Built-in / current",""});
    const char* folders[]={"Presets","Wavetables","Samples","LFOs"};
    for(int group=0;group<4;++group){
        const auto root=std::filesystem::path(mpc::contentPath(folders[group]));std::error_code ec;
        if(!std::filesystem::exists(root,ec))continue;
        for(std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end&&assets_[group].size()<4096;it.increment(ec)){
            if(ec){ec.clear();continue;}if(!it->is_regular_file(ec))continue;
            std::string ext=it->path().extension().string();for(auto& c:ext)c=char(std::tolower((unsigned char)c));
            if((group==0&&ext==".vital")||(group==1&&(ext==".vitaltable"||ext==".wav"))||(group==2&&ext==".wav")||(group==3&&ext==".vitallfo"))
                assets_[group].push_back({it->path().lexically_relative(root).generic_string(),it->path().string()});
        }
    }
    std::error_code ec;auto root=std::filesystem::path(mpc::contentPath("Banks"));
    if(std::filesystem::exists(root,ec))for(std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}if(!it->is_regular_file(ec))continue;std::string ext=it->path().extension().string();for(auto& c:ext)c=char(std::tolower((unsigned char)c));if(ext!=".vitalbank"&&ext!=".zip")continue;
        try {BankArchive bank(it->path().string());for(const auto& e:bank.entries()){std::string type=std::filesystem::path(e.name).extension().string();for(auto& c:type)c=char(std::tolower((unsigned char)c));int group=type==".vital"?0:type==".vitaltable"?1:type==".vitallfo"?3:type==".wav"?(e.name.find("Wavetable")!=std::string::npos?1:2):-1;
            if(group>=0&&assets_[group].size()<4096)assets_[group].push_back({it->path().stem().string()+" / "+e.name,it->path().string(),e.name});}}
        catch(const std::exception& e){loadMessage_=std::string("Bank skipped: ")+it->path().filename().string()+": "+e.what();}
    }
    for(auto& group:assets_){std::sort(group.begin()+1,group.end(),[](const Asset& a,const Asset& b){return a.label<b.label;});
        for(auto& asset:group){asset.name=asset.path.empty()?asset.label:std::filesystem::path(asset.entry.empty()?asset.path:asset.entry).stem().string();
            asset.bank=asset.entry.empty()?"Loose presets":std::filesystem::path(asset.path).stem().string();}}
}
inline std::vector<uint8_t> Vitaleon::readAssetBytes(const Asset& asset){
    if(!asset.entry.empty()){BankArchive bank(asset.path);return bank.extract(asset.entry);}
    std::error_code ec;auto size=std::filesystem::file_size(asset.path,ec);
    vitaleon_assets::require(!ec&&size<=64*1024*1024,"Asset missing or larger than 64 MiB");
    std::vector<uint8_t> bytes;vitaleon_assets::require(mpc::readFile(asset.path,bytes),"Cannot read asset");return bytes;
}
inline Vitaleon::Json Vitaleon::readAsset(const Asset& asset){
    auto bytes=readAssetBytes(asset);Json j=Json::parse(bytes.begin(),bytes.end());vitaleon_assets::tree(j);return j;
}
inline void Vitaleon::defaultTables(){
    savedAssets_["wavetables"]=Json::array();
    for(int o=0;o<3;++o){auto* table=engine_->getWavetable(o);
        WavetableCreator creator(table);creator.initPredefinedWaves();creator.render();creator.setName("Init");savedAssets_["wavetables"].push_back(creator.stateToJson());
    }
}
// One-time conversion of the original sixteen-parameter project/program format.
// Only the full graph renders audio; the old fields remain reserved for state IDs.
inline void Vitaleon::migrateOldControls(){
    for(const auto& b:bindings_)publish(b.index,param(b.index).def);
    auto put=[&](const char* key,float value){publish(controlIndex(key),value);};
    put("volume",(get(Volume)+80)*(get(Volume)+80));
    put("filter_1_on",get(Filter));put("filter_1_cutoff",69+12*std::log2(get(Cutoff)/440));
    put("filter_1_resonance",get(Resonance));put("filter_1_model",vital::constants::kDigital);put("filter_1_style",0);
    put("env_1_attack",std::pow(get(Attack)*.001f,.25f));put("env_1_decay",std::pow(get(Decay)*.001f,.25f));
    put("env_1_sustain",get(Sustain));put("env_1_release",std::pow(get(Release)*.001f,.25f));
    put("oversampling",0);put("voice_override",vital::VoiceHandler::kSteal);
    for(const char* key:{"chorus_on","compressor_on","delay_on","distortion_on","eq_on","filter_fx_on","flanger_on","phaser_on","reverb_on","filter_2_on","sample_on"})put(key,0);
    const int spectral[]={vital::SynthOscillator::kNoSpectralMorph,vital::SynthOscillator::kFormScale,vital::SynthOscillator::kHarmonicScale,vital::SynthOscillator::kSmear};
    for(int o=0;o<3;++o){auto pre="osc_"+std::to_string(o+1)+"_";
        auto osc=[&](const char* suffix,float value){publish(controlIndex(pre+suffix),value);};
        osc("on",o<getInt(Oscillators)?1:0);osc("unison_voices",get(Unison));osc("unison_detune",std::sqrt(get(Detune)));
        osc("wave_frame",get(WavePos)*85);osc("spectral_morph_type",float(spectral[getInt(Spectral)]));osc("spectral_morph_amount",get(Warp));
        osc("destination",vital::constants::kFilter1);osc("level",.5f);osc("random_phase",.75f);osc("tune",o==0?0:(o==1?-.07f:.07f));
        publish(tableSelect_[o],0);
    }
    for(const auto& route:routes_){publish(route.source,0);publish(route.destination,0);}
    publish(presetSelect_,0);publish(sampleSelect_,0);publish(lfoShapeSelect_,0);publish(FullMode,1);
    migrateOldTables_.store(true);programReset_.store(true);refreshProxies();
}
inline void Vitaleon::migrationTables(){
    const vital::PredefinedWaveFrames::Shape shapes[]={vital::PredefinedWaveFrames::kSin,vital::PredefinedWaveFrames::kTriangle,vital::PredefinedWaveFrames::kSaw,vital::PredefinedWaveFrames::kSquare};
    savedAssets_["wavetables"]=Json::array();
    for(int o=0;o<3;++o){
        WavetableCreator creator(engine_->getWavetable(o));auto* group=new WavetableGroup();auto* wave=new WaveSource();
        wave->setInterpolationStyle(WaveSource::kLinear);wave->setInterpolationMode(WaveSource::kTime);
        for(int i=0;i<4;++i){wave->insertNewKeyframe(i*85);wave->getKeyframe(i)->wave_frame()->copy(vital::PredefinedWaveFrames::getWaveFrame(shapes[i]));}
        group->addComponent(wave);creator.addGroup(group);auto state=creator.stateToJson();state["full_normalize"]=false;state["remove_all_dc"]=false;
        creator.jsonToState(state);savedAssets_["wavetables"].push_back(creator.stateToJson());
    }
}
inline void Vitaleon::loadTable(int oscillator,const Json& table){
    vitaleon_assets::table(table);
    if(savedAssets_.count("wavetables")&&savedAssets_["wavetables"].size()==3&&savedAssets_["wavetables"][oscillator]==table)return;
    WavetableCreator creator(engine_->getWavetable(oscillator));creator.jsonToState(table);
    savedAssets_["wavetables"][oscillator]=creator.stateToJson();
}
inline void Vitaleon::loadCurve(LineGenerator* curve,const Json& data){vitaleon_assets::curve(data);curve->jsonToState(data);}
inline void Vitaleon::loadSampleState(const Json& data,const vitaleon_assets::SampleData* decoded){
    // The saved PCM is the exact input used for the current band-limited sample.
    // Reuse its prepared buffers when another preset embeds identical data.
    if(savedAssets_.count("sample")&&savedAssets_["sample"]==data)return;
    vitaleon_assets::SampleData owned;
    if(!decoded){owned=vitaleon_assets::sample(data);decoded=&owned;}
    const auto& sample=*decoded;auto* target=engine_->getSample();
    if(sample.right.empty())target->loadSample(sample.left.data(),int(sample.left.size()),sample.rate);
    else target->loadSample(sample.left.data(),sample.right.data(),int(sample.left.size()),sample.rate);
    target->setName(sample.name);savedAssets_["sample"]=data;
}
inline void Vitaleon::captureAssets(){
    if(!savedAssets_.count("sample")){
        auto* sample=engine_->getSample();int count=sample->originalLength();Json data={{"name",sample->getName()},{"length",count},{"sample_rate",sample->sampleRate()}};
        for(int ch=0;ch<(sample->isStereo()?2:1);++ch){std::vector<int16_t> pcm(count);vital::utils::floatToPcmData(pcm.data(),sample->originalBuffer(ch!=0),count);data[ch?"samples_stereo":"samples"]=Base64::toBase64(pcm.data(),pcm.size()*2).toStdString();}
        loadSampleState(data);
    }
    captureCurves();
    refreshSamplePicture();publishSampleName();publishTableNames();
}
// The LFO shapes and route curves as the project stores them. Called with the state lock held.
inline void Vitaleon::captureCurves(){
    savedAssets_["lfos"]=Json::array();savedAssets_["line_mappings"]=Json::array();
    for(int i=0;i<8;++i)savedAssets_["lfos"].push_back(engine_->getLfoSource(i)->stateToJson());
    for(int i=0;i<64;++i)savedAssets_["line_mappings"].push_back(engine_->getModulationBank().atIndex(i)->modulation_processor->lineMapGenerator()->stateToJson());
}
// Which of the five ready-made shapes a curve is, 0 = none of them (custom). The SHAPE dial
// shows this, so it stays where it was put and reads "Custom" once a point is changed.
inline int Vitaleon::curveShape(const LineGenerator* curve)const{
    struct Reference{int points=0;bool smooth=false;std::vector<std::array<float,3>> data;};
    static const std::array<Reference,5> references=[]{std::array<Reference,5> result;
        for(int shape=0;shape<5;++shape){LineGenerator made;
            switch(shape){case 0:made.initTriangle();break;case 1:made.initSin();break;case 2:made.initSawUp();break;case 3:made.initSawDown();break;default:made.initSquare();}
            auto& r=result[size_t(shape)];r.points=made.getNumPoints();r.smooth=made.smooth();
            for(int i=0;i<r.points;++i)r.data.push_back({made.getPoint(i).first,made.getPoint(i).second,made.getPower(i)});}
        return result;}();
    for(int shape=0;shape<5;++shape){const auto& r=references[size_t(shape)];
        if(curve->getNumPoints()!=r.points||curve->smooth()!=r.smooth)continue;
        bool same=true;
        for(int i=0;i<r.points&&same;++i){const auto point=curve->getPoint(i);
            same=std::abs(point.first-r.data[size_t(i)][0])<1e-4f&&std::abs(point.second-r.data[size_t(i)][1])<1e-4f&&std::abs(curve->getPower(i)-r.data[size_t(i)][2])<1e-4f;}
        if(same)return shape+1;}
    return 0;
}
// Name of the loaded sample for the SAMPLE page (0.3.31). Called with the state lock held,
// after captureAssets has made sure that savedAssets_ describes the sample in the engine.
// When the name changed, its parameter gets a new value so that the host asks for the text again.
inline void Vitaleon::publishSampleName(){
    if(sampleNameText_<0)return;
    auto it=savedAssets_.find("sample");
    std::string name=it!=savedAssets_.end()&&it->is_object()?it->value("name",std::string()):std::string();
    if(sampleNameShown_&&name==sampleName_)return;
    sampleName_=std::move(name);sampleNameShown_=true;
    const size_t hash=std::hash<std::string>{}(sampleName_)|1;
    publishNormalised(sampleNameText_,float(hash&0xffffff)/16777216.0f);browserDisplayDirty_.store(true);
}
// The wavetables of the OSC overview (0.3.52).
// A factory table is a file "NN name" in the folder Factory of the installed wavetables.
inline void Vitaleon::findFactoryTables(){
    factoryTable_.fill(0);
    for(size_t i=1;i<assets_[1].size();++i){const auto& asset=assets_[1][i];if(!asset.entry.empty())continue;
        const std::filesystem::path file(asset.path);const std::string stem=file.stem().string();
        if(file.parent_path().filename()!="Factory"||stem.size()<4||!std::isdigit((unsigned char)stem[0])||!std::isdigit((unsigned char)stem[1])||stem[2]!=' ')continue;
        const int number=(stem[0]-'0')*10+stem[1]-'0';if(number>=1&&number<=kFactoryTables&&!factoryTable_[size_t(number)])factoryTable_[size_t(number)]=int(i);}
}
inline int Vitaleon::factoryNumber(int selection)const{
    if(selection>0)for(int n=1;n<=kFactoryTables;++n)if(factoryTable_[size_t(n)]==selection)return n;
    return 0;
}
// A tap on the page of factory tables.
inline void Vitaleon::pickTable(int number){
    if(number<1||number>kFactoryTables){publishTablePick();return;}
    const int oscillator=mpc::clamp(getInt(tableTarget_),1,3)-1,at=factoryTable_[size_t(number)];
    if(!at){{std::lock_guard<std::mutex> lock(stateMutex_);loadMessage_="Factory table "+std::to_string(number)+" is not installed.";setLoadStatus(2);}publishTablePick();return;}
    if(getInt(tableSelect_[size_t(oscillator)])!=at)set(tableSelect_[size_t(oscillator)],float(at));
}
// Names of the three tables in use. Called with the state lock held, like publishSampleName.
inline void Vitaleon::publishTableNames(){
    if(tableNameText_<0)return;
    const auto tables=savedAssets_.find("wavetables");
    for(int o=0;o<3;++o){std::string name;
        if(tables!=savedAssets_.end()&&tables->is_array()&&size_t(o)<tables->size()&&(*tables)[size_t(o)].is_object()){const auto it=(*tables)[size_t(o)].find("name");if(it!=(*tables)[size_t(o)].end()&&it->is_string())name=it->get<std::string>();}
        if(tableNamesShown_&&name==tableName_[size_t(o)])continue;
        tableName_[size_t(o)]=std::move(name);
        const size_t hash=std::hash<std::string>{}(tableName_[size_t(o)])|1;
        publishNormalised(tableNameText_+o,float(hash&0xffffff)/16777216.0f);browserDisplayDirty_.store(true);}
    tableNamesShown_=true;
}
inline bool Vitaleon::formatTableName(int index,std::string& out){
    if(tableNameText_<0||index<tableNameText_||index>=tableNameText_+3)return false;
    std::lock_guard<std::mutex> lock(stateMutex_);
    const std::string& name=tableName_[size_t(index-tableNameText_)];out=name.empty()?std::string("No name"):textPart(name,0);return true;
}
// Name of the effect at a place of the chain, for the FX overview (0.3.36).
inline bool Vitaleon::formatFxSlot(int index,float plain,std::string& out){
    if(fxSlotText_<0||index<fxSlotText_||index>=fxSlotText_+9)return false;
    static const char* names[9]={"Chorus","Compressor","Delay","Distortion","EQ","FX Filter","Flanger","Phaser","Reverb"};
    out=names[mpc::clamp(int(std::lround(plain*10))-1,0,8)];return true;
}
inline bool Vitaleon::formatSampleName(int index,std::string& out){
    if(sampleNameText_<0||index!=sampleNameText_)return false;
    std::lock_guard<std::mutex> lock(stateMutex_);
    out=sampleName_.empty()?std::string("No sample"):textPart(sampleName_,0);return true;
}
// Texts of the loaded sound on HOME (0.3.27). They live in savedAssets_, which the project
// stores, so a restored project shows them without the file. A sound without macro names
// shows "Macro 1" to "Macro 4".
inline bool Vitaleon::formatSoundText(int index,std::string& out){
    if(soundInfoText_<0||index<soundInfoText_||index>=macroNameText_+4)return false;
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto text=[&](const char* key){auto it=savedAssets_.find(key);return it!=savedAssets_.end()&&it->is_string()?it->get<std::string>():std::string();};
    if(index<soundNoteText_){const std::string author=text("author"),style=text("preset_style");out=textPart(author.empty()||style.empty()?author+style:author+"  |  "+style,index-soundInfoText_);}
    else if(index<macroNameText_)out=textPart(text("comments"),index-soundNoteText_);
    else{const size_t n=size_t(index-macroNameText_);std::string name;auto it=savedAssets_.find("macro_names");
        if(it!=savedAssets_.end()&&it->is_array()&&n<it->size()&&(*it)[n].is_string())name=(*it)[n].get<std::string>();
        out=name.empty()?"Macro "+std::to_string(n+1):textPart(name,0);}
    return true;
}
// Called with the state lock held, at the end of a service pass: when the texts changed,
// their parameters get a new value so that the host asks for the strings again.
inline void Vitaleon::publishSoundTexts(){
    if(soundInfoText_<0)return;
    std::string all;for(const char* key:{"author","preset_style","comments","macro_names"}){auto it=savedAssets_.find(key);if(it!=savedAssets_.end())all+=it->dump();all+='\n';}
    const size_t hash=std::hash<std::string>{}(all)|1;if(hash==soundTextsShown_)return;
    soundTextsShown_=hash;const float marker=float(hash&0xffffff)/16777216.0f;
    for(int p=soundInfoText_;p<macroNameText_+4;++p)publishNormalised(p,marker);
    browserDisplayDirty_.store(true);
}
// Picture of the sample for the OSC overview (0.3.26): the largest level in each of 128
// slices, as read-only parameters. Made here, on the service thread with the engine
// parked, and only when the sample changed. A long sample is probed, not read in full.
inline void Vitaleon::refreshSamplePicture(){
    if(sampleReadout_<0)return;
    const auto* sample=engine_->getSample();const int length=sample->originalLength();
    const float* left=sample->originalBuffer(false);const float* right=sample->isStereo()?sample->originalBuffer(true):nullptr;
    if(length<=0||!left)return;
    uint64_t key=1469598103934665603ull;auto mix=[&](uint64_t value){key=(key^value)*1099511628211ull;};
    mix(uint64_t(length));mix(uint64_t(reinterpret_cast<uintptr_t>(left)));mix(right?1:0);
    for(int probe=0;probe<32;++probe){uint32_t bits;std::memcpy(&bits,left+size_t(probe)*size_t(length-1)/31,4);mix(bits);}
    if(key==samplePictureKey_)return;
    samplePictureKey_=key;bool changed=false;
    // Twice: 128 slices for the small picture on the OSC overview, 256 for the SAMPLE page (0.3.32).
    for(const auto& picture:{std::pair<int,int>{sampleReadout_,kSampleColumns},std::pair<int,int>{sampleFineReadout_,kSampleFineColumns}}){
        const int readout=picture.first,columns=picture.second;if(readout<0)continue;
        for(int column=0;column<columns;++column){
            const size_t from=size_t(column)*size_t(length)/size_t(columns),to=std::min(size_t(length),std::max(from+1,size_t(column+1)*size_t(length)/size_t(columns))),step=std::max<size_t>(1,(to-from)/512);
            float peak=0;
            for(size_t i=from;i<to;i+=step){peak=std::max(peak,std::abs(left[i]));if(right)peak=std::max(peak,std::abs(right[i]));}
            const float value=std::round(mpc::clamp(peak,0.0f,1.0f)*(kWaveLevels-1))/(kWaveLevels-1);
            if(getNormalised(readout+column)!=value){publishNormalised(readout+column,value);changed=true;}
        }
    }
    if(changed)browserDisplayDirty_.store(true);
}
inline void Vitaleon::restoreAssets(const Json& state,bool tables,bool samples){
    if(tables&&state.count("wavetables")){vitaleon_assets::require(state["wavetables"].size()==3,"Expected three wavetables");for(int i=0;i<3;++i)loadTable(i,state["wavetables"][i]);}
    if(samples&&state.count("sample")&&!state["sample"].is_null())loadSampleState(state["sample"]);
    if(state.count("lfos")){vitaleon_assets::require(state["lfos"].size()<=8,"Too many LFOs");for(size_t i=0;i<state["lfos"].size();++i)loadCurve(engine_->getLfoSource(int(i)),state["lfos"][i]);}
    if(state.count("line_mappings")){vitaleon_assets::require(state["line_mappings"].size()<=64,"Too many mappings");for(size_t i=0;i<state["line_mappings"].size();++i)loadCurve(engine_->getModulationBank().atIndex(int(i))->modulation_processor->lineMapGenerator(),state["line_mappings"][i]);}
    captureAssets();for(const char* key:{"preset_name","author","preset_style","comments","macro_names"}){if(state.count(key))savedAssets_[key]=state[key];else savedAssets_.erase(key);}curveSelectionChanged_.store(true);
}
inline void Vitaleon::loadPreset(const Json& preset){
    using namespace vitaleon_assets;
    tree(preset);
    require(preset.is_object()&&preset.count("settings")&&preset["settings"].is_object(),"Not a Vital preset");
    const auto& s=preset["settings"];const std::string version=preset.value("synth_version",std::string("unknown"));
    bool modernStacks=LoadSave::compareVersionStrings(version,"1.5.0")>=0;
    for(int o=1;o<=3;++o)modernStacks|=s.count("osc_"+std::to_string(o)+"_spectral_morph_phase")!=0;
    // Tolerant import: Vital itself ignores settings it does not know. Load
    // everything this engine can represent and report what was left out,
    // instead of refusing the whole sound.
    importNotes_.clear();std::map<std::string,double> overrides;
    auto note=[&](const std::string& text){if(std::find(importNotes_.begin(),importNotes_.end(),text)==importNotes_.end())importNotes_.push_back(text);};
    auto controlValue=[&](const std::string& key,double value){
        auto forced=overrides.find(key);if(forced!=overrides.end())return forced->second;
        // 1.5 inserts Three Octaves at ID 5, shifting Power Chord..Odd Harmonics.
        // Internal IDs retain the 1.0 layout; new choices are appended at 11/12.
        if(modernStacks&&key.find("osc_")==0&&key.size()>12&&key.substr(key.size()-12)=="_stack_style"){
            if(value==5)return 11.0;if(value>=6&&value<=11)return value-1;
        }return value;
    };
    require(LoadSave::compareVersionStrings(version,"1.0.0")>=0,"Pre-1.0 Vital preset needs conversion in Vital");
    // Never silently clamp newer DSP modes to unrelated modes in this engine.
    for(auto i=s.begin();i!=s.end();++i)if(i.value().is_number()){
        const auto key=i.key();if(key=="polyphony"){require(number(i.value())>=1&&number(i.value())<=32,"Polyphony outside 1–32");continue;}if(key=="beats_per_minute")continue;
        // Compatibility metadata, not a new DSP control: newer presets store a
        // spectral phase that has no effect for morph modes 0–11. The unmodulated
        // default is accepted silently, any other value is ignored and reported.
        bool phaseDefault=false;
        for(int osc=1;osc<=3;++osc){auto prefix="osc_"+std::to_string(osc)+"_";
            if(key==prefix+"spectral_morph_phase"){phaseDefault=true;if(std::abs(number(i.value())-.5)>=1e-7)note("OSC "+std::to_string(osc)+" spectral phase ignored");}
            // Spectral modes added after Vital 1.0.6 do not exist in this engine:
            // the oscillator loads without spectral morphing (never another mode).
            if(key==prefix+"spectral_morph_type"&&number(i.value())>param(controlIndex(key)).max+1e-5){overrides[key]=0;note("OSC "+std::to_string(osc)+" spectral mode "+std::to_string(int(number(i.value())))+" not available, off");}
        }
        if(phaseDefault)continue;
        // 1.6 writes both ramp fields for every modulation slot. This older
        // engine has no added ramp stage: accept only the minimum setting as
        // a compatibility bypass, never a configured/modulated ramp. This is
        // not a claim of sample-identical 1.6 smoothing (see COMPATIBILITY.md).
        bool minimumRamp=false;
        for(int slot=1;slot<=64;++slot){auto prefix="modulation_"+std::to_string(slot)+"_ramp_";
            if(key==prefix+"up"||key==prefix+"down"){minimumRamp=true;if(number(i.value())!=-10.0)note("Modulation ramps ignored");}
        }
        if(minimumRamp)continue;
        int index=controlIndex(key);if(index<0){note("Ignored: "+key);continue;}
        double value=controlValue(key,number(i.value()));require(value>=param(index).min-1e-5&&value<=param(index).max+1e-5,"Unsupported preset value: "+key+" (Vital "+version+")");
    }
    require(!s.count("modulations")||s["modulations"].size()<=64,"More than 64 modulations");
    if(s.count("modulations"))for(const auto& route:s["modulations"]){std::string src=route.at("source"),dst=route.at("destination");
        if(!src.empty()&&std::find(sources_.begin(),sources_.end(),src)==sources_.end())note("Mod source skipped: "+src);
        if(!dst.empty()&&std::find(destinations_.begin(),destinations_.end(),dst)==destinations_.end())note("Mod target skipped: "+dst);
        if(route.count("line_mapping"))curve(route["line_mapping"]);}
    if(s.count("lfos")){require(s["lfos"].size()<=8,"Too many LFOs");for(const auto& l:s["lfos"])curve(l);}
    // jsonToState already renders. Prepare each changed table exactly once,
    // retaining the results until ALL preflight checks pass. Reuse unchanged
    // current tables; their canonical JSON describes the current engine data.
    std::array<std::unique_ptr<vital::Wavetable>,3> preparedTables;
    std::array<Json,3> preparedTableStates;
    if(s.count("wavetables")){
        require(s["wavetables"].size()==3,"Expected three wavetables");
        for(int o=0;o<3;++o){const auto& t=s["wavetables"][o];table(t);
            if(savedAssets_.count("wavetables")&&savedAssets_["wavetables"].size()==3&&savedAssets_["wavetables"][o]==t)continue;
            preparedTables[o]=std::make_unique<vital::Wavetable>(257);
            WavetableCreator c(preparedTables[o].get());c.jsonToState(t);preparedTableStates[o]=c.stateToJson();
        }
    }
    std::unique_ptr<SampleData> preparedSample;
    if(s.count("sample")&&!s["sample"].is_null())preparedSample=std::make_unique<SampleData>(sample(s["sample"]));
    // Preflight above must succeed before switching out of the legacy graph.
    clearRoutes();
    if(!s.count("wavetables"))defaultTables();
    // Do not build a fresh band-limited noise sample immediately before replacing
    // it with the preset's embedded sample. Missing samples still reset to noise.
    if(!s.count("sample")||s["sample"].is_null()){engine_->getSample()->init();savedAssets_.erase("sample");}
    for(int i=0;i<8;++i)engine_->getLfoSource(i)->initTriangle();
    programReset_.store(false);migrateOldTables_.store(false);curveEdits_.store(0);
    for(const auto& b:bindings_)publish(b.index,s.count(b.key)?float(controlValue(b.key,s[b.key].get<double>())):param(b.index).def);
    // MPC budget: imported sounds start at 1x oversampling and at most eight
    // voices. Nearly every Vital preset stores 2x, which roughly doubles the
    // cost per voice. Both remain adjustable (HOME > VOICES) and are saved
    // with the MPC project; the preset file itself is never changed.
    publish(Voices,std::min(8.0f,s.value("polyphony",8.0f)));publish(controlIndex("oversampling"),0);publish(FullMode,1);
    for(int i=0;i<64;++i){publish(routes_[i].source,0);publish(routes_[i].destination,0);auto* mapping=engine_->getModulationBank().atIndex(i)->modulation_processor->lineMapGenerator();mapping->initLinear();
        if(s.count("modulations")&&size_t(i)<s["modulations"].size()){const auto& r=s["modulations"][i];std::string src=r["source"],dst=r["destination"];
            if(!src.empty()&&!dst.empty()&&std::find(sources_.begin(),sources_.end(),src)!=sources_.end()&&std::find(destinations_.begin(),destinations_.end(),dst)!=destinations_.end()){publish(routes_[i].source,float(std::find(sources_.begin(),sources_.end(),src)-sources_.begin()));publish(routes_[i].destination,float(std::find(destinations_.begin(),destinations_.end(),dst)-destinations_.begin()));}
            if(r.count("line_mapping"))loadCurve(mapping,r["line_mapping"]);}}
    for(int o=0;o<3;++o)if(preparedTables[o]){
        engine_->getWavetable(o)->adoptPrepared(*preparedTables[o]);
        savedAssets_["wavetables"][o]=std::move(preparedTableStates[o]);
    }
    if(preparedSample)loadSampleState(s["sample"],preparedSample.get());
    restoreAssets(s,false,false);savedAssets_["preset_name"]=preset.value("preset_name",std::string("Imported"));savedAssets_["author"]=preset.value("author",std::string());
    // What a sound says about itself, for HOME (0.3.27): style, comment and the names of its
    // macros. Text only, cut to a sane length at a character boundary.
    {auto words=[&](const char* key){auto it=preset.find(key);std::string text=it!=preset.end()&&it->is_string()?it->get<std::string>():std::string();
        if(text.size()>240){size_t end=240;while(end>0&&(uint8_t(text[end])&0xc0)==0x80)--end;text.resize(end);}return text;};
     savedAssets_["preset_style"]=words("preset_style");savedAssets_["comments"]=words("comments");
     savedAssets_["macro_names"]=Json::array({words("macro1"),words("macro2"),words("macro3"),words("macro4")});}
    // A preset brings its own tables, sample and LFO shapes. The file selectors go
    // back to "Built-in / current" so they never name a file that is no longer loaded
    // (seen on the Live II: table 14 chosen, factory sound loaded, selector still said 14).
    for(int o=0;o<3;++o){publish(tableSelect_[o],0);lastTable_[o]=0;}
    publish(sampleSelect_,0);lastSample_=0;publish(lfoShapeSelect_,0);lastShape_=0;
}
inline void Vitaleon::loadSelections(){
    for(int o=0;o<3;++o){int selection=getInt(tableSelect_[o]);if(selection==lastTable_[o])continue;lastTable_[o]=selection;if(!selection)continue;
        const auto& asset=assets_[1].at(selection);auto extension=std::filesystem::path(asset.entry.empty()?asset.path:asset.entry).extension().string();for(auto& c:extension)c=char(std::tolower((unsigned char)c));if(extension==".wav"){
            dsp::WavData w;const auto bytes=readAssetBytes(asset);vitaleon_assets::require(dsp::parseWav(bytes,w)&&w.numFrames()>=8&&w.numFrames()<=16777216,"Invalid wavetable WAV");
            loadTableAudio(o,vitaleon_assets::wavetableCycles(bytes,w.channels[0]),int(w.sampleRate),asset.name);
        }else loadTable(o,readAsset(asset));
        // Say what happened, and replace an older failure text (it used to stay for good).
        loadMessage_="Table loaded: "+asset.name+" > OSC "+std::to_string(o+1);setLoadStatus(0);
    }
    int sampleIndex=getInt(sampleSelect_);if(sampleIndex!=lastSample_){lastSample_=sampleIndex;if(sampleIndex){const auto& asset=assets_[2].at(sampleIndex);dsp::WavData w;
        vitaleon_assets::require(dsp::parseWav(readAssetBytes(asset),w)&&w.numFrames()>=4&&w.numFrames()<=16777216,"Invalid sample WAV");
        Json data={{"name",asset.label},{"length",w.numFrames()},{"sample_rate",int(w.sampleRate)}};
        for(int ch=0;ch<std::min(2,w.numChannels());++ch){std::vector<int16_t> pcm(w.numFrames());for(size_t i=0;i<pcm.size();++i){vitaleon_assets::require(std::isfinite(w.channels[ch][i]),"Non-finite WAV");pcm[i]=int16_t(std::round(mpc::clamp(w.channels[ch][i],-1.0f,1.0f)*32767));}data[ch?"samples_stereo":"samples"]=Base64::toBase64(pcm.data(),pcm.size()*2).toStdString();}loadSampleState(data);
        loadMessage_="Sample loaded: "+asset.name;setLoadStatus(0);}}
    int shape=getInt(lfoShapeSelect_);if(shape!=lastShape_){lastShape_=shape;if(shape){loadCurve(engine_->getLfoSource(getInt(lfoSelect_)-1),readAsset(assets_[3].at(shape)));curveSelectionChanged_.store(true);
        loadMessage_="LFO shape loaded: "+assets_[3].at(shape).name+" > LFO "+std::to_string(getInt(lfoSelect_));setLoadStatus(0);}}
}
inline void Vitaleon::loadTableAudio(int oscillator,const std::vector<float>& audio,int rate,const std::string& name){
    WavetableCreator creator(engine_->getWavetable(oscillator));
    creator.initFromAudioFile(audio.data(),int(audio.size()),rate,WavetableCreator::kWavetableSplice,FileSource::kNoInterpolate);creator.render();
    creator.setName(name);savedAssets_["wavetables"][oscillator]=creator.stateToJson();
}
// Library import, first half (service thread, the sound keeps playing): read the file,
// check it and decode it. Anything wrong with the file is thrown here, before the
// sound is touched; the browser shows the reason.
inline void Vitaleon::queueImport(const std::string& path,int oscillator,bool keep){
    using namespace vitaleon_assets;
    std::string extension=std::filesystem::path(path).extension().string();for(auto& c:extension)c=char(std::tolower((unsigned char)c));
    PendingImport job;job.name=std::filesystem::path(path).stem().string();
    {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Reading "+job.name+"...";publishBrowser();}
    // oscillator: 0 to 2 when a row of the wavetable page or an arrow of the overview asks; else
    // the place LIBRARY is switched to (WAV AS).
    const int chosen=oscillator>=0?mpc::clamp(oscillator,0,2):(importTarget_>=0?mpc::clamp(getInt(importTarget_),0,3):0);
    if(chosen<3)lastTableTarget_=chosen;
    const auto bytes=readAssetBytes({job.name,path,"",job.name,""});
    auto json=[&](const char* what){
        try{Json j=Json::parse(bytes.begin(),bytes.end());tree(j);return j;}
        catch(const nlohmann::json::exception&){throw std::runtime_error(std::string("not a valid ")+what+" file");}
    };
    if(extension==".vitallfo"){job.kind=3;job.data=json("LFO shape");curve(job.data);job.target=mpc::clamp(getInt(lfoSelect_),1,8)-1;}
    else if(extension==".vitaltable"){job.kind=1;job.data=json("wavetable");table(job.data);job.target=lastTableTarget_;}
    else{
        dsp::WavData w;require(dsp::parseWav(bytes,w)&&w.numChannels()>=1,"not a readable WAV (PCM 8 to 32 bit or 32-bit float)");
        if(chosen<3){
            require(w.numFrames()>=8&&w.numFrames()<=16777216,"WAV too short or too long for a wavetable");
            for(float v:w.channels[0])require(std::isfinite(v),"WAV holds invalid samples");
            job.kind=2;job.target=chosen;job.rate=int(w.sampleRate);job.audio=wavetableCycles(bytes,w.channels[0]);
        }else{
            require(w.numFrames()>=4,"WAV too short for a sample");
            require(w.numFrames()<=4194304,"sample too long (at most 95 seconds at 44.1 kHz)");
            require(w.sampleRate>=8000&&w.sampleRate<=384000,"sample rate not supported");
            job.kind=4;job.data={{"name",job.name},{"length",w.numFrames()},{"sample_rate",int(w.sampleRate)}};
            for(int ch=0;ch<std::min(2,w.numChannels());++ch){std::vector<int16_t> pcm(w.numFrames());
                for(size_t i=0;i<pcm.size();++i){require(std::isfinite(w.channels[ch][i]),"WAV holds invalid samples");pcm[i]=int16_t(std::round(mpc::clamp(w.channels[ch][i],-1.0f,1.0f)*32767));}
                job.data[ch?"samples_stereo":"samples"]=Base64::toBase64(pcm.data(),pcm.size()*2).toStdString();}
        }
    }
    // 0.3.53: a table loaded in LIBRARY is copied into the user's folder (keep); a table that
    // is loaded from that folder is remembered by its path, so its row can be marked.
    if(job.kind==1||job.kind==2){if(keep)job.note=keepTable(path,job.userPath);else job.userPath=path;}
    {std::lock_guard<std::mutex> state(stateMutex_);pendingImport_=std::move(job);importPending_=true;}
    requestService();
}
// Second half (service pass, engine parked, state lock held): put the decoded data into
// the sound. The part that receives it is switched on if it was off, so the result is
// heard at once; the status line says so.
inline void Vitaleon::applyImport(){
    if(!importPending_)return;
    importPending_=false;PendingImport job=std::move(pendingImport_);pendingImport_=PendingImport();
    try{
        auto switchOn=[&](const std::string& key){const int index=controlIndex(key);if(get(index)>=.5f)return std::string();publish(index,1);return std::string(", switched on");};
        if(job.kind==1||job.kind==2){
            if(job.kind==1){loadTable(job.target,job.data);}
            else loadTableAudio(job.target,job.audio,job.rate,job.name);
            publish(tableSelect_[job.target],0);lastTable_[job.target]=0;
            const std::string osc=std::to_string(job.target+1);
            loadMessage_="Table loaded: "+job.name+" > OSC "+osc+switchOn("osc_"+osc+"_on");
            if(!job.note.empty())loadMessage_+="."+job.note;
            {std::lock_guard<std::mutex> tables(tableMutex_);userTablePath_[size_t(job.target)]=job.userPath;}
        }else if(job.kind==3){
            loadCurve(engine_->getLfoSource(job.target),job.data);curveSelectionChanged_.store(true);
            publish(lfoShapeSelect_,0);lastShape_=0;
            loadMessage_="LFO shape loaded: "+job.name+" > LFO "+std::to_string(job.target+1);
        }else if(job.kind==4){
            loadSampleState(job.data);publish(sampleSelect_,0);lastSample_=0;
            loadMessage_="Sample loaded: "+job.name+switchOn("sample_on");
        }else return;
        setLoadStatus(0);
    }catch(const std::exception& e){loadMessage_="Cannot load "+job.name+": "+e.what();setLoadStatus(2);}
}
inline void Vitaleon::editLfo(){
    auto* curve=selectedCurve();
    unsigned edits=curveEdits_.exchange(0);
    if(edits){
        switch((edits&1)?getInt(lfoShape_):0){case 1:curve->initTriangle();break;case 2:curve->initSin();break;case 3:curve->initSawUp();break;case 4:curve->initSawDown();break;case 5:curve->initSquare();break;default:{
            int n=(edits&2)?getInt(pointCount_):curve->getNumPoints();if(n!=curve->getNumPoints()){while(curve->getNumPoints()<n)curve->addMiddlePoint(curve->getNumPoints()-2);while(curve->getNumPoints()>n)curve->removePoint(curve->getNumPoints()-2);}
            int p=mpc::clamp(getInt(pointSelect_)-1,0,n-1);float lo=p?curve->getPoint(p-1).first:0,hi=p+1<n?curve->getPoint(p+1).first:1;
            auto point=curve->getPoint(p);if(edits&4)point.first=mpc::clamp(get(pointX_),lo,hi);if(edits&8)point.second=1-get(pointY_);
            curve->setPoint(p,point);if(edits&16)curve->setPower(p,get(pointCurve_));break;}}
        if(edits&32)curve->setSmooth(getBool(lfoSmooth_));curve->render();
    }
    curveSelectionChanged_.store(false);refreshCurveControls();
}
inline void Vitaleon::refreshCurveControls(){
    const auto* curve=selectedCurve();int p=mpc::clamp(getInt(pointSelect_)-1,0,curve->getNumPoints()-1);
    auto edits=curveEdits_.load();publish(pointSelect_,float(p+1));
    // Up to 0.3.35 the shape selector fell back to Custom after every use, which no dial can show.
    if(!(edits&1))publish(lfoShape_,float(curveShape(curve)));
    if(!(edits&2))publish(pointCount_,float(curve->getNumPoints()));
    if(!(edits&4))publish(pointX_,curve->getPoint(p).first);
    if(!(edits&8))publish(pointY_,1-curve->getPoint(p).second);
    if(!(edits&16))publish(pointCurve_,curve->getPower(p));
    if(!(edits&32))publish(lfoSmooth_,curve->smooth()?1:0);
}
