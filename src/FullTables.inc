// SPDX-License-Identifier: GPL-3.0-or-later
// 0.3.53: every wavetable that can be loaded, the user's own folder, and their small pictures.
//
// The list (tableFiles_) holds the files of the table selector first (the factory tables and
// whatever else lies in the plugin's own Wavetables folder), then the files of the user's
// folder <drive>/Vitaleon/Wavetables. That folder stands beside "VST2 Plugins", like the
// folder of the saved sounds, so an update of the plugin leaves it alone. A table that is
// loaded in LIBRARY from another drive is copied there (keepTable).
//
// The page of wavetables shows the 32 factory tables on its first page, as pictures made
// with the skin. Then come the pages of every folder of tables that ships with the plugin
// (0.3.59: Fizmatic, 54 tables on two pages), then the user's. Every such page shows 32
// tables; nobody knows in advance
// what those are, so the plugin reads each file and draws its picture itself: 25 points
// along the first cycle, each one of 11 levels. The skin shows them as one line of 24
// segments; a segment is a part of the skin that knows its two ends.
//
// What the MPC charges for (measured on a Live II with five test copies, 2026-11-24, time
// from a page change until the screen is drawn again):
//   0.3.52                                              0.8 s
//   0.3.54 without picture parts and picture values     0.8 s
//   0.3.54 without picture parts, with their 768 values 2.6 s
//   0.3.54 with the parts on a strip of two frames      2.4 s
//   0.3.54 (768 parts on a strip of 730 frames)         3.6 s
// So 768 more parameters cost 1.8 s (101 more cost nothing: 3392 are fast, 4160 are not),
// and a film strip costs by its frames: about 2 microseconds for every frame of every part
// that uses it, on every page change, whether the part is on screen or not. Hence no
// parameters of their own for the pictures, and 1 + 11 x 11 frames instead of 1 + 27 x 27.

