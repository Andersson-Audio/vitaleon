// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurfaceMap.h"
inline void Vitaleon::createNavigation(){
    auto specification=Json::parse(surfaceMapJson);
    auto find=[&](std::string name){for(int i=0;i<numParams();++i)if(param(i).name==name)return i;std::replace(name.begin(),name.end(),'_',' ');for(int i=0;i<numParams();++i)if(param(i).name==name)return i;throw std::runtime_error("Unknown surface parameter: "+name);};
    rememberedPages_.fill(-1);
    // A page may name a parameter that is only appended further down (the places of the FX
    // overview, 0.3.36). Such a name is resolved at the end, once every parameter exists.
    struct Pending{size_t view,section,position;std::string name;};std::vector<Pending> pending;
    for(const auto& page:specification){SurfaceView view{page.at("group"),page.at("member"),{}};view.hidden=page.value("hidden",false);view.redirect=page.value("redirect",-1);
        if(page.at("chart").is_string()){const std::string chart=page.at("chart");const int at=int(surfaceViews_.size());
            if(chart=="mxtable")matrixView_=at;else if(chart=="pickgroup")pickGroupView_=at;else if(chart=="savesound")saveView_=at;else if(chart=="tables")tablesView_=at;else if(chart=="picktarget")pickTargetView_=at;else if(chart=="oscmix")oscOverviewView_=at;else if(chart=="library")libraryView_=at;else if(chart=="filters")filtersView_=at;else if(chart=="advanced")routeCurveView_=at;}for(const auto& section:page.at("sections")){std::vector<int> row;for(const auto& item:section.at("items")){int found=-1;if(item.get<std::string>().empty()){row.push_back(-1);continue;}   // an unused knob of a bank (0.3.36)
            try{found=find(item);}catch(const std::runtime_error&){pending.push_back({surfaceViews_.size(),view.sections.size(),row.size(),item.get<std::string>()});}row.push_back(found);}view.sections.push_back(std::move(row));}if(rememberedPages_[view.group]<0&&!view.hidden)rememberedPages_[view.group]=int(surfaceViews_.size());surfaceViews_.push_back(std::move(view));}
    // The overview is the leftmost tab of OSC and the page that group opens with, although
    // it stands last in the list (appended, so no earlier page changed its number).
    if(oscOverviewView_>=0)rememberedPages_[1]=oscOverviewView_;
    auto integer=[&](const char* id,const char* name,int max){auto p=mpc::intParam(id,name,0,max,0);p.automatable=false;return addParam(p);};
    uiGroup_=integer("ui_group","UI Group",6);uiPage_=integer("ui_page","UI Page",int(surfaceViews_.size())-1);uiSection_=integer("ui_section","UI Section",31);
    uiKnobs_=numParams();for(int i=0;i<16;++i){auto id="ui_knob_"+std::to_string(i),name="UI Control "+std::to_string(i+1);auto p=mpc::floatParam(id.c_str(),name.c_str(),0,1,0);p.automatable=false;addParam(p);uiTargets_[i].store(-1);slotAutomated_[i].store(-1);}
    // Appended after every existing parameter (index 1855). A momentary trigger for
    // the PICK buttons of the matrix rows: the plugin resets it to 0 at once, so
    // the same row can be picked again (a radio button that is already lit sends nothing).
    // Room for the names the slots take later, so that renaming never moves the text.
    for(int i=0;i<16;++i)params_[size_t(uiKnobs_+i)].name.reserve(64);
    uiPick_=integer("ui_pick","UI Pick",64);
    // Appended (index 1856): the Q-Link group of four last turned, 0 = slots 1-4.
    // The host does not tell a plugin which group its knobs are banked to.
    uiBank_=integer("ui_bank","UI Bank",3);
    // Appended (index 1857, 0.3.25): what LOAD on a WAV row of the library does with the
    // file. 0-2 = wavetable of oscillator 1-3, 3 = sample. A .vitaltable goes to the
    // oscillator chosen here (the last one chosen while this stands on Sample).
    importTarget_=integer("import_target","Import Target",3);
    // Appended (index 1858-1985, 0.3.26): the picture of the sample on the OSC overview,
    // the largest level in each of 128 slices. Written by refreshSamplePicture().
    sampleReadout_=numParams();for(int i=0;i<kSampleColumns;++i)addReadout("display_sample_"+std::to_string(i));
    // Appended (index 1986-1994, 0.3.27): texts of the loaded sound for HOME: author and
    // style (two parts), its comment (three lines) and the names of its four macros.
    soundInfoText_=numParams();for(int i=0;i<2;++i)addReadout("display_sound_info_"+std::to_string(i));
    soundNoteText_=numParams();for(int i=0;i<3;++i)addReadout("display_sound_note_"+std::to_string(i));
    macroNameText_=numParams();for(int i=0;i<4;++i)addReadout("display_macro_name_"+std::to_string(i));
    // Appended (index 1995, 0.3.28): a momentary trigger like UI Pick. 1-3 = show the list of
    // wavetables for that oscillator, 4 = the list of samples (see navigationChanged).
    uiBrowse_=integer("ui_browse","UI Browse",4);
    // Appended (index 1996-2283, 0.3.30): the pictures of the two filters on the FILTERS page,
    // the level at 144 frequencies each. Written by refreshFilterPicture().
    for(int f=0;f<2;++f){filterReadout_[size_t(f)]=numParams();for(int i=0;i<kFilterColumns;++i)addReadout("display_filter_"+std::to_string(f)+"_"+std::to_string(i));}
    createFilterProbes();
    // Appended (index 2284, 0.3.31): the name of the loaded sample, a text for the SAMPLE page.
    sampleNameText_=addReadout("display_sample_name");
    // Appended (index 2285-2924, 0.3.32): finer pictures. 64 more columns per wave picture
    // (256 with the two old blocks of 96), 192 more for the LFO and route curve (384), and
    // the sample in 256 slices for the SAMPLE page.
    for(int o=0;o<3;++o){waveFineReadout_[size_t(o)]=numParams();for(int i=0;i<64;++i)addReadout("display_wave_fine_"+std::to_string(o)+"_"+std::to_string(i));}
    curveFineReadout_=numParams();for(int i=0;i<192;++i)addReadout("display_lfo_fine_"+std::to_string(i));
    sampleFineReadout_=numParams();for(int i=0;i<kSampleFineColumns;++i)addReadout("display_sample_fine_"+std::to_string(i));
    // Appended (index 2925-3236, 0.3.34): the picture of the selected envelope, see updateDisplays.
    envShapeReadout_=numParams();for(int i=0;i<kEnvColumns;++i)addReadout("display_env_shape_"+std::to_string(i));
    {static const char* keys[9]={"delay","attack","hold","decay","sustain","release","attack_power","decay_power","release_power"};
     for(int e=0;e<6;++e)for(int k=0;k<9;++k)envShapeParams_[size_t(e)][size_t(k)]=controlIndex("env_"+std::to_string(e+1)+"_"+keys[k]);}
    // Appended (index 3237, 0.3.34): which picture an envelope page shows, see uiEnvView_.
    uiEnvView_=integer("ui_env_view","UI Env View",1);
    // Appended (index 3238-3256, 0.3.36): the FX overview in chain order, see fxSlotText_.
    fxSlotText_=numParams();for(int i=0;i<9;++i)addReadout("display_fx_slot_"+std::to_string(i));
    fxSlotOn_=numParams();for(int i=0;i<9;++i){auto id="fx_slot_on_"+std::to_string(i),name="FX Slot "+std::to_string(i+1);auto p=mpc::boolParam(id.c_str(),name.c_str(),false);p.automatable=false;addParam(p);}
    {static const char* keys[9]={"chorus","compressor","delay","distortion","eq","filter_fx","flanger","phaser","reverb"};
     for(int type=0;type<9;++type)fxOnParam_[size_t(type)]=controlIndex(std::string(keys[type])+"_on");}
    uiFxMove_=integer("ui_fx_move","UI FX Move",2);
    for(const auto& late:pending)surfaceViews_[late.view].sections[late.section][late.position]=find(late.name);
    // Appended (index 3257-3258, 0.3.39): the bank selector's command and the install switch.
    bankStep_=integer("bank_step","Bank Step",3);
    {auto p=mpc::boolParam("bank_keep","Install Bank",false);p.automatable=false;bankKeep_=addParam(p);}
    // Appended (index 3259 to 3274, 0.3.43): the colour classes of the eight routes on screen,
    // see matrixTint_.
    matrixTint_=numParams();
    for(int i=0;i<16;++i){const std::string id=std::string(i<8?"mx_source_tint_":"mx_target_tint_")+std::to_string(i%8+1),name=std::string(i<8?"Matrix Source Tint ":"Matrix Target Tint ")+std::to_string(i%8+1);integer(id.c_str(),name.c_str(),5);}
    // Appended (index 3275 to 3281, 0.3.44): SAVE SOUND. Two momentary commands, the CAPS
    // switch, and the texts of the name line and the status line.
    uiSave_=integer("ui_save","UI Save",2);uiKey_=integer("ui_key","UI Key",40);
    {auto p=mpc::boolParam("save_caps","Save Caps",false);p.automatable=false;saveCaps_=addParam(p);}
    saveNameText_=addReadout("save_name");saveStatusText_=numParams();for(int i=0;i<3;++i)addReadout("save_status_"+std::to_string(i));
    // Appended (index 3282, 0.3.45): MY SOUNDS and EXPORT of the library, a momentary command.
    libraryStep_=integer("library_step","Library Step",2);
    // Appended (index 3283, 0.3.50): INIT SOUND, a switch that is tapped twice.
    {auto p=mpc::boolParam("ui_init","UI Init",false);p.automatable=false;uiInit_=addParam(p);}
    // Appended (index 3284 to 3290, 0.3.52): the wavetables of the OSC overview. Two momentary
    // commands, the oscillator and the table of the page of factory tables, three names.
    tableStep_=integer("table_step","Table Step",6);uiTables_=integer("ui_tables","UI Tables",3);
    tableTarget_=integer("table_target","Table Target",3);tablePick_=integer("table_pick","Table Pick",kFactoryTables);
    tableNameText_=numParams();for(int i=0;i<3;++i)addReadout("display_table_name_"+std::to_string(i));
    findFactoryTables();
    // Appended (index 3291 to 3391, 0.3.53): the pages of the wavetable page. Page, page step,
    // marked row, empty flag, page text; then per row a tap, a flag and a name. The picture
    // values that 0.3.53 and 0.3.54 appended after them (to index 4159) are gone again: neither
    // version was released, and the 768 values made every page change slow (see FullTables.inc).
    tablePage_=integer("table_page","Table Page",31);tablePageStep_=integer("table_page_step","Table Page Step",2);
    tableRowMark_=integer("table_row_mark","Table Row Mark",kTableRows);tableEmpty_=integer("table_empty","Table Empty",1);
    tablePageText_=addReadout("display_table_page");
    tableRowTap_=numParams();for(int r=0;r<kTableRows;++r){const std::string id="table_row_tap_"+std::to_string(r),name="Table Row "+std::to_string(r+1);auto p=mpc::boolParam(id.c_str(),name.c_str(),false);p.automatable=false;addParam(p);}
    tableRowOn_=numParams();for(int r=0;r<kTableRows;++r)integer(("table_row_on_"+std::to_string(r)).c_str(),("Table Row On "+std::to_string(r+1)).c_str(),1);
    tableRowText_=numParams();for(int r=0;r<kTableRows;++r)addReadout("display_table_row_"+std::to_string(r));
    scanTableFiles();
    updateNavigation();
}
inline void Vitaleon::updateNavigation(){
    int page=mpc::clamp(getInt(uiPage_),0,int(surfaceViews_.size())-1);
    // A retired page (FILTER 2 up to 0.3.29; RANDOM and EXPRESSION up to 0.3.37) keeps its
    // number in the list so that no later page moved; a project saved on it opens the page
    // that holds its controls now.
    if(surfaceViews_[size_t(page)].redirect>=0){page=mpc::clamp(surfaceViews_[size_t(page)].redirect,0,int(surfaceViews_.size())-1);publish(uiPage_,float(page));}
    const auto& v=surfaceViews_[page];publish(uiGroup_,float(v.group));if(!v.hidden)rememberedPages_[v.group]=page;
    int section=mpc::clamp(getInt(uiSection_),0,int(v.sections.size())-1);publish(uiSection_,float(section));
    if(v.group==3)publish(envSelect_,float(v.member));
    if(v.group==4){publish(lfoSelect_,float(v.member));publish(curveTarget_,0);curveSelectionChanged_.store(true);inspectCurve_.store(true);}
    if(page==routeCurveView_){publish(curveTarget_,1);curveSelectionChanged_.store(true);inspectCurve_.store(true);}   // the route curve
    const auto& targets=v.sections[section];for(int i=0;i<16;++i)uiTargets_[i].store(i<int(targets.size())?targets[i]:-1);
    // Device, 0.3.35: after Q-LINKS MORE on an LFO page no group was framed, because the frame
    // stood on a knob group that layer does not use; it looked empty. The frame now moves to
    // the first group whenever the page or layer on screen has nothing in the current one.
    if(uiBank_>=0&&getInt(uiBank_)*4>=int(targets.size()))publish(uiBank_,0);
    slotEditMs_.store(0,std::memory_order_relaxed); // other controls now: every slot shows its new target
    if(slotNamesLive_.load(std::memory_order_relaxed))renameSlots();
    refreshProxies();
    pageTurnPass_.store(displayPasses_.load(std::memory_order_relaxed),std::memory_order_relaxed);pageTurnMs_.store(std::max(1LL,nowMs()),std::memory_order_relaxed);displayNow_.store(true,std::memory_order_relaxed);
    browserDisplayDirty_.store(true);
}
// What a Q-Link slot is called while it reaches this control: the engine's own name of the
// control where it has one, else the parameter's name; a place of the FX overview is called
// after the effect that stands there.
inline std::string Vitaleon::slotName(int target){
    std::string name;
    if(fxSlotOn_>=0&&target>=fxSlotOn_&&target<fxSlotOn_+9){const int text=fxSlotText_+(target-fxSlotOn_);formatFxSlot(text,getNormalised(text),name);return name;}
    name=param(target).name;
    for(const auto& b:bindings_)if(b.index==target){const std::string shown=vital::Parameters::getDetails(b.key).display_name;if(!shown.empty())name=shown;break;}
    std::replace(name.begin(),name.end(),'_',' ');
    bool start=true;for(char& c:name){if(start&&c>='a'&&c<='z')c=char(c-32);start=c==' ';}
    auto swap=[&](const char* from,const char* to){for(size_t at=name.find(from);at!=std::string::npos;at=name.find(from,at))name.replace(at,std::strlen(from),to);};
    swap("Oscillator","Osc");swap("Envelope","Env");swap("Frequency","Freq");swap("Resonance","Reso");
    if(name.rfind("Home ",0)==0)name.erase(0,5);
    return name;
}
// Called with navigationMutex_ held. Words that every control of the page shares at the start
// ("Reverb", "Osc 1") are left out: the page says them already and the MPC has little room.
inline void Vitaleon::renameSlots(){
    if(uiKnobs_<0)return;
    std::array<std::string,16> names;std::vector<std::vector<std::string>> words;
    for(int i=0;i<16;++i){const int target=uiTargets_[size_t(i)].load();if(target<0)continue;
        names[size_t(i)]=slotName(target);std::vector<std::string> parts;std::string part;
        for(char c:names[size_t(i)]){if(c==' '){if(!part.empty())parts.push_back(part);part.clear();}else part+=c;}
        if(!part.empty())parts.push_back(part);words.push_back(std::move(parts));}
    size_t shared=0;
    if(words.size()>1){
        size_t shortest=words[0].size();for(const auto& w:words)shortest=std::min(shortest,w.size());
        while(shared+1<shortest){bool same=true;for(const auto& w:words)same&=w[shared]==words[0][shared];if(!same)break;++shared;}
    }
    for(int i=0;i<16;++i){
        std::string name=names[size_t(i)];
        if(name.empty())name="Unused";
        else for(size_t dropped=0;dropped<shared;++dropped){const size_t space=name.find(' ');if(space==std::string::npos)break;name.erase(0,space+1);}
        if(name.size()>31)name.resize(31);
        params_[size_t(uiKnobs_+i)].name.assign(name);
    }
}
inline bool Vitaleon::navigationChanged(int index,float plain){
    if(uiGroup_<0)return false;
    if(index>=uiKnobs_&&index<uiKnobs_+16){
        int target=uiTargets_[index-uiKnobs_].load();if(target<0)return true;
        // Record the stable sound parameter, never a page-dependent UI slot.
        int automated=target;
        if(target>=homeControls_&&target<homeControls_+7)automated=homeTarget(target-homeControls_);
        if(target==homeFilter_)automated=controlIndex("filter_1_on");
        if(target==advancedValue_)automated=advancedTargets_[getInt(advancedSelector_)];
        if(target==effectType_)automated=effectOrder_;
        for(const auto& bank:banks_)for(size_t k=0;k<bank.proxies.size();++k)if(target==bank.proxies[k])automated=bank.targets[mpc::clamp(getInt(bank.selector)-1,0,int(bank.targets.size())-1)][k];
        // Knobs and faders on screen are bound to this very slot (0.3.22), so the
        // host moves the control it wrote and nothing has to be redrawn. Nothing here
        // calls into the host (0.3.23); flushSlotNotifications does that afterwards.
        const float before=get(target);
        setNormalised(target,plain);
        if(param(automated).automatable)slotAutomated_[size_t(index-uiKnobs_)].store(automated,std::memory_order_relaxed);
        // A switch or list on screen is bound to its own parameter: redraw it when it steps.
        if(param(target).kind!=mpc::Param::Kind::Float&&get(target)!=before)slotStepped_.store(target,std::memory_order_relaxed);
        // The frame on screen marks the four controls of the knob group in use.
        const int bank=(index-uiKnobs_)/4;
        if(uiBank_>=0&&getInt(uiBank_)!=bank){publish(uiBank_,float(bank));bankChanged_.store(true,std::memory_order_relaxed);}
        slotEditMs_.store(nowMs(),std::memory_order_relaxed);
        return true;
    }
    if(index==uiEnvView_&&uiEnvView_>=0){browserDisplayDirty_.store(true);return true;}   // the other picture: redraw the page
    if(index==uiFxMove_&&uiFxMove_>=0){const int which=int(std::lround(plain));
        // A momentary command like UI Pick: 1 = one place earlier, 2 = one place later. The
        // choice follows the effect, so the button can be pressed again to move it further.
        if(which==1||which==2){publish(uiFxMove_,0);
            int order[9];vital::utils::decodeFloatToOrder(order,get(effectOrder_),9);
            const int from=mpc::clamp(getInt(effectSlot_)-1,0,8),to=from+(which==1?-1:1);
            if(to>=0&&to<9){std::swap(order[from],order[to]);set(effectOrder_,vital::utils::encodeOrderToFloat(order,9));publish(effectSlot_,float(to+1));}
            refreshProxies();
            {std::lock_guard<std::mutex> lock(navigationMutex_);if(slotNamesLive_.load(std::memory_order_relaxed))renameSlots();}   // the places carry other effects now
            browserDisplayDirty_.store(true);}
        return true;}
    if(index==bankStep_&&bankStep_>=0){const int which=int(std::lround(plain));
        // A momentary command; the work (reading a bank's index) is done by the service thread.
        if(which>=1&&which<=3){publish(bankStep_,0);
            {std::lock_guard<std::mutex> lock(browserMutex_);
             if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{which==3?18:17,std::string(),std::string(),which==1?-1:1});browserPending_.fetch_add(1);}}
            if(which==3&&libraryView_>=0)navigateTo(libraryView_,0);}
        return true;}
    if(index==bankKeep_&&bankKeep_>=0){
        std::lock_guard<std::mutex> lock(browserMutex_);
        if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{19,std::string(),std::string(),plain>.5f?1:0});browserPending_.fetch_add(1);}
        return true;}
    if(index==libraryStep_&&libraryStep_>=0){const int which=int(std::lround(plain));
        if(which>0){publish(libraryStep_,0);std::lock_guard<std::mutex> lock(browserMutex_);
            if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{which==1?21:22,browser_.path,browser_.root});browserPending_.fetch_add(1);}}
        return true;}
    if(index==uiKey_&&uiKey_>=0){const int key=int(std::lround(plain));if(key>0){publish(uiKey_,0);saveKey(key);}return true;}
    if(index==tableStep_&&tableStep_>=0){const int which=int(std::lround(plain));
        if(which>=1&&which<=6){publish(tableStep_,0);stepTable((which-1)/2,which%2?-1:1);}
        return true;}
    if(index==uiTables_&&uiTables_>=0){const int which=int(std::lround(plain));
        if(which>=1&&which<=3){publish(uiTables_,0);if(tablesView_>=0){publish(tableTarget_,float(which));publishTablePick();requestTablePage();navigateTo(tablesView_,0);}}
        return true;}
    if(index==tableTarget_&&tableTarget_>=0){publishTablePick();return true;}
    if(index==tablePageStep_&&tablePageStep_>=0){const int which=int(std::lround(plain));
        if(which==1||which==2){publish(tablePageStep_,0);
            // The factory page, the pages of the groups, the user's (see tablePages).
            const int pages=int(tablePages().size());int page=mpc::clamp(getInt(tablePage_),0,pages-1);
            page=which==2?(page+1)%pages:(page+pages-1)%pages;publish(tablePage_,float(page));requestTablePage();}
        return true;}
    if(tableRowTap_>=0&&index>=tableRowTap_&&index<tableRowTap_+kTableRows){if(plain>.5f){publish(index,0);tapTableRow(index-tableRowTap_);}return true;}
    if(index==tablePick_&&tablePick_>=0){pickTable(int(std::lround(plain)));return true;}
    if(index==uiInit_&&uiInit_>=0){
        long long armed=initArmedMs_.load();
        if(plain>.5f){
            if(!armed){std::lock_guard<std::mutex> lock(stateMutex_);initPriorMessage_=loadMessage_;loadMessage_=kInitAsk;setLoadStatus(0);}
            initArmedMs_.store(std::max(1LL,nowMs()));
        }else if(armed&&nowMs()-armed<=initConfirmMs_.load()&&initArmedMs_.compare_exchange_strong(armed,0)){
            std::lock_guard<std::mutex> lock(browserMutex_);
            if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{23,std::string(),std::string()});browserPending_.fetch_add(1);}
        }
        return true;}
    if(index==uiSave_&&uiSave_>=0){const int what=int(std::lround(plain));
        if(what>0){publish(uiSave_,0);
            if(what==1)openSave();
            else{std::lock_guard<std::mutex> lock(browserMutex_);if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{20,std::string(),std::string()});browserPending_.fetch_add(1);}}}
        return true;}
    if(index==uiPick_){const int route=int(std::lround(plain));
        if(route>0&&matrixView_>=0&&getInt(uiPage_)==matrixView_){publish(uiPick_,0);set(routeSelect_,float(route));navigateTo(pickGroupView_,0);}
        return true;}
    if(index==uiBrowse_){const int which=int(std::lround(plain));
        // The library opens on the plugin's own Wavetables or Samples folder with the WAV target
        // set to where the button was pressed, so a tap on a row loads it there. From that
        // folder UP and DRIVES lead to every other drive as usual.
        if(which>=1&&which<=4&&libraryView_>=0){publish(uiBrowse_,0);
            if(importTarget_>=0)publish(importTarget_,float(which-1));
            std::error_code ec;namespace fs=std::filesystem;
            const fs::path content=fs::path(mpc::contentFolder()),folder=content/(which==4?"Samples":"Wavetables");
            auto base=fs::canonical(content,ec);if(ec){base=content;ec.clear();}
            auto place=fs::canonical(folder,ec);if(ec){place=folder;ec.clear();}
            {std::lock_guard<std::mutex> lock(browserMutex_);
             if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{12,place.string(),base.string()});browserPending_.fetch_add(1);}}
            navigateTo(libraryView_,0);}
        return true;}
    if(index!=uiGroup_&&index!=uiPage_&&index!=uiSection_)return false;
    std::lock_guard<std::mutex> lock(navigationMutex_);
    if(index==uiGroup_){publish(uiPage_,float(rememberedPages_[mpc::clamp(int(std::lround(plain)),0,6)]));publish(uiSection_,0);}
    // Entering the matrix shows the page of the selected route.
    else if(index==uiPage_)publish(uiSection_,getInt(uiPage_)==matrixView_?float((getInt(routeSelect_)-1)/8):0.0f);
    // Target picker step 1 -> 2: a group button sets the section, the entry view shows that group.
    else if(index==uiSection_&&pickGroupView_>=0&&getInt(uiPage_)==pickGroupView_&&std::lround(plain)>0)publish(uiPage_,float(pickTargetView_));
    updateNavigation();return true;
}
inline void Vitaleon::navigateTo(int page,int section){
    if(page<0||uiPage_<0)return;
    std::lock_guard<std::mutex> lock(navigationMutex_);publish(uiPage_,float(page));publish(uiSection_,float(section));updateNavigation();
}
inline void Vitaleon::refreshNavigation(){
    if(uiKnobs_<0)return;
    const bool moving=slotEditMs_.load(std::memory_order_relaxed)!=0;
    for(int i=0;i<16;++i){int target=uiTargets_[i].load();
        if(target<0){publishNormalised(uiKnobs_+i,0);continue;}
        // While it moves, a stepped target keeps the knob's own position inside its
        // step: writing the step's value back would undo the small increments of a
        // turning Q-Link. At rest, and after a page change, the slot shows the target.
        const auto& t=param(target);
        if(moving&&t.kind!=mpc::Param::Kind::Float&&t.toPlain(getNormalised(uiKnobs_+i))==get(target))continue;
        publishNormalised(uiKnobs_+i,getNormalised(target));}
}
// Service thread, no lock held. See slotAutomated_.
inline void Vitaleon::flushSlotNotifications(){
    if(uiKnobs_<0)return;
    const int stepped=slotStepped_.exchange(-1,std::memory_order_relaxed);if(stepped>=0)notifyHostParameterChanged(stepped);
    // One parameter, not the page: the frame follows at once where the host redraws
    // on this call; the page redraw at rest covers a host that does not.
    if(bankChanged_.exchange(false,std::memory_order_relaxed)){notifyHostParameterChanged(uiBank_);bankRedraw_.store(true,std::memory_order_relaxed);}
    const long long last=slotEditMs_.load(std::memory_order_relaxed);
    if(last==0||nowMs()-last<kSlotRestMs)return;
    for(auto& slot:slotAutomated_){const int sound=slot.exchange(-1,std::memory_order_relaxed);if(sound>=0)notifyHostParameterChanged(sound);}
    if(bankRedraw_.exchange(false,std::memory_order_relaxed))requestDisplayUpdate();
    long long expected=last;slotEditMs_.compare_exchange_strong(expected,0,std::memory_order_relaxed);
}
