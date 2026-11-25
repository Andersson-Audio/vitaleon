// SPDX-License-Identifier: GPL-3.0-or-later
// Catalog snapshots never mutate while the host reads their names.
inline int Vitaleon::numPrograms()const{auto c=std::atomic_load(&activeCatalog_);return c?int(c->names.size()):mpc::Plugin::numPrograms();}
inline const char* Vitaleon::programName(int i)const{
    auto c=std::atomic_load(&activeCatalog_);if(!c)return mpc::Plugin::programName(i);
    // The ABI copies this string immediately; a thread-local owner keeps it alive
    // even when another thread switches the catalog during that copy.
    thread_local char name[kVstMaxProgNameLen+1]{};
    if(i<0||i>=int(c->names.size())){name[0]=0;return name;}
    const auto& text=c->names[i];size_t n=std::min(text.size(),size_t(kVstMaxProgNameLen));std::memcpy(name,text.data(),n);name[n]=0;return name;
}
inline void Vitaleon::setProgram(int i){
    if(i<0||i>=numPrograms())return;
    if(i<factoryProgramCount_){mpc::Plugin::setProgram(i);return;}
    auto before=std::atomic_load(&activeCatalog_);if(before&&i<int(before->names.size())&&before->files.at(i-factoryProgramCount_).path.empty())return;
    beginStateTransaction(true);
    struct EndTransaction{Vitaleon* p;~EndTransaction(){p->endStateTransaction(true);}} transaction{this};
    auto c=std::atomic_load(&activeCatalog_);if(!c||i>=int(c->names.size())||c->files.at(i-factoryProgramCount_).path.empty())return;
    if(c==initialCatalog_){currentProgram_=i;programSelected(i);stateLoadPending_.store(true);return;}
    {std::lock_guard<std::mutex> state(stateMutex_);pendingAsset_=c->files.at(i-factoryProgramCount_);externalPresetPending_=true;stagedState_=Json();}
    currentProgram_=i;requestService();browserDisplayDirty_.store(true);stateLoadPending_.store(true);
}
inline void Vitaleon::createStorage(){
    auto c=std::make_shared<Catalog>();c->name="All installed banks";
    for(int i=0;i<mpc::Plugin::numPrograms();++i)c->names.push_back(mpc::Plugin::programName(i));
    for(int file:filePrograms_)c->files.push_back(assets_[0][file]);
    initialCatalog_=c;std::atomic_store(&activeCatalog_,std::shared_ptr<const Catalog>(c));
    // 0.3.39: a new instance starts on the factory bank when it is installed, so PRESET shows
    // that bank and not every installed sound. A project that names another bank replaces it
    // when its state arrives. Without the factory bank everything stays as it was.
    for(const auto& bank:installedBanks())if(!bank.folder&&!bank.path.empty()){
        std::string lower=bank.name;for(auto& ch:lower)ch=char(std::tolower((unsigned char)ch));
        if(lower!="vitaleon factory")continue;
        try{std::atomic_store(&activeCatalog_,indexBank(bank.path));}catch(const std::exception&){}
        break;}
    const char* commands[]={"Drives","Up Folder","Refresh Drives","Previous Files","Next Files","Installed Banks","Open Row 1","Open Row 2","Open Row 3","Open Row 4","Open Row 5","Open Row 6"};
    storageCommands_=numParams();for(int i=0;i<12;++i){auto id="storage_command_"+std::to_string(i);auto p=mpc::boolParam(id.c_str(),commands[i],false);p.automatable=false;addParam(p);}
    storageRows_=numParams();for(int i=0;i<12;++i)addReadout("storage_row_"+std::to_string(i));
    storagePath_=numParams();for(int i=0;i<3;++i)addReadout("storage_path_"+std::to_string(i));
    storageStatus_=numParams();for(int i=0;i<4;++i)addReadout("storage_status_"+std::to_string(i));
    storagePage_=addReadout("storage_page");activeBankText_=numParams();for(int i=0;i<2;++i)addReadout("active_bank_"+std::to_string(i));
    storageAvailable_=numParams();for(int i=0;i<6;++i)addReadout("storage_available_"+std::to_string(i));
    browser_.entries=vitaleon_storage::discover(mpc::pluginFolder(),userFolder("Sounds"),userFolder("Wavetables"));publishBrowser();
}
// The first sound of the bank in PRESET, loaded when a new instance is prepared.
inline void Vitaleon::startFirstSound(){
    auto c=std::atomic_load(&activeCatalog_);
    if(!c||c==initialCatalog_||c->files.empty()||c->files.front().path.empty()){programSelected(factoryProgramCount_);return;}
    {std::lock_guard<std::mutex> state(stateMutex_);pendingAsset_=c->files.front();externalPresetPending_=true;stagedState_=Json();}
    currentProgram_=factoryProgramCount_;requestService();browserDisplayDirty_.store(true);stateLoadPending_.store(true);
}
// The banks in the plugin's own Banks folder, the factory bank first, then the folder of loose
// sounds if it holds any, and last "all installed sounds" (no path). Read from the drive each
// time, so a bank installed a moment ago is in the list.
inline std::vector<Vitaleon::InstalledBank> Vitaleon::installedBanks()const{
    namespace fs=std::filesystem;std::vector<InstalledBank> banks;std::error_code ec;
    auto lower=[](std::string text){for(auto& ch:text)ch=char(std::tolower((unsigned char)ch));return text;};
    const fs::path folder(mpc::contentPath("Banks"));
    if(fs::exists(folder,ec))for(fs::recursive_directory_iterator it(folder,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_regular_file(ec)){ec.clear();continue;}
        const std::string ext=lower(it->path().extension().string());if(ext!=".vitalbank"&&ext!=".zip")continue;
        banks.push_back({it->path().stem().string(),it->path().string(),false});}
    std::sort(banks.begin(),banks.end(),[&](const InstalledBank& a,const InstalledBank& b){
        const bool fa=lower(a.name)=="vitaleon factory",fb=lower(b.name)=="vitaleon factory";return fa!=fb?fa:a.name<b.name;});
    // 0.3.44: the sounds saved with SAVE SOUND, in <drive>/Vitaleon/Sounds.
    ec.clear();const fs::path mine(userFolder("Sounds"));
    if(fs::exists(mine,ec))for(fs::directory_iterator it(mine,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec))
        if(it->is_regular_file(ec)&&lower(it->path().extension().string())==".vital"){banks.push_back({"My sounds",mine.string(),true});break;}
    ec.clear();const fs::path loose(mpc::contentPath("Presets"));
    if(fs::exists(loose,ec))for(fs::directory_iterator it(loose,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec))
        if(it->is_regular_file(ec)&&lower(it->path().extension().string())==".vital"){banks.push_back({"Loose presets",loose.string(),true});break;}
    banks.push_back({"All installed banks",std::string(),false});
    return banks;
}
inline std::shared_ptr<const Vitaleon::Catalog> Vitaleon::catalogFor(const InstalledBank& bank){
    if(bank.path.empty())return initialCatalog_;
    return bank.folder?indexFolder(bank.path):indexBank(bank.path);
}
// Copies a bank from another drive into the plugin's Banks folder and returns the copy's
// path. A bank that is already there (same name and size) is not copied again. Throws with a
// sentence for the status line when it cannot.
inline std::string Vitaleon::installBank(const std::string& source){
    namespace fs=std::filesystem;std::error_code ec;
    const fs::path from(source),folder(mpc::contentPath("Banks"));
    fs::create_directories(folder,ec);ec.clear();
    const auto home=fs::weakly_canonical(folder,ec);ec.clear();const auto origin=fs::weakly_canonical(from,ec);ec.clear();
    if(vitaleon_storage::inside(origin,home))return source;   // an installed bank
    const auto size=fs::file_size(from,ec);if(ec)throw std::runtime_error("the bank file cannot be read.");
    fs::path target=folder/from.filename();
    if(fs::exists(target,ec)){
        if(fs::file_size(target,ec)==size&&!ec)return target.string();   // installed before
        for(int n=2;n<100;++n){target=folder/(from.stem().string()+" ("+std::to_string(n)+")"+from.extension().string());if(!fs::exists(target,ec))break;}}
    ec.clear();const auto space=fs::space(folder,ec);
    if(!ec&&space.available<size+uintmax_t(8)*1024*1024)throw std::runtime_error("not enough room on the plugin's drive.");
    fs::path part=target;part+=".part";
    {std::ifstream in(from,std::ios::binary);std::ofstream out(part,std::ios::binary|std::ios::trunc);
     if(!in||!out)throw std::runtime_error("the plugin's Banks folder cannot be written.");
     std::vector<char> buffer(size_t(1)<<20);uintmax_t done=0;int shown=-1;
     while(in){in.read(buffer.data(),std::streamsize(buffer.size()));const auto count=in.gcount();if(count<=0)break;
        out.write(buffer.data(),count);
        if(!out){out.close();fs::remove(part,ec);throw std::runtime_error("writing failed. Is the drive full?");}
        done+=uintmax_t(count);const int percent=size?int(done*100/size):100;
        if(percent/5!=shown){shown=percent/5;std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Installing on the MPC: "+std::to_string(percent)+" %";publishBrowser();}}
     out.flush();
     if(!out||done!=size){out.close();fs::remove(part,ec);throw std::runtime_error("the copy is incomplete. Was the drive removed?");}}
    fs::rename(part,target,ec);if(ec){fs::remove(part,ec);throw std::runtime_error("the copy could not be finished.");}
    return target.string();
}
inline bool Vitaleon::storageChanged(int index){
    if(index<storageCommands_||index>=storageCommands_+12)return false;
    // Every tap is one command, whatever value the toggle sends. The button goes
    // straight back to "off" and the display update that follows each command
    // shows it unlit: on the Live II the buttons used to stay lit after a tap and
    // looked like selections (user report, 0.3.18).
    publish(index,0);
    std::lock_guard<std::mutex> lock(browserMutex_);int cmd=index-storageCommands_;
    BrowserJob job{cmd,browser_.path,browser_.root};
    if(cmd>=6){size_t row=size_t(browser_.page*6+cmd-6);if(row>=browser_.entries.size())return true;auto entry=browser_.entries[row];job.path=entry.path;if(browser_.path.empty())job.root=entry.path;job.command=entry.directory?12:13;
        if(entry.program>=0){job.command=15;job.program=entry.program;}
        else if(!entry.directory){auto ext=std::filesystem::path(entry.path).extension().string();for(auto& ch:ext)ch=char(std::tolower((unsigned char)ch));if(ext==".vital")job.command=14;else if(entry.kind)job.command=16;}}
    if(browserJobs_.size()>=16)return true;
    browserJobs_.push_back(std::move(job));browserPending_.fetch_add(1);return true;
}
inline void Vitaleon::publishBrowser(){
    static std::atomic<unsigned> serial{0};float marker=float((++serial)%16000000)/16000000;
    for(int p=storageRows_;p<numParams()&&p<=activeBankText_+1;++p)publishNormalised(p,marker);
    // Three states per row: 0 empty, 1 OPEN (drive, folder, bank), 2 LOAD (sound, table, WAV, LFO shape).
    for(int row=0;row<6;++row){const int n=browser_.page*6+row;float state=0;
        if(n<int(browser_.entries.size()))state=browser_.entries[size_t(n)].sound||browser_.entries[size_t(n)].kind?1.0f:0.5f;
        publishNormalised(storageAvailable_+row,state);}
    browserDisplayDirty_.store(true);
}
inline std::shared_ptr<const Vitaleon::Catalog> Vitaleon::indexBank(const std::string& path){
    BankArchive bank(path);auto c=std::make_shared<Catalog>();c->path=path;c->name=std::filesystem::path(path).stem().string();std::set<std::string> entries,used;
    for(int i=0;i<factoryProgramCount_;++i){c->names.push_back(mpc::Plugin::programName(i));used.insert(c->names.back());}
    for(const auto& e:bank.entries()){
        auto ext=std::filesystem::path(e.name).extension().string();for(auto& a:ext)a=char(std::tolower((unsigned char)a));if(ext!=".vital")continue;
        BankArchive::check(entries.insert(e.name).second,"Duplicate preset entry in bank");
        BankArchive::check(!(e.flags&1)&&(e.method==0||e.method==8)&&e.size<=64*1024*1024&&e.compressed<=64*1024*1024,"Unsupported preset entry in bank");
        BankArchive::check(c->files.size()<4095,"Bank has more than 4095 presets");
        c->files.push_back({e.name,path,e.name,std::filesystem::path(e.name).stem().string(),c->name});
    }
    BankArchive::check(!c->files.empty(),"No Vital presets in this bank");
    std::sort(c->files.begin(),c->files.end(),[](const Asset& a,const Asset& b){return a.entry<b.entry;});
    auto shorten=[](const std::string& s,size_t limit){size_t n=std::min(s.size(),limit);while(n&&n<s.size()&&(uint8_t(s[n])&0xc0)==0x80)--n;return s.substr(0,n);};
    for(const auto& asset:c->files){auto name=shorten(asset.name,24);for(int n=2;used.count(name);++n){auto suffix=" #"+std::to_string(n);name=shorten(asset.name,24-suffix.size())+suffix;}used.insert(name);c->names.push_back(name);}
    return c;
}
// A folder of loose .vital files behaves like a bank: its sounds appear in the
// MPC PRESET list. Nothing is read here except the directory itself.
inline std::shared_ptr<const Vitaleon::Catalog> Vitaleon::indexFolder(const std::string& folder){
    namespace fs=std::filesystem;auto c=std::make_shared<Catalog>();c->path=folder;c->name=fs::path(folder).filename().string();if(c->name.empty())c->name=folder;std::set<std::string> used;
    // The folder of the sounds saved with SAVE SOUND has a name of its own in the bank selector.
    {std::error_code same;if(fs::equivalent(folder,userFolder("Sounds"),same)&&!same)c->name="My sounds";}
    for(int i=0;i<factoryProgramCount_;++i){c->names.push_back(mpc::Plugin::programName(i));used.insert(c->names.back());}
    std::error_code ec;
    for(fs::directory_iterator it(folder,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_regular_file(ec)){ec.clear();continue;}auto ext=it->path().extension().string();for(auto& ch:ext)ch=char(std::tolower((unsigned char)ch));if(ext!=".vital")continue;
        BankArchive::check(c->files.size()<4095,"Folder has more than 4095 presets");
        c->files.push_back({it->path().filename().string(),it->path().string(),"",it->path().stem().string(),c->name});
    }
    BankArchive::check(!c->files.empty(),"No Vital presets in this folder");
    // The plugin's own folder of single sounds has a name of its own in the bank selector.
    if(std::filesystem::equivalent(folder,mpc::contentPath("Presets"),ec)&&!ec)c->name="Loose presets";
    std::sort(c->files.begin(),c->files.end(),[](const Asset& a,const Asset& b){return a.label<b.label;});
    auto shorten=[](const std::string& s,size_t limit){size_t n=std::min(s.size(),limit);while(n&&n<s.size()&&(uint8_t(s[n])&0xc0)==0x80)--n;return s.substr(0,n);};
    for(const auto& asset:c->files){auto name=shorten(asset.name,24);for(int n=2;used.count(name);++n){auto suffix=" #"+std::to_string(n);name=shorten(asset.name,24-suffix.size())+suffix;}used.insert(name);c->names.push_back(name);}
    return c;
}
inline void Vitaleon::installCatalog(std::shared_ptr<const Catalog> c,bool lifecycleHeld,bool first){
    // Serialize with program selection/project transactions, but do not park DSP:
    // browsing and changing the list must not interrupt the sounding voice.
    std::unique_lock<std::mutex> life(lifecycleMutex_,std::defer_lock);if(!lifecycleHeld)life.lock();
    // A bank that is opened in LIBRARY or restored with a project preserves the current sound.
    // Keep its slot only when its stable path/entry still belongs to this catalog; otherwise
    // mark no slot. 'first' (the bank arrows of HOME): the caller loads the first sound of the
    // bank next, so the list is the bank's own and its first entry is the chosen one.
    int selected=currentProgram_.load();if(selected<0||selected>=factoryProgramCount_)selected=-1;
    if(first&&!c->files.empty())selected=factoryProgramCount_;
    else{std::lock_guard<std::mutex> state(stateMutex_);
        if(!selectedAsset_.path.empty())selected=-1;
        else for(int i=0;i<factoryProgramCount_;++i)if(loadedName_==mpc::Plugin::programName(i))selected=i;
        for(size_t i=0;i<c->files.size();++i)if(!selectedAsset_.path.empty()&&c->files[i].path==selectedAsset_.path&&c->files[i].entry==selectedAsset_.entry)selected=factoryProgramCount_+int(i);
    }
    if(selected<0){auto retained=std::make_shared<Catalog>(*c);selected=int(retained->names.size());retained->names.push_back("[Current sound]");retained->files.push_back({"","","","[Current sound]",c->name});c=retained;}
    std::atomic_store(&activeCatalog_,c);
    currentProgram_=selected;
    if(effect_)effect_->numPrograms=int(c->names.size());
    browserDisplayDirty_.store(true);
}
inline void Vitaleon::serviceBrowser(){
    BrowserJob job;
    {std::lock_guard<std::mutex> lock(browserMutex_);if(browserJobs_.empty())return;job=std::move(browserJobs_.front());browserJobs_.pop_front();}
    try{
        if(job.command==20)saveSound();   // SAVE on the SAVE SOUND page (0.3.44)
        else if(job.command==21)showMySounds();   // MY SOUNDS in the library (0.3.45)
        else if(job.command==23)initSound();   // the second tap on INIT SOUND (0.3.50)
        else if(job.command==24)showTablePage();   // the page of wavetables was opened or turned (0.3.53)
        else if(job.command==25){   // a table of the user's folder, from its row or from an arrow of the overview
            try{queueImport(job.path,job.program,false);}
            catch(const std::exception& e){std::lock_guard<std::mutex> lock(stateMutex_);loadMessage_="Cannot load "+std::filesystem::path(job.path).stem().string()+": "+e.what();setLoadStatus(2);}}
        else if(job.command==22){   // EXPORT: My sounds to the drive that is open in the library
            const std::string text=exportSounds(job.path.empty()?std::string():job.root);
            std::lock_guard<std::mutex> lock(browserMutex_);browser_.status=text;publishBrowser();}
        else if(job.command==13||job.command==5){
            // INSTALL on: a bank from another drive is copied into the plugin's Banks folder
            // first and then used from there, so it stays when the drive is removed.
            bool installed=false;
            if(job.command==13&&bankKeep_>=0&&getBool(bankKeep_)){
                {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Installing on the MPC...";publishBrowser();}
                std::string copy;
                try{copy=installBank(job.path);}catch(const std::exception& e){throw std::runtime_error(std::string("Not installed: ")+e.what());}
                if(copy!=job.path){installed=true;job.path=copy;std::error_code ec;auto base=std::filesystem::canonical(mpc::contentFolder(),ec);job.root=ec?mpc::contentFolder():base.string();}
            }
            {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Reading bank index...";publishBrowser();}
            auto c=job.command==5?initialCatalog_:indexBank(job.path);installCatalog(c);
            std::lock_guard<std::mutex> lock(browserMutex_);
            if(job.command==5)browser_.status="Installed sounds are in PRESET.";
            else{
                // An opened bank is shown like a folder: its sounds are rows with LOAD.
                std::vector<vitaleon_storage::Entry> sounds;
                for(size_t i=0;i<c->files.size();++i)sounds.push_back({c->files[i].name,job.path,false,true,int(i)});
                browser_.entries=std::move(sounds);browser_.path=job.path;browser_.root=job.root;browser_.page=0;
                browser_.status=(installed?"Installed on the MPC. ":"")+std::to_string(c->files.size())+" sounds. Tap LOAD.";
            }
            publishBrowser();
        }else if(job.command==17){
            // Bank selector: the previous or next installed bank becomes the PRESET list, and its
            // first sound is loaded (user on the device, 2026-11-25; up to 0.3.59 the sound that
            // played stayed). A step that ends on the bank it began on loads nothing.
            const auto banks=installedBanks();auto active=std::atomic_load(&activeCatalog_);
            const int count=int(banks.size());int current=job.program>0?-1:count;   // from a bank that is not installed: start at an end
            if(active){std::error_code ec;
                if(active==initialCatalog_||active->path.empty())current=count-1;
                else for(int i=0;i<count;++i)if(!banks[size_t(i)].path.empty()&&std::filesystem::equivalent(banks[size_t(i)].path,active->path,ec)&&!ec){current=i;break;}else ec.clear();}
            const int next=((current+job.program)%count+count)%count;
            {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Reading bank index...";publishBrowser();}
            const bool other=next!=current;
            auto c=catalogFor(banks[size_t(next)]);installCatalog(c,false,other);
            if(other&&!c->files.empty())startFirstSound();
            std::lock_guard<std::mutex> lock(browserMutex_);
            browser_.status="PRESET now lists: "+banks[size_t(next)].name+" ("+std::to_string(c->files.size())+" sounds).";
            publishBrowser();
        }else if(job.command==19){
            std::lock_guard<std::mutex> lock(browserMutex_);
            browser_.status=job.program?"INSTALL on: a bank you OPEN from a drive is copied into Vitaleon and stays.":"INSTALL off: a bank you OPEN is used from its drive.";
            publishBrowser();
        }else if(job.command==18){
            // The installed banks as rows: OPEN one to put it into PRESET.
            namespace fs=std::filesystem;std::error_code ec;
            const fs::path content(mpc::contentFolder());fs::create_directories(content/"Banks",ec);ec.clear();
            auto base=fs::canonical(content,ec);if(ec){base=content;ec.clear();}
            auto place=fs::canonical(content/"Banks",ec);if(ec){place=content/"Banks";ec.clear();}
            auto entries=vitaleon_storage::list(place.string(),base.string());
            std::lock_guard<std::mutex> lock(browserMutex_);
            browser_.entries=std::move(entries);browser_.path=place.string();browser_.root=base.string();browser_.page=0;
            browser_.status=browser_.entries.empty()?"No bank is installed yet.":"Installed banks. OPEN one to put it into PRESET.";
            publishBrowser();
        }else if(job.command==15){
            // A sound row of the open bank.
            auto active=std::atomic_load(&activeCatalog_);
            bool valid=active&&active->path==job.path&&job.program>=0&&size_t(job.program)<active->files.size()&&!active->files[size_t(job.program)].path.empty();
            {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status=valid?"Loading sound...":"Open the bank again.";publishBrowser();}
            if(valid)setProgram(factoryProgramCount_+job.program);
        }else if(job.command==16){
            // A wavetable, WAV or LFO shape: read here, swapped in by the next service pass.
            queueImport(job.path,-1,true);   // 0.3.53: a wavetable is copied into the user's folder as well
        }else if(job.command==14){
            // Single .vital file: list its folder in PRESET and load the tapped sound.
            {std::lock_guard<std::mutex> lock(browserMutex_);browser_.status="Loading sound...";publishBrowser();}
            auto c=indexFolder(std::filesystem::path(job.path).parent_path().string());installCatalog(c);
            auto active=std::atomic_load(&activeCatalog_);
            for(size_t i=0;i<active->files.size();++i)if(active->files[i].path==job.path){setProgram(factoryProgramCount_+int(i));break;}
        }else if(job.command==3||job.command==4){
            std::lock_guard<std::mutex> lock(browserMutex_);int last=std::max(0,(int(browser_.entries.size())-1)/6);browser_.page=mpc::clamp(browser_.page+(job.command==4?1:-1),0,last);publishBrowser();
        }else{
            if(job.command==0||job.command==2){job.path.clear();job.root.clear();}
            if(job.command==1){if(job.path==job.root){job.path.clear();job.root.clear();}else job.path=std::filesystem::path(job.path).parent_path().string();}
            auto entries=job.path.empty()?vitaleon_storage::discover(mpc::pluginFolder(),userFolder("Sounds"),userFolder("Wavetables")):vitaleon_storage::list(job.path,job.root);
            std::lock_guard<std::mutex> lock(browserMutex_);browser_.entries=std::move(entries);browser_.path=job.path;browser_.root=job.root;browser_.page=0;browser_.status=browser_.entries.empty()?"Nothing to open here.":browser_.path.empty()?"Choose a drive.":"OPEN a folder. LOAD a sound, table, WAV or LFO shape.";publishBrowser();
        }
    }catch(const std::exception& e){std::lock_guard<std::mutex> lock(browserMutex_);browser_.status=std::string("Cannot open: ")+e.what();publishBrowser();}
    browserPending_.fetch_sub(1);
}
inline bool Vitaleon::formatStorage(int index,std::string& out){
    if(index<storageRows_||index>activeBankText_+1)return false;
    if(index>=activeBankText_){auto c=std::atomic_load(&activeCatalog_);out=textPart(c?c->name:"Installed banks",index-activeBankText_);return true;}
    std::lock_guard<std::mutex> lock(browserMutex_);
    if(index<storageRows_+12){
        // Two columns per row: the kind, and the name in ONE label (the two halves of
        // a split name used to stand apart with a gap in between).
        int row=(index-storageRows_)/2,part=(index-storageRows_)%2,n=browser_.page*6+row;out.clear();
        if(n<int(browser_.entries.size())){const auto& e=browser_.entries[size_t(n)];
            if(part==0)out=e.directory?(browser_.path.empty()&&e.name!="My sounds"&&e.name!="User wavetables"?"DRIVE":"FOLDER"):e.sound?"SOUND":e.kind==1?"TABLE":e.kind==2?"WAV":e.kind==3?"LFO":"BANK";
            else{std::string name=e.directory||e.program>=0?e.name:std::filesystem::path(e.name).stem().string();
                if(name.size()>31){size_t head=19,tail=name.size()-9;
                    while(head>0&&(uint8_t(name[head])&0xc0)==0x80)--head;while(tail<name.size()&&(uint8_t(name[tail])&0xc0)==0x80)++tail;
                    name=name.substr(0,head)+"..."+name.substr(tail);}
                out=name;}
        }
    }
    else if(index>=storagePath_&&index<storagePath_+3){
        // One label: the last 31 characters of the place, cut at a folder boundary where possible.
        std::string place=browser_.path.empty()?"Drives":browser_.path;
        if(place.size()>31){size_t cut=place.size()-28;while(cut<place.size()&&(uint8_t(place[cut])&0xc0)==0x80)++cut;size_t slash=place.find('/',cut);if(slash!=std::string::npos&&slash+1<place.size()&&place.size()-slash<=28)cut=slash;place="..."+place.substr(cut);}
        out=index==storagePath_?place:std::string();
    }
    else if(index>=storageStatus_&&index<storageStatus_+4)out=textPart(browser_.status,index-storageStatus_);
    else if(index==storagePage_)out=std::to_string(browser_.page+1)+" / "+std::to_string(std::max(1,(int(browser_.entries.size())+5)/6))+" - "+std::to_string(browser_.entries.size())+" items";
    return true;
}