// Paths are compared as written by the directory listing that produced them.
namespace vitaleon_tables{
inline std::string lower(std::string text){for(auto& ch:text)ch=char(std::tolower((unsigned char)ch));return text;}
// The first cycle of a wavetable WAV, read without loading the file: the header, then as
// many samples as one cycle has. Cycle length as in wavetableCycles: what a "clm " chunk
// states, else 2048, and a file shorter than two such cycles is one cycle.
inline std::vector<float> wavCycle(const std::string& path){
    std::ifstream in(std::filesystem::path(path),std::ios::binary);char head[12]{};
    if(!in.read(head,12)||std::memcmp(head,"RIFF",4)||std::memcmp(head+8,"WAVE",4))return {};
    int format=0,channels=0,bits=0,stated=0;uint32_t dataSize=0;std::streamoff dataAt=0;
    for(int guard=0;guard<256;++guard){unsigned char chunk[8]{};if(!in.read(reinterpret_cast<char*>(chunk),8))break;
        const uint32_t size=uint32_t(chunk[4])|uint32_t(chunk[5])<<8|uint32_t(chunk[6])<<16|uint32_t(chunk[7])<<24;const std::streamoff at=in.tellg();
        if(!std::memcmp(chunk,"fmt ",4)){unsigned char f[40]{};in.read(reinterpret_cast<char*>(f),std::streamsize(std::min<uint32_t>(size,40)));in.clear();
            format=f[0]|f[1]<<8;channels=f[2]|f[3]<<8;bits=f[14]|f[15]<<8;if(format==0xfffe&&size>=26)format=f[24]|f[25]<<8;}
        else if(!std::memcmp(chunk,"clm ",4)){char text[24]{};in.read(text,std::streamsize(std::min<uint32_t>(size,23)));in.clear();
            if(!std::memcmp(text,"<!>",3)){const int n=std::atoi(text+3);if(n>=8&&n<=65536)stated=n;}}
        else if(!std::memcmp(chunk,"data",4)){dataAt=at;dataSize=size;}
        in.seekg(at+std::streamoff(size)+std::streamoff(size&1));if(!in)break;}
    in.clear();
    const int bytes=bits/8;if(!dataAt||channels<1||channels>8||!(format==1||format==3)||!(bytes>=1&&bytes<=4)||(format==3&&bytes!=4))return {};
    const uint64_t total=dataSize/uint64_t(channels*bytes);if(total<8)return {};
    uint64_t cycle=stated?uint64_t(stated):(total<4096?total:2048);cycle=std::min<uint64_t>(std::min<uint64_t>(cycle,total),65536);
    std::vector<unsigned char> raw(size_t(cycle)*size_t(channels*bytes));
    in.seekg(dataAt);if(!in.read(reinterpret_cast<char*>(raw.data()),std::streamsize(raw.size())))return {};
    std::vector<float> out;out.resize(size_t(cycle));
    for(size_t i=0;i<out.size();++i){const unsigned char* p=raw.data()+i*size_t(channels*bytes);float v=0;
        if(format==3){std::memcpy(&v,p,4);}
        else if(bytes==1)v=(float(p[0])-128.0f)/128.0f;
        else if(bytes==2)v=float(int16_t(uint16_t(p[0])|uint16_t(p[1])<<8))/32768.0f;
        else if(bytes==3)v=float(int32_t((uint32_t(p[0])<<8|uint32_t(p[1])<<16|uint32_t(p[2])<<24))>>8)/8388608.0f;
        else v=float(int32_t(uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24))/2147483648.0f;
        out[i]=std::isfinite(v)?v:0.0f;}
    return out;
}
// The first cycle of a .vitaltable whose first source holds audio: a drawn wave ("Wave
// Source", 2048 floats) or a sampled one ("Audio File Source", 16-bit audio and a window).
// Sources that are computed (lines, tones) have no picture here.
inline std::vector<float> tableCycle(const std::string& path){
    std::error_code ec;const auto size=std::filesystem::file_size(std::filesystem::path(path),ec);if(ec||size>32u*1024u*1024u)return {};
    std::ifstream in(std::filesystem::path(path),std::ios::binary);std::string text(size_t(size),'\0');if(!in.read(text.data(),std::streamsize(text.size())))return {};
    const nlohmann::json table=nlohmann::json::parse(text,nullptr,false);if(!table.is_object())return {};
    const auto groups=table.find("groups");if(groups==table.end()||!groups->is_array()||groups->empty()||!(*groups)[0].is_object())return {};
    const auto parts=(*groups)[0].find("components");if(parts==(*groups)[0].end()||!parts->is_array()||parts->empty()||!(*parts)[0].is_object())return {};
    const auto& source=(*parts)[0];const std::string kind=source.value("type",std::string());
    const auto frames=source.find("keyframes");const bool keyed=frames!=source.end()&&frames->is_array()&&!frames->empty()&&(*frames)[0].is_object();
    auto decode=[](const nlohmann::json& holder,const char* key){MemoryOutputStream bytes;const auto it=holder.find(key);if(it!=holder.end()&&it->is_string())Base64::convertFromBase64(bytes,it->get<std::string>());return bytes;};
    if(kind=="Wave Source"&&keyed){auto bytes=decode((*frames)[0],"wave_data");std::vector<float> out(bytes.getDataSize()/sizeof(float));
        if(!out.empty())std::memcpy(out.data(),bytes.getData(),out.size()*sizeof(float));for(auto& v:out)if(!std::isfinite(v))v=0;return out;}
    if(kind=="Audio File Source"){auto bytes=decode(source,"audio_file");const size_t count=bytes.getDataSize()/sizeof(int16_t);if(count<8)return {};
        double window=2048,start=0;{const auto w=source.find("window_size");if(w!=source.end()&&w->is_number())window=w->get<double>();}
        if(keyed){const auto s=(*frames)[0].find("start_position");if(s!=(*frames)[0].end()&&s->is_number())start=s->get<double>();}
        const size_t length=size_t(mpc::clamp(window,8.0,65536.0)),from=std::min(size_t(std::max(0.0,start)),count>length?count-length:0);
        std::vector<int16_t> pcm(count);std::memcpy(pcm.data(),bytes.getData(),count*sizeof(int16_t));
        std::vector<float> out(std::min(length,count-from));for(size_t i=0;i<out.size();++i)out[i]=float(pcm[from+i])/32768.0f;return out;}
    return {};
}
}

