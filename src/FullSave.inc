// SPDX-License-Identifier: GPL-3.0-or-later
// 0.3.44: SAVE SOUND. The sound that is playing is written out as a Vital preset (.vital),
// so that it can be used in another project, copied to another MPC or opened in Vital.
//
// Where: <drive>/Vitaleon/Sounds, beside the "VST2 Plugins" folder and NOT inside the plugin's
// own folder: replacing the plugin with a newer version replaces that folder as a whole.
// The folder is the bank "My sounds" of the bank selector.
//
// What: everything a project stores about the sound. The numbers of all controls, the 64
// routes with their curves, the eight LFO shapes, the three wavetables and the sample, the
// names of the macros, author, style and comment.

// The folder for what the user keeps: <drive>/Vitaleon/<part>. Without a "VST2 Plugins"
// folder above the plugin (a build folder, the tests) it stands beside the plugin's folder.
inline std::string Vitaleon::userFolder(const char* part){
    namespace fs=std::filesystem;fs::path plugin=fs::path(mpc::pluginFolder()).lexically_normal();
    if(!plugin.has_filename())plugin=plugin.parent_path();
    fs::path root=plugin.parent_path();
    for(auto p=plugin;!p.empty()&&p!=p.parent_path();p=p.parent_path())if(p.filename()=="VST2 Plugins"){root=p.parent_path();break;}
    return (root/"Vitaleon"/part).string();
}
// The sound as a Vital preset. Vital 1.0.6 numbers the unison stack styles 0 to 10; the two
// styles added later are 11 and 12 inside this engine (see loadPreset). A sound that uses one
// of them is written in the newer numbering with the newer version, so that it reads back the
// same here and in a current Vital; every other sound is a plain 1.0.6 preset.
inline Vitaleon::Json Vitaleon::presetJson(const std::string& name){
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto stack=[](const std::string& key){return key.rfind("osc_",0)==0&&key.size()>12&&key.compare(key.size()-12,12,"_stack_style")==0;};
    bool modern=false;for(const auto& b:bindings_)if(stack(b.key)&&get(b.index)>10.5f)modern=true;
    Json settings=Json::object();
    for(const auto& b:bindings_){double value=get(b.index);
        if(modern&&stack(b.key)){const int style=int(std::lround(value));value=style==11?5:style>=5&&style<=10?style+1:style;}
        settings[b.key]=value;}
    settings["polyphony"]=getInt(Voices);
    Json routes=Json::array();const auto mappings=savedAssets_.find("line_mappings");
    for(int i=0;i<64;++i){const int source=getInt(routes_[size_t(i)].source),target=getInt(routes_[size_t(i)].destination);
        Json route={{"source",source>0&&target>0?sources_.at(size_t(source)):std::string()},{"destination",source>0&&target>0?destinations_.at(size_t(target)):std::string()}};
        if(mappings!=savedAssets_.end()&&mappings->is_array()&&size_t(i)<mappings->size())route["line_mapping"]=(*mappings)[size_t(i)];
        routes.push_back(std::move(route));}
    settings["modulations"]=std::move(routes);
    for(const char* key:{"lfos","wavetables","sample"}){auto it=savedAssets_.find(key);if(it!=savedAssets_.end()&&!it->is_null())settings[key]=*it;}
    auto text=[&](const char* key){auto it=savedAssets_.find(key);return it!=savedAssets_.end()&&it->is_string()?it->get<std::string>():std::string();};
    Json preset={{"author",text("author")},{"comments",text("comments")},{"preset_name",name},{"preset_style",text("preset_style")},{"synth_version",modern?"1.5.5":"1.0.6"}};
    const auto macros=savedAssets_.find("macro_names");
    for(int n=0;n<4;++n){std::string macro;if(macros!=savedAssets_.end()&&macros->is_array()&&size_t(n)<macros->size()&&(*macros)[size_t(n)].is_string())macro=(*macros)[size_t(n)].get<std::string>();
        preset["macro"+std::to_string(n+1)]=macro.empty()?"MACRO "+std::to_string(n+1):macro;}
    preset["settings"]=std::move(settings);
    return preset;
}
// A name as it can be typed here and stored as a file name everywhere: letters, digits,
// spaces and hyphens, at most 24 characters (the length of a name in the MPC's PRESET list).
inline std::string Vitaleon::saveNameFrom(const std::string& text){
    std::string name;
    for(unsigned char ch:text){if(std::isalnum(ch)||ch=='-')name+=char(ch);else if((ch==' '||ch=='_')&&!name.empty()&&name.back()!=' ')name+=' ';if(name.size()>=kSaveNameLength)break;}
    while(!name.empty()&&name.back()==' ')name.pop_back();
    return name;
}
// Called with saveMutex_ held: the name line and the status line get a new value, so that the
// host asks for their texts again.
inline void Vitaleon::publishSaveTexts(){
    if(saveNameText_<0)return;
    const float marker=float((std::hash<std::string>{}(saveName_+'\n'+saveStatus_)&0xfffffe)|1)/16777216.0f;
    publishNormalised(saveNameText_,marker);for(int i=0;i<3;++i)publishNormalised(saveStatusText_+i,marker);
    browserDisplayDirty_.store(true);
}
inline bool Vitaleon::formatSaveText(int index,std::string& out){
    if(saveNameText_<0||(index!=saveNameText_&&(index<saveStatusText_||index>=saveStatusText_+3)))return false;
    std::lock_guard<std::mutex> lock(saveMutex_);
    out=index==saveNameText_?saveName_+"_":textPart(saveStatus_,index-saveStatusText_);
    return true;
}
// SAVE SOUND on HOME: the page opens with the name of the loaded sound, ready to be changed.
inline void Vitaleon::openSave(){
    if(saveView_<0)return;
    std::string loaded;{std::lock_guard<std::mutex> state(stateMutex_);loaded=loadedName_;}
    {std::lock_guard<std::mutex> lock(saveMutex_);saveName_=saveNameFrom(loaded);saveConfirm_.clear();
     saveStatus_="Type a name, then SAVE. The sound is stored in My sounds.";publishSaveTexts();}
    navigateTo(saveView_,0);
}
// One key of the page: 1-26 A to Z, 27-36 the digits 1 to 9 and 0, 37 space, 38 hyphen,
// 39 delete the last character, 40 clear the name. With CAPS off the first letter of a word
// is a capital and the others are small; with CAPS on every letter is a capital.
inline void Vitaleon::saveKey(int key){
    std::lock_guard<std::mutex> lock(saveMutex_);
    const bool room=saveName_.size()<kSaveNameLength,start=saveName_.empty()||saveName_.back()==' '||saveName_.back()=='-';
    if(key>=1&&key<=26){if(room)saveName_+=char((start||(saveCaps_>=0&&getBool(saveCaps_))?'A':'a')+key-1);}
    else if(key>=27&&key<=36){if(room)saveName_+=char('0'+(key-26)%10);}
    else if(key==37){if(room&&!start)saveName_+=' ';}
    else if(key==38){if(room&&!saveName_.empty()&&saveName_.back()!='-')saveName_+='-';}
    else if(key==39){if(!saveName_.empty())saveName_.pop_back();}
    else if(key==40)saveName_.clear();
    saveConfirm_.clear();saveStatus_=room||key>=39?"Type a name, then SAVE. The sound is stored in My sounds.":"A name has at most 24 characters.";
    publishSaveTexts();
}
// SAVE on the page. Runs on the service thread (a sound with a sample is a few megabytes).
inline void Vitaleon::saveSound(){
    namespace fs=std::filesystem;std::error_code ec;
    std::string name;{std::lock_guard<std::mutex> lock(saveMutex_);name=saveNameFrom(saveName_);}
    auto say=[&](const std::string& text){std::lock_guard<std::mutex> lock(saveMutex_);saveStatus_=text;publishSaveTexts();};
    if(name.empty()){say("Type a name first.");return;}
    const fs::path folder(userFolder("Sounds")),target=folder/(name+".vital");
    fs::create_directories(folder,ec);ec.clear();
    if(fs::exists(target,ec)){bool confirmed=false;
        {std::lock_guard<std::mutex> lock(saveMutex_);confirmed=saveConfirm_==name;saveConfirm_=name;}
        if(!confirmed){say("\""+name+"\" is already in My sounds. SAVE again replaces it.");return;}}
    ec.clear();
    try{
        say("Saving...");
        const std::string text=presetJson(name).dump();
        const auto space=fs::space(folder,ec);
        if(!ec&&space.available<uintmax_t(text.size())+uintmax_t(1)*1024*1024)throw std::runtime_error("not enough room on the drive.");
        ec.clear();fs::path part=target;part+=".part";
        {std::ofstream out(part,std::ios::binary|std::ios::trunc);
         if(!out)throw std::runtime_error("the folder Vitaleon/Sounds cannot be written.");
         out.write(text.data(),std::streamsize(text.size()));out.flush();
         if(!out){out.close();fs::remove(part,ec);throw std::runtime_error("writing failed. Is the drive full?");}}
        fs::remove(target,ec);ec.clear();fs::rename(part,target,ec);
        if(ec){fs::remove(part,ec);throw std::runtime_error("the file could not be finished.");}
        // The sound that is playing IS this file now: its name on HOME, and its place in the
        // bank "My sounds", which PRESET lists from here on. The project remembers both.
        auto catalog=indexFolder(folder.string());
        {std::lock_guard<std::mutex> state(stateMutex_);
         loadedName_=name;savedAssets_["preset_name"]=name;
         for(const auto& file:catalog->files)if(fs::path(file.path).filename()==target.filename())selectedAsset_=file;
         loadMessage_="Saved: "+name+". PRESET lists My sounds.";setLoadStatus(0);publishLoadedName();}
        installCatalog(catalog);
        {std::lock_guard<std::mutex> lock(saveMutex_);saveConfirm_.clear();saveStatus_="Saved.";publishSaveTexts();}
        navigateTo(0,0);
    }catch(const std::exception& e){say(std::string("Not saved: ")+e.what());}
}
// 0.3.45 MY SOUNDS in the library: the folder of the saved sounds as rows, LOAD on each.
inline void Vitaleon::showMySounds(){
    namespace fs=std::filesystem;std::error_code ec;
    const fs::path sounds(userFolder("Sounds"));fs::create_directories(sounds,ec);ec.clear();
    auto place=fs::canonical(sounds,ec);if(ec){place=sounds;ec.clear();}
    auto entries=vitaleon_storage::list(place.string(),place.string());
    std::lock_guard<std::mutex> lock(browserMutex_);
    browser_.entries=std::move(entries);browser_.path=place.string();browser_.root=place.string();browser_.page=0;
    browser_.status=browser_.entries.empty()?"No sound saved yet. SAVE SOUND on HOME stores sounds here.":"My sounds. LOAD one. To copy them: open a USB stick or SD card, then EXPORT.";
    publishBrowser();
}
// 0.3.45 EXPORT: every sound of My sounds is copied to <drive>/Vitaleon/Sounds on the drive that
// is open in the library. The same place a Vitaleon on that drive saves to, so a stick carries
// the sounds to another MPC, and a computer finds them in one folder. A file of the same name
// is replaced. Runs on the service thread; returns the sentence for the status line.
inline std::string Vitaleon::exportSounds(const std::string& driveRoot){
    namespace fs=std::filesystem;std::error_code ec;
    const std::string first="Open a USB stick or SD card first (DRIVES), then EXPORT.";
    if(driveRoot.empty())return first;
    const fs::path source(userFolder("Sounds"));
    const fs::path drive=fs::weakly_canonical(fs::path(driveRoot),ec);if(ec)return "The drive is not available. Return to DRIVES.";
    const fs::path home=fs::weakly_canonical(source.parent_path(),ec);ec.clear();
    const fs::path plugin=fs::weakly_canonical(fs::path(mpc::pluginFolder()),ec);ec.clear();
    // Not into the plugin's own folder and not into the folder the sounds come from.
    if(vitaleon_storage::inside(drive,plugin)||vitaleon_storage::inside(drive,home))return first;
    const fs::path target=drive/"Vitaleon"/"Sounds";
    {const auto there=fs::weakly_canonical(target,ec);ec.clear();const auto here=fs::weakly_canonical(source,ec);ec.clear();
     if(there==here)return "My sounds are on this drive already. Open a USB stick or SD card (DRIVES).";}
    std::vector<fs::path> files;uintmax_t total=0;
    if(fs::exists(source,ec))for(fs::directory_iterator it(source,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_regular_file(ec)){ec.clear();continue;}
        std::string ext=it->path().extension().string();for(auto& ch:ext)ch=char(std::tolower((unsigned char)ch));if(ext!=".vital")continue;
        files.push_back(it->path());const auto size=it->file_size(ec);if(!ec)total+=size;ec.clear();}
    ec.clear();
    if(files.empty())return "Nothing to export yet. SAVE SOUND on HOME stores sounds in My sounds.";
    std::sort(files.begin(),files.end());
    fs::create_directories(target,ec);if(ec)return "Not exported: this drive cannot be written.";
    const auto space=fs::space(target,ec);
    if(!ec&&space.available<total+uintmax_t(1)*1024*1024)return "Not exported: not enough room on this drive.";
    ec.clear();int done=0;std::vector<char> buffer(size_t(1)<<18);
    for(const auto& from:files){
        {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Exporting "+std::to_string(done+1)+" of "+std::to_string(files.size())+"...";publishBrowser();}
        const fs::path to=target/from.filename();fs::path part=to;part+=".part";bool good=false;
        {std::ifstream in(from,std::ios::binary);std::ofstream out(part,std::ios::binary|std::ios::trunc);
         if(in&&out){uintmax_t copied=0;
            while(in){in.read(buffer.data(),std::streamsize(buffer.size()));const auto count=in.gcount();if(count<=0)break;out.write(buffer.data(),count);if(!out)break;copied+=uintmax_t(count);}
            out.flush();const auto size=fs::file_size(from,ec);good=bool(out)&&!ec&&copied==size;ec.clear();}}
        if(good){fs::remove(to,ec);ec.clear();fs::rename(part,to,ec);good=!ec;}
        if(!good){fs::remove(part,ec);return "Export stopped after "+std::to_string(done)+" of "+std::to_string(files.size())+": the drive cannot be written or was removed.";}
        ++done;}
    std::string name=drive.filename().string();if(name.empty())name=drive.string();
    return "Exported "+std::to_string(done)+(done==1?" sound":" sounds")+" to "+name+"/Vitaleon/Sounds.";
}