// Every table that can be loaded, in the order the arrows of the OSC overview walk through.
inline void Vitaleon::scanTableFiles(){
    namespace fs=std::filesystem;std::vector<TableFile> all;
    // A table that lies in a folder of the plugin's Wavetables (not Factory) belongs to the group of
    // that name and gets pages of its own; loose files and tables out of banks go with the user's.
    auto groupOf=[](const Asset& asset){std::string group;if(!asset.entry.empty())return group;
        const size_t slash=asset.label.find('/');if(slash==std::string::npos||slash==0)return group;
        group=asset.label.substr(0,slash);if(group=="Factory"){group.clear();return group;}
        for(auto& ch:group)ch=char(std::toupper((unsigned char)ch));if(group.size()>10)group.resize(10);return group;};
    for(size_t i=1;i<assets_[1].size();++i){const auto& asset=assets_[1][i];const int factory=factoryNumber(int(i));
        all.push_back({asset.name,asset.entry.empty()?asset.path:std::string(),int(i),factory,factory?std::string():groupOf(asset)});}
    std::vector<TableFile> mine;std::error_code ec;const fs::path folder(userFolder("Wavetables"));
    if(fs::is_directory(folder,ec))for(fs::recursive_directory_iterator it(folder,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end&&mine.size()<2048;it.increment(ec)){
        if(!it->is_regular_file(ec)){ec.clear();continue;}
        const std::string ext=vitaleon_tables::lower(it->path().extension().string()),stem=it->path().stem().string();
        if((ext!=".wav"&&ext!=".vitaltable")||stem.empty()||stem[0]=='.')continue;
        mine.push_back({stem,it->path().string(),0,0,std::string()});}
    std::sort(mine.begin(),mine.end(),[](const TableFile& a,const TableFile& b){const auto x=vitaleon_tables::lower(a.name),y=vitaleon_tables::lower(b.name);return x!=y?x<y:a.path<b.path;});
    all.insert(all.end(),mine.begin(),mine.end());
    std::lock_guard<std::mutex> lock(tableMutex_);tableFiles_=std::move(all);userRows_.clear();
}
// The pages in the order of the arrows: FACTORY, the groups as they come in the list (by
// name), USER. A group has as many pages as its tables need; the user's has one at least,
// which says how to fill it. No more pages than the page number can count.
inline std::vector<Vitaleon::TablePage> Vitaleon::tablePages(){
    std::vector<TablePage> pages(1);pages[0].name="FACTORY";
    std::vector<std::pair<std::string,std::vector<int>>> groups;std::vector<int> mine;
    {std::lock_guard<std::mutex> lock(tableMutex_);
     for(size_t i=0;i<tableFiles_.size();++i){const auto& file=tableFiles_[i];if(file.factory)continue;
        if(file.group.empty()){mine.push_back(int(i));continue;}
        auto at=std::find_if(groups.begin(),groups.end(),[&](const auto& g){return g.first==file.group;});
        if(at==groups.end()){groups.push_back({file.group,{}});at=groups.end()-1;}
        at->second.push_back(int(i));}}
    auto add=[&](const std::string& name,const std::vector<int>& places,size_t least){
        const size_t count=std::max(least,(places.size()+size_t(kTableRows)-1)/size_t(kTableRows));
        for(size_t k=0;k<count;++k){TablePage page;page.name=name+" "+std::to_string(k+1)+" OF "+std::to_string(count);
            for(size_t p=k*size_t(kTableRows);p<places.size()&&p<(k+1)*size_t(kTableRows);++p)page.places.push_back(places[p]);
            pages.push_back(std::move(page));}};
    for(const auto& group:groups)add(group.first,group.second,0);
    add("USER",mine,1);
    const size_t most=size_t(param(tablePage_).max)+1;if(pages.size()>most)pages.resize(most);
    return pages;
}
// The picture of a table: 25 points along its first cycle, each a level from 0 (bottom) to 10
// (top), 5 is the zero line. Called on the browser thread only; a file is read once and its
// picture kept while the plugin runs.
inline std::array<unsigned char,Vitaleon::kTablePoints> Vitaleon::tablePicture(const TableFile& file){
    std::array<unsigned char,kTablePoints> levels;levels.fill((kTableLevels-1)/2);
    if(file.path.empty())return levels;
    std::error_code ec;const auto size=std::filesystem::file_size(std::filesystem::path(file.path),ec);
    const std::string key=file.path+"|"+std::to_string(ec?0:size);
    const auto known=tablePictures_.find(key);if(known!=tablePictures_.end())return known->second;
    std::vector<float> cycle;
    try{cycle=vitaleon_tables::lower(std::filesystem::path(file.path).extension().string())==".wav"?vitaleon_tables::wavCycle(file.path):vitaleon_tables::tableCycle(file.path);}
    catch(const std::exception&){cycle.clear();}
    if(cycle.size()>=8){float peak=0;for(float v:cycle)peak=std::max(peak,std::abs(v));
        if(peak>1e-9f)for(int point=0;point<kTablePoints;++point){
            // A point shows the largest swing of the stretch of the cycle around it, with its sign:
            // a slow wave keeps its shape, and a fast one is seen to be busy instead of averaging to
            // nothing. The stretches of neighbouring points touch.
            const double span=double(cycle.size())/double(kTablePoints-1),centre=double(point)*span;
            const size_t from=size_t(std::max(0.0,centre-span*.5)),to=std::min(cycle.size(),std::max(from+1,size_t(centre+span*.5)));float most=0;
            for(size_t i=from;i<to;++i)if(std::abs(cycle[i])>std::abs(most))most=cycle[i];
            const double value=double(most)/peak;levels[size_t(point)]=static_cast<unsigned char>(mpc::clamp(int(std::lround((value+1.0)*.5*(kTableLevels-1))),0,kTableLevels-1));}}
    if(tablePictures_.size()>4096)tablePictures_.clear();
    tablePictures_[key]=levels;return levels;
}
// Fills the page of wavetables that is on screen (browser thread): reads the user's folder
// again, so a file that was just copied there is listed, and draws the rows of the page.
inline void Vitaleon::showTablePage(){
    if(tablePage_<0)return;
    const long long began=nowMs();
    scanTableFiles();
    std::vector<TableFile> rows;const auto pages=tablePages();const int page=mpc::clamp(getInt(tablePage_),0,int(pages.size())-1);
    {std::lock_guard<std::mutex> lock(tableMutex_);
     for(int place:pages[size_t(page)].places)if(size_t(place)<tableFiles_.size()&&userRows_.size()<size_t(kTableRows)){userRows_.push_back(place);rows.push_back(tableFiles_[size_t(place)]);}}
    if(page!=getInt(tablePage_))publish(tablePage_,float(page));
    std::array<std::array<unsigned char,kTablePoints>,kTableRows> pictures{};
    for(size_t r=0;r<rows.size();++r)pictures[r]=tablePicture(rows[r]);
    // The pictures first, so that a row is never shown with what the wave pictures left in its
    // values. They go into the values of the three wave pictures (see the top of this file);
    // not when the page was left meanwhile, and not on the factory page, whose rows are images.
    if(page>0&&getInt(uiPage_)==tablesView_){
        waveStale_.store(7,std::memory_order_relaxed);const int frames=1+kTableLevels*kTableLevels;
        for(int r=0;r<kTableRows;++r){const bool on=size_t(r)<rows.size();
            // A part of the picture is the line from one point to the next: 0 is "no picture", then
            // 11 x 11 pairs of levels. Neighbouring parts share a point, so the line is unbroken.
            for(int part=0;part<kTablePoints-1;++part){const int index=on?1+pictures[size_t(r)][size_t(part)]*kTableLevels+pictures[size_t(r)][size_t(part+1)]:0;
                const int place=r*(kTablePoints-1)+part;publishNormalised(waveColumn(place/kWaveColumns,place%kWaveColumns),float(index)/float(frames-1));}}}
    {std::lock_guard<std::mutex> lock(tableMutex_);
     for(int r=0;r<kTableRows;++r)tableRowName_[size_t(r)]=size_t(r)<rows.size()?rows[size_t(r)].name:std::string();
     tablePageName_=pages[size_t(page)].name;}
    for(int r=0;r<kTableRows;++r){const bool on=size_t(r)<rows.size();
        publish(tableRowOn_+r,on?1.0f:0.0f);
        publishNormalised(tableRowText_+r,float((std::hash<std::string>{}(tableRowName_[size_t(r)])|1)&0xffffff)/16777216.0f);}
    publishNormalised(tablePageText_,float((std::hash<std::string>{}(tablePageName_)|1)&0xffffff)/16777216.0f);
    publish(tableEmpty_,page>0&&rows.empty()?1.0f:0.0f);
    publishTablePick();tablePageMs_.store(int(nowMs()-began),std::memory_order_relaxed);browserDisplayDirty_.store(true);
}
inline bool Vitaleon::formatTableRow(int index,std::string& out){
    if(tableRowText_<0)return false;
    if(index==tablePageText_){std::lock_guard<std::mutex> lock(tableMutex_);out=tablePageName_.empty()?std::string("FACTORY"):tablePageName_;return true;}
    if(index<tableRowText_||index>=tableRowText_+kTableRows)return false;
    std::lock_guard<std::mutex> lock(tableMutex_);out=textPart(tableRowName_[size_t(index-tableRowText_)],0);return true;
}
// Loads the table at a place of the list into an oscillator. A file of the selector goes
// through the selector, as before; a file of the user's folder is read on the browser thread.
inline void Vitaleon::loadTableFile(int place,int oscillator){
    if(oscillator<0||oscillator>2)return;
    TableFile file;{std::lock_guard<std::mutex> lock(tableMutex_);if(place<0||size_t(place)>=tableFiles_.size())return;file=tableFiles_[size_t(place)];}
    if(file.selector>0){if(getInt(tableSelect_[size_t(oscillator)])!=file.selector)set(tableSelect_[size_t(oscillator)],float(file.selector));return;}
    std::lock_guard<std::mutex> lock(browserMutex_);
    if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{25,file.path,std::string(),oscillator});browserPending_.fetch_add(1);}
}
// A tap on a row of a page of user tables.
inline void Vitaleon::tapTableRow(int row){
    int place=-1;{std::lock_guard<std::mutex> lock(tableMutex_);if(row>=0&&size_t(row)<userRows_.size())place=userRows_[size_t(row)];}
    if(place>=0)loadTableFile(place,mpc::clamp(getInt(tableTarget_),1,3)-1);
}
// The arrows beside a table's name on the OSC overview: the table before or after the one
// the oscillator plays, round the whole list. From a table that came with a sound, a step
// goes to the first or the last table.
inline void Vitaleon::stepTable(int oscillator,int direction){
    if(oscillator<0||oscillator>2)return;
    const int selection=getInt(tableSelect_[size_t(oscillator)]);int count=0,at=-1;
    {std::lock_guard<std::mutex> lock(tableMutex_);count=int(tableFiles_.size());const std::string& mine=userTablePath_[size_t(oscillator)];
     for(int i=0;i<count&&at<0;++i){const auto& file=tableFiles_[size_t(i)];
        if(selection>0?file.selector==selection:(!mine.empty()&&file.selector==0&&file.path==mine))at=i;}}
    if(count<1){std::lock_guard<std::mutex> lock(stateMutex_);loadMessage_="No wavetable files are installed.";setLoadStatus(2);return;}
    loadTableFile(direction>0?(at<0||at>=count-1?0:at+1):(at<=0?count-1:at-1),oscillator);
}
// The marks of the page of wavetables: the factory table its oscillator plays (first page)
// and the row of the user page that holds the table it plays.
inline void Vitaleon::publishTablePick(){
    if(tablePick_<0)return;
    const int oscillator=mpc::clamp(getInt(tableTarget_),1,3)-1,selection=getInt(tableSelect_[size_t(oscillator)]);
    const int number=factoryNumber(selection);int row=0;
    {std::lock_guard<std::mutex> lock(tableMutex_);
     for(int o=0;o<3;++o)if(getInt(tableSelect_[size_t(o)])>0)userTablePath_[size_t(o)].clear();   // a table of the selector took its place
     const std::string& mine=userTablePath_[size_t(oscillator)];
     for(size_t r=0;r<userRows_.size()&&!row;++r){const auto& file=tableFiles_[size_t(userRows_[r])];
        if(selection>0?file.selector==selection:(!mine.empty()&&file.selector==0&&file.path==mine))row=int(r)+1;}}
    bool changed=false;
    if(getInt(tablePick_)!=number){publish(tablePick_,float(number));changed=true;}
    if(tableRowMark_>=0&&getInt(tableRowMark_)!=row){publish(tableRowMark_,float(row));changed=true;}
    if(changed)browserDisplayDirty_.store(true);
}
// A table loaded in LIBRARY is kept: copied into the user's folder, unless it is there
// already or belongs to the plugin's own content. 'kept' receives the path of the file in
// the user's folder; the text returned is added to the message of the load.
inline std::string Vitaleon::keepTable(const std::string& source,std::string& kept){
    namespace fs=std::filesystem;kept.clear();std::error_code ec;
    const fs::path file(source),folder(userFolder("Wavetables"));
    auto inside=[&](const fs::path& root){std::error_code e;const fs::path a=fs::weakly_canonical(file,e);if(e)return false;const fs::path b=fs::weakly_canonical(root,e);if(e)return false;
        const fs::path rel=a.lexically_relative(b);return !rel.empty()&&*rel.begin()!=".."&&*rel.begin()!=".";};
    if(inside(folder)){kept=source;return std::string();}
    if(inside(fs::path(mpc::pluginFolder())))return std::string();
    const auto size=fs::file_size(file,ec);if(ec)return " Not kept: the file cannot be read again.";
    fs::create_directories(folder,ec);if(ec)return " Not kept: the folder Vitaleon/Wavetables cannot be made.";
    fs::path target=folder/file.filename();
    for(int n=2;n<200;++n){std::error_code e;if(!fs::exists(target,e))break;
        if(fs::file_size(target,e)==size&&!e){kept=target.string();return " Already in User Wavetables.";}
        target=folder/(file.stem().string()+" "+std::to_string(n)+file.extension().string());}
    const fs::path part(target.string()+".part");
    fs::copy_file(file,part,fs::copy_options::overwrite_existing,ec);if(!ec)fs::rename(part,target,ec);
    if(ec){std::error_code e;fs::remove(part,e);return " Not kept in User Wavetables: "+ec.message()+".";}
    kept=target.string();scanTableFiles();return " Kept in User Wavetables.";
}
