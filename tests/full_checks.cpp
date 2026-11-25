// SPDX-License-Identifier: GPL-3.0-or-later
#define VITALEON_TEST 1
#include "../src/Vitaleon.cpp"
#include "../src/engine/synth_lfo.h"
#include <iostream>
#include <sstream>
#include <new>
MPC_PLUGIN_ENTRY(Vitaleon)
static thread_local bool audit=false;
static thread_local size_t allocations=0;
static std::atomic<unsigned> displayUpdates{0};
static std::atomic<size_t> selectedCache{0},loadedCache{0};
static std::atomic<bool> displayOnAudio{false};
static std::atomic<int> automatedTarget{-1};
static std::atomic<unsigned> readoutUpdates{0};
static std::atomic<unsigned> automateCalls{0}; // every audioMasterAutomate, whatever the parameter
static std::atomic<int> cachedProgramCount{0};
static std::atomic<bool> validReentrantNames{true};
static AEffect displayEffect{};
// 0.3.56: what a run of values looked like at the moment of the last display update.
static std::atomic<int> watchFirst{-1},watchCount{0};static std::atomic<uint64_t> watchSeen{0};
static uint64_t watchHash(mpc::Plugin* plugin){uint64_t h=1469598103934665603ull;const int first=watchFirst.load(),count=watchCount.load();
 for(int i=0;i<count;++i){const float v=plugin->getNormalised(first+i);uint32_t bits;std::memcpy(&bits,&v,4);h=(h^bits)*1099511628211ull;}return h;}
static VstIntPtr displayHost(AEffect* effect,VstInt32 opcode,VstInt32 index,VstIntPtr,void*,float){
 if(opcode==audioMasterAutomate){automateCalls.fetch_add(1);if(static_cast<mpc::Plugin*>(effect->object)->param(index).automatable)automatedTarget.store(index);else readoutUpdates.fetch_add(1);}
 if(opcode==audioMasterUpdateDisplay){
  if(audit)displayOnAudio.store(true);
  auto* plugin=static_cast<mpc::Plugin*>(effect->object);
  if(effect->dispatcher){for(int i=0;i<effect->numPrograms;++i){char name[256]{};auto ok=effect->dispatcher(effect,effGetProgramNameIndexed,i,0,name,0);if(!ok||!name[0])validReentrantNames.store(false);}cachedProgramCount.store(effect->numPrograms);}
  for(int p=0;p<plugin->numParams();++p){
   if(plugin->param(p).id=="library_selected_0")selectedCache.store(std::hash<std::string>{}(plugin->displayText(p)));
   // Reentrant string queries acquire the synth state mutex: the notification
   // must happen outside it, not from setLoadStatus while loading.
   if(plugin->param(p).id=="library_loaded_0")loadedCache.store(std::hash<std::string>{}(plugin->displayText(p)));
  }
  if(watchFirst.load()>=0)watchSeen.store(watchHash(plugin));
  displayUpdates.fetch_add(1);
 }
 return opcode==audioMasterVersion?2400:opcode==audioMasterUpdateDisplay?1:0;
}
void* operator new(size_t n){if(audit)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}void operator delete[](void* p,size_t)noexcept{std::free(p);}
int main(int argc,char** argv){
 // These checks call the plugin from the thread that also renders its audio, so nothing can
 // fade while a call waits. The one check with a real audio thread (0.3.47) switches it on.
 Vitaleon::parkFadeWaitMs_.store(0);
 if(argc>1&&std::string(argv[1])=="--native-bank"){
  AEffect* e=VSTPluginMain(displayHost);auto* p=static_cast<Vitaleon*>(e->object);
  auto d=[&](int op,int ix=0,VstIntPtr val=0,void* ptr=nullptr,float opt=0){return e->dispatcher(e,op,ix,val,ptr,opt);};
  d(effOpen);d(effSetSampleRate,0,0,nullptr,44100);d(effSetBlockSize,0,512);d(effMainsChanged,0,1);
  float l[512]{},r[512]{};float* out[]={l,r};int loaded=0,rejected=0,failed=0;std::set<std::string> names;
  int status=-1;for(int i=0;i<p->numParams();++i)if(p->param(i).id=="load_status")status=i;
  std::cout<<"native_program_count="<<e->numPrograms<<std::endl;
  for(int i=0;i<e->numPrograms;++i){char name[256]{};d(effGetProgramNameIndexed,i,0,name);std::string text=name;if(!names.insert(text).second||text.size()>24)++failed;try{nlohmann::json(text).dump();}catch(...){++failed;}
   d(effSetProgram,0,i);for(int n=0;n<30000&&p->busy();++n){e->processReplacing(e,nullptr,out,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   if(p->busy()){++failed;std::cout<<"FAIL timeout "<<text<<std::endl;continue;}
   if(p->getInt(status)==2){++rejected;std::cout<<"REJECTED "<<text<<" : "<<p->displayText(status)<<std::endl;continue;}
   VstMidiEvent note{};note.type=kVstMidiType;note.byteSize=sizeof(note);note.midiData[0]=char(0x90);note.midiData[1]=60;note.midiData[2]=90;VstEvents events{};events.numEvents=1;events.events[0]=reinterpret_cast<VstEvent*>(&note);d(effProcessEvents,0,0,&events);
   double energy=0;bool finite=true;for(int n=0;n<80;++n){e->processReplacing(e,nullptr,out,128);for(int j=0;j<128;++j){finite&=std::isfinite(l[j])&&std::isfinite(r[j]);energy+=l[j]*l[j]+r[j]*r[j];}}
   if(!finite)++failed;else ++loaded;std::cout<<(finite?"LOADED ":"FAIL audio ")<<i<<" "<<text<<" energy="<<energy<<std::endl;
  }
  std::cout<<"loaded="<<loaded<<" rejected="<<rejected<<" failed="<<failed<<std::endl;d(effMainsChanged,0,0);d(effClose);return failed||!loaded?1:0;
 }
 int failures=0;auto check=[&](bool ok,const std::string& name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;if(!ok)++failures;};
 if(argc>1&&std::string(argv[1])=="--factory-start"){
  // 0.3.39: with the factory bank installed a new instance lists that bank alone and starts
  // on its first sound; a project that names another bank opens that bank.
  auto open=[&]{AEffect* e=VSTPluginMain(displayHost);e->dispatcher(e,effOpen,0,0,nullptr,0);e->dispatcher(e,effSetSampleRate,0,0,nullptr,44100);e->dispatcher(e,effSetBlockSize,0,512,nullptr,0);e->dispatcher(e,effMainsChanged,0,1,nullptr,0);return e;};
  float l[512]{},r[512]{};float* out[]={l,r};
  auto wait=[&](AEffect* e){auto* p=static_cast<Vitaleon*>(e->object);for(int n=0;n<10000&&p->busy();++n){e->processReplacing(e,nullptr,out,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
  auto text=[&](AEffect* e,const std::string& name){auto* p=static_cast<Vitaleon*>(e->object);for(int i=0;i<p->numParams();++i)if(p->param(i).id==name)return p->displayText(i);return std::string("?");};
  auto index=[&](AEffect* e,const std::string& name){auto* p=static_cast<Vitaleon*>(e->object);for(int i=0;i<p->numParams();++i)if(p->param(i).id==name)return i;return -1;};
  auto program=[&](AEffect* e,int n){char name[256]{};if(n<0)e->dispatcher(e,effGetProgramName,0,0,name,0);else e->dispatcher(e,effGetProgramNameIndexed,n,0,name,0);return std::string(name);};
  AEffect* e=open();auto* p=static_cast<Vitaleon*>(e->object);e->processReplacing(e,nullptr,out,128);wait(e);
  const int cutoff=p->controlIndex("filter_1_cutoff");
  check(text(e,"active_bank_0")=="Vitaleon Factory"&&e->numPrograms==2&&program(e,0)=="BA Alpha"&&program(e,1)=="LD Beta","a new instance lists the factory bank alone in PRESET, though another bank comes first by name");
  check(p->get(cutoff)==33&&program(e,-1)=="BA Alpha"&&e->dispatcher(e,effGetProgram,0,0,nullptr,0)==0&&text(e,"library_loaded_0")=="BA Alpha","a new instance starts on the first sound of the factory bank");
  e->dispatcher(e,effSetProgram,0,1,nullptr,0);wait(e);
  check(p->get(cutoff)==44&&text(e,"library_loaded_0")=="LD Beta","the other sounds of the factory bank load from PRESET");
  e->setParameter(e,index(e,"bank_step"),2.0f/3);wait(e);
  // 0.4.0: a step to another bank loads that bank's first sound (up to 0.3.59 the sound stayed).
  check(text(e,"active_bank_0")=="Aaa Bank"&&e->numPrograms==1&&program(e,0)=="Other One"&&p->get(cutoff)==55&&text(e,"library_loaded_0")=="Other One"&&e->dispatcher(e,effGetProgram,0,0,nullptr,0)==0,
      "BANK > goes from the factory bank to the next bank and loads its first sound; PRESET lists the bank alone");
  e->dispatcher(e,effSetProgram,0,0,nullptr,0);wait(e);
  void* chunk=nullptr;auto bytes=e->dispatcher(e,effGetChunk,0,0,&chunk,0);std::vector<uint8_t> saved((uint8_t*)chunk,(uint8_t*)chunk+bytes);
  AEffect* again=open();auto* q=static_cast<Vitaleon*>(again->object);again->dispatcher(again,effSetChunk,0,saved.size(),saved.data(),0);again->processReplacing(again,nullptr,out,128);wait(again);
  check(p->get(cutoff)==55&&q->get(cutoff)==55&&text(again,"active_bank_0")=="Aaa Bank"&&program(again,0)=="Other One","a project saved on another bank opens that bank, not the factory bank");
  e->setParameter(e,index(e,"bank_step"),1.0f/3);wait(e);
  check(text(e,"active_bank_0")=="Vitaleon Factory"&&p->get(cutoff)==33&&text(e,"library_loaded_0")=="BA Alpha"&&e->numPrograms==2,"< BANK goes back to the factory bank and loads its first sound");
  // 0.3.52 / 0.3.53: the wavetables of the OSC overview and their pages. The plugin's own
  // folder holds three factory tables (01, 02, 05) and "Own Table"; the user's folder
  // (Vitaleon/Wavetables beside the plugin) holds "User One" (a ramp) and "User Zed" (a square).
  {namespace fs=std::filesystem;
   auto set=[&](const std::string& name,int value){const int at=index(e,name);e->setParameter(e,at,p->param(at).toNormalised(float(value)));wait(e);};
   auto get=[&](const std::string& name){return p->getInt(index(e,name));};
   auto message=[&]{std::string all;for(int i=0;i<4;++i)all+=text(e,"display_load_text_"+std::to_string(i));return all;};
   auto rowName=[&](int row){return text(e,"display_table_row_"+std::to_string(row));};
   // The picture of a row as the skin gets it: 24 segments of a line, so 25 points in 11 levels;
   // empty when the row has none. A segment that does not start where the one before ended
   // gives a single -1. 0.3.55: the 768 values are those of the three wave pictures.
   std::vector<int> waveValue;for(int o=0;o<3;++o)for(int i=0;i<256;++i)waveValue.push_back(index(e,i<96?"display_wave_"+std::to_string(o)+"_"+std::to_string(i):i<192?"display_wave_bank_"+std::to_string(o)+"_"+std::to_string(i-96):"display_wave_fine_"+std::to_string(o)+"_"+std::to_string(i-192)));
   auto picture=[&](int row){std::vector<int> levels;for(int part=0;part<24;++part){const int value=int(std::lround(p->getNormalised(waveValue[size_t(row*24+part)])*121.0f));
       if(!value)return std::vector<int>();const int from=(value-1)/11,to=(value-1)%11;
       if(!part)levels.push_back(from);else if(levels.back()!=from)return std::vector<int>{-1};
       levels.push_back(to);}return levels;};
   const fs::path mine(Vitaleon::userFolder("Wavetables")),stick(argc>2?argv[2]:"");
   check(index(e,"table_step")==3284&&index(e,"ui_tables")==3285&&index(e,"table_target")==3286&&index(e,"table_pick")==3287&&index(e,"display_table_name_0")==3288
         &&index(e,"table_page")==3291&&index(e,"table_page_step")==3292&&index(e,"table_row_mark")==3293&&index(e,"table_empty")==3294&&index(e,"display_table_page")==3295
         &&index(e,"table_row_tap_0")==3296&&index(e,"table_row_on_0")==3328&&index(e,"display_table_row_0")==3360&&p->numParams()==3392&&index(e,"display_table_col_0_0")<0,
       "the wavetable commands, names and rows are appended after every older parameter, and the pictures have no parameters of their own");
   check(fs::exists(mine/"User One.wav")&&mine.parent_path().filename()=="Vitaleon"&&mine.string().find("factory-start")==std::string::npos,"the user's wavetables are in a Vitaleon folder beside the plugin's own folder, not in it");
   const int page=index(e,"ui_page"),tables=int(p->param(page).max);
   // The arrows: six files of the selector (two of them in the folder Pack), then the two of the user's folder.
   const std::string before=text(e,"display_table_name_0");
   std::vector<std::string> round;for(int n=0;n<9;++n){set("table_step",2);round.push_back(text(e,"display_table_name_0"));}
   if(round[0]==before||round[8]!=round[0]){std::cout<<"TABLE STEP DETAIL before="<<before<<" round=";for(const auto& name:round)std::cout<<name<<"|";std::cout<<" message="<<message()<<std::endl;}
   const std::set<std::string> distinct(round.begin(),round.begin()+8);
   check(round[0]!=before&&distinct.size()==8&&round[8]==round[0]&&round[4]=="Delta"&&round[5]=="Gamma"&&round[6]=="User One"&&round[7]=="User Zed"&&get("table_step")==0,
       "the right arrow goes through every table in turn, the plugin's and then the user's, and round again");
   set("table_step",1);const bool back=text(e,"display_table_name_0")==round[7]&&get("table_1")==0&&message().find("Table loaded: User Zed > OSC 1")==0;set("table_step",1);
   check(back&&text(e,"display_table_name_0")==round[6],"the left arrow goes the other way, from the first table round to the user's last, and says what it loaded");
   set("table_step",1);
   check(text(e,"display_table_name_0")==round[5]&&get("table_1")==6,"a step from a user table back into the plugin's tables uses the selector again");
   const std::string second=text(e,"display_table_name_1");set("table_step",4);
   check(text(e,"display_table_name_1")!=second&&text(e,"display_table_name_0")==round[5],"each oscillator has its own arrows");
   // The page of wavetables: the factory page.
   set("ui_tables",3);
   check(get("ui_page")==tables&&get("ui_tables")==0&&get("table_target")==3&&get("table_pick")==0&&get("table_page")==0&&text(e,"display_table_page")=="FACTORY"&&get("table_row_on_0")==0,
       "a tap on the picture of oscillator 3 opens the wavetable page for it on the factory page, with nothing marked and no user row");
   set("table_pick",5);
   check(text(e,"display_table_name_2").find("05 Epsilon")!=std::string::npos&&get("table_pick")==5&&message().find("> OSC 3")!=std::string::npos,"a tap on a factory row loads that table into the oscillator of the page and marks the row");
   set("table_pick",2);const bool changed=text(e,"display_table_name_2").find("02 Beta")!=std::string::npos&&get("table_pick")==2;
   set("table_pick",7);
   check(changed&&get("table_pick")==2&&text(e,"display_table_name_2").find("02 Beta")!=std::string::npos&&message().find("Factory table 7 is not installed")==0,
       "a factory table that is not installed changes nothing and the mark goes back to the table in use");
   set("table_target",1);const int first=get("table_pick");set("table_target",3);
   check(get("table_pick")==2&&first!=2,"the mark follows the oscillator that the page is switched to");
   // 0.3.59: after the factory page come the pages of the folders of tables that ship with
   // the plugin, each under its folder's name; the user's pages come last.
   set("table_page_step",2);
   check(get("table_page")==1&&text(e,"display_table_page")=="PACK 1 OF 1"&&rowName(0)=="Delta"&&rowName(1)=="Gamma"&&get("table_row_on_1")==1&&get("table_row_on_2")==0&&rowName(2).empty()
         &&get("table_empty")==0&&picture(0).size()==25&&picture(2).empty(),
       "a folder of tables that ships with the plugin has pages of its own, named after the folder, with the plugin's pictures");
   set("table_row_tap_1",1);
   check(text(e,"display_table_name_2")=="Gamma"&&get("table_row_mark")==2&&get("table_3")==6&&get("table_pick")==0&&message().find("> OSC 3")!=std::string::npos,
       "a tap on a row of such a page loads the table through the selector and marks the row");
   // The last pages: the tables that belong to no folder, with pictures the plugin made.
   set("table_page_step",2);
   const auto ramp=picture(1),square=picture(2);
   bool rising=ramp.size()==25;for(size_t i=1;i<ramp.size();++i)rising&=ramp[i]>=ramp[i-1];
   bool halves=square.size()==25;for(size_t i=0;i<square.size();++i)halves&=square[i]==(i<=12?10:0);
   if(!rising||!halves){std::cout<<"TABLE PICTURE DETAIL ramp=";for(int v:ramp)std::cout<<v<<",";std::cout<<" square=";for(int v:square)std::cout<<v<<",";std::cout<<std::endl;}
   check(get("table_page")==2&&get("table_page_step")==0&&text(e,"display_table_page")=="USER 1 OF 1"&&rowName(0)=="Own Table"&&rowName(1)=="User One"&&rowName(2)=="User Zed"
         &&get("table_row_on_0")==1&&get("table_row_on_2")==1&&get("table_row_on_3")==0&&rowName(3).empty()&&get("table_empty")==0,
       "the user's page lists the tables of no folder, 32 to a page: the plugin's loose ones first, then the user's by name");
   check(rising&&ramp.front()<=1&&ramp.back()>=9&&halves&&picture(0).size()==25&&picture(3).empty(),
       "the plugin draws the picture of each table from its file as one unbroken line: a ramp rises, a square stands at the top and then at the bottom, a row without a table has none");
   set("table_row_tap_1",1);
   check(text(e,"display_table_name_2")=="User One"&&get("table_row_mark")==2&&get("table_row_tap_1")==0&&get("table_pick")==0&&get("table_3")==0&&message().find("Table loaded: User One > OSC 3")==0,
       "a tap on a user row loads that file into the oscillator of the page and marks the row");
   set("table_row_tap_0",1);
   check(text(e,"display_table_name_2")=="Own Table"&&get("table_row_mark")==1&&get("table_3")==4,"a row that holds a table of the plugin's own folder loads it through the selector");
   set("table_row_tap_5",1);
   check(text(e,"display_table_name_2")=="Own Table"&&get("table_row_mark")==1,"a tap on a row without a table does nothing");
   set("table_target",2);const int other=get("table_row_mark");set("table_target",3);   // oscillator 2 plays a factory table, which is on no user page
   check(get("table_row_mark")==1&&other!=1,"the marked row follows the oscillator of the page");
   set("table_page_step",2);const bool wrapped=get("table_page")==0&&text(e,"display_table_page")=="FACTORY"&&get("table_row_on_0")==0&&get("table_row_on_1")==0;
   set("table_page_step",1);
   check(wrapped&&get("table_page")==2&&rowName(1)=="User One","the pages go round: after the last user page comes the factory page again");
   // LIBRARY: a table loaded from another drive is copied into the user's folder.
   set("import_target",1);
   p->testLibraryLoad((stick/"Stick Table.wav").string());wait(e);
   const bool kept=fs::exists(mine/"Stick Table.wav")&&fs::file_size(mine/"Stick Table.wav")==fs::file_size(stick/"Stick Table.wav")&&!fs::exists(mine/"Stick Table.wav.part");
   if(!kept||message().find("Kept in User Wavetables")==std::string::npos)std::cout<<"TABLE KEEP DETAIL message="<<message()<<" exists="<<fs::exists(mine/"Stick Table.wav")<<std::endl;
   check(kept&&text(e,"display_table_name_1")=="Stick Table"&&message().find("Table loaded: Stick Table > OSC 2")==0&&message().find("Kept in User Wavetables")!=std::string::npos,
       "a wavetable loaded in LIBRARY from another drive is loaded and copied into the user's folder, and the message says so");
   set("ui_tables",2);
   check(get("table_page")==2&&rowName(1)=="Stick Table"&&rowName(2)=="User One"&&get("table_row_mark")==2&&picture(1).size()==25,
       "the copied table is on the user page the next time it opens, with its picture, and marked as the table in use");
   p->testLibraryLoad((stick/"Stick Table.wav").string());wait(e);
   const bool again1=message().find("Already in User Wavetables")!=std::string::npos&&!fs::exists(mine/"Stick Table 2.wav");
   p->testLibraryLoad((stick/"other"/"Stick Table.wav").string());wait(e);
   check(again1&&fs::exists(mine/"Stick Table 2.wav")&&fs::file_size(mine/"Stick Table 2.wav")==fs::file_size(stick/"other"/"Stick Table.wav")&&fs::file_size(mine/"Stick Table.wav")==fs::file_size(stick/"Stick Table.wav"),
       "the same file is not copied twice, and another file of the same name is kept beside the first");
   p->testLibraryLoad((fs::path(Vitaleon::userFolder("Wavetables")).parent_path().parent_path()/"factory-start"/"Content"/"Wavetables"/"Own Table.wav").string());wait(e);
   check(!fs::exists(mine/"Own Table.wav")&&message().find("Table loaded: Own Table > OSC 2")==0&&message().find("User Wavetables")==std::string::npos,"a table of the plugin's own folder is loaded without a copy");
   p->testLibraryLoad((stick/"Broken.wav").string());wait(e);
   check(!fs::exists(mine/"Broken.wav"),"a file that is not a wavetable is not copied");
   // A .vitaltable in the user's folder: the plugin's own table file format.
   {auto made=p->testAssets()["wavetables"][0];made["name"]="User Made";std::ofstream file(mine/"User Made.vitaltable",std::ios::binary);file<<made.dump();}
   set("ui_tables",1);
   int row=-1;for(int r=0;r<8;++r)if(rowName(r)=="User Made")row=r;
   bool flat=true;if(row>=0)for(int v:picture(row))flat&=v==5;
   check(row>=0&&picture(row).size()==25&&!flat,"a .vitaltable put into the folder while the plugin runs is listed when the page opens, with a picture of its first wave");
   if(row>=0)set("table_row_tap_"+std::to_string(row),1);
   check(row>=0&&text(e,"display_table_name_0")=="User Made"&&get("table_row_mark")==row+1,"and loads from its row");
   // 0.3.55: the pictures borrowed the values of the three wave pictures. Back on a page with
   // wave pictures those hold waves again (levels of 108), with Live Graphics on and off.
   {auto waves=[&]{for(int n=0;n<60;++n){e->processReplacing(e,nullptr,out,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}wait(e);
        int wrong=0;for(int at:waveValue){const float v=p->getNormalised(at)*107.0f;if(std::abs(v-std::round(v))>1e-3f)++wrong;}return wrong;};
    int borrowed=0;for(int at:waveValue){const float v=p->getNormalised(at)*107.0f;if(std::abs(v-std::round(v))>1e-3f)++borrowed;}
    set("ui_page",40);const int afterLive=waves();
    set("visuals",0);set("ui_tables",1);int again=0;for(int at:waveValue){const float v=p->getNormalised(at)*107.0f;if(std::abs(v-std::round(v))>1e-3f)++again;}
    set("ui_page",40);const int afterOff=waves();set("visuals",1);
    if(!borrowed||afterLive||!again||afterOff)std::cout<<"TABLE WAVE DETAIL borrowed="<<borrowed<<" afterLive="<<afterLive<<" again="<<again<<" afterOff="<<afterOff<<std::endl;
    check(borrowed>0&&afterLive==0&&again>0&&afterOff==0,"the wave pictures get their own values back when the wavetable page is left, with Live Graphics on and off");
    set("ui_tables",1);}
   set("table_page_step",1);set("table_page_step",1);set("table_pick",1);set("table_step",2);
   check(text(e,"display_table_name_0").find("02 Beta")!=std::string::npos&&get("table_pick")==2&&get("table_row_mark")==0,"the arrows and the pages work on the same list: one step after table 01 is table 02, and the factory page marks it");
   e->dispatcher(e,effSetProgram,0,1,nullptr,0);wait(e);
   check(get("table_pick")==0&&get("table_row_mark")==0&&text(e,"display_table_name_0").find("02 Beta")==std::string::npos,"a sound that is loaded brings its own tables: nothing is marked and the names are those of the sound");
   // LIBRARY lists the user's folder among the drives.
   set("ui_page",3);set("storage_command_0",1);
   bool listed=false;for(int r=0;r<6;++r)if(text(e,"storage_row_"+std::to_string(r*2+1))=="User wavetables"&&text(e,"storage_row_"+std::to_string(r*2))=="FOLDER")listed=true;
   if(!listed){std::cout<<"TABLE DRIVES DETAIL";for(int r=0;r<6;++r)std::cout<<" | "<<text(e,"storage_row_"+std::to_string(r*2))<<":"<<text(e,"storage_row_"+std::to_string(r*2+1));std::cout<<" status="<<text(e,"storage_status_0")<<text(e,"storage_status_1")<<" path="<<text(e,"storage_path_0")<<std::endl;}
   check(listed,"LIBRARY lists the folder of the user's wavetables among the drives");
   set("ui_page",0);}
  again->dispatcher(again,effMainsChanged,0,0,nullptr,0);again->dispatcher(again,effClose,0,0,nullptr,0);
  e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);return failures?1:0;
  again->dispatcher(again,effMainsChanged,0,0,nullptr,0);again->dispatcher(again,effClose,0,0,nullptr,0);
  e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);return failures?1:0;
 }
 if(argc>1&&std::string(argv[1])=="--no-content"){
  // A plugin without any installed sound (a build from source): one "Init" program.
  AEffect* e=VSTPluginMain(displayHost);auto* p=static_cast<Vitaleon*>(e->object);
  auto d=[&](int op,int ix=0,VstIntPtr val=0,void* ptr=nullptr,float opt=0){return e->dispatcher(e,op,ix,val,ptr,opt);};
  d(effOpen);d(effSetSampleRate,0,0,nullptr,44100);d(effSetBlockSize,0,512);d(effMainsChanged,0,1);
  float l[512]{},r[512]{};float* out[]={l,r};char name[256]{};d(effGetProgramNameIndexed,0,0,name);
  auto wait=[&]{for(int n=0;n<10000&&p->busy();++n){e->processReplacing(e,nullptr,out,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
  auto energy=[&]{VstMidiEvent note{};note.type=kVstMidiType;note.byteSize=sizeof(note);note.midiData[0]=char(0x90);note.midiData[1]=60;note.midiData[2]=100;VstEvents events{};events.numEvents=1;events.events[0]=reinterpret_cast<VstEvent*>(&note);d(effProcessEvents,0,0,&events);
   double sum=0;for(int n=0;n<80;++n){e->processReplacing(e,nullptr,out,128);for(int j=0;j<128;++j)sum+=l[j]*l[j]+r[j]*r[j];}return sum;};
  wait();check(e->numPrograms==1&&std::string(name)=="Init"&&d(effGetProgram)==0,"plugin without sounds offers exactly one Init program");
  // The Init state of every parameter, for tools/generate_surface.py (tools/init-values.json).
  if(argc>2){nlohmann::json values=nlohmann::json::array();for(int i=0;i<p->numParams();++i)values.push_back({{"index",i},{"name",p->param(i).name},{"value",p->getNormalised(i)},{"display",p->displayText(i)}});std::ofstream file(argv[2]);file<<values.dump(1);}
  check(energy()>1e-6,"plugin without sounds plays its default sound");
  int cutoff=p->controlIndex("filter_1_cutoff");float before=p->get(cutoff);p->set(cutoff,before==20?30.0f:20.0f);d(effSetProgram,0,0);wait();
  int loaded=-1;for(int i=0;i<p->numParams();++i)if(p->param(i).id=="library_loaded_0")loaded=i;
  check(p->get(cutoff)==before&&p->displayText(loaded)=="Init"&&energy()>1e-6,"selecting Init resets the controls and still plays");
  d(effMainsChanged,0,0);d(effClose);return failures?1:0;
 }
 {
  vital::Wavetable oldPath(257),prepared(257),target(257);
  WavetableCreator a(&oldPath);a.initPredefinedWaves();auto json=a.stateToJson();a.jsonToState(json);a.render();
  WavetableCreator b(&prepared);b.jsonToState(json);
  int version=target.getVersion();target.adoptPrepared(prepared);
  auto* x=oldPath.getAllData();auto* y=target.getAllData();int frames=x->num_frames;
  bool identical=frames==y->num_frames&&x->frequency_ratio==y->frequency_ratio&&x->sample_rate==y->sample_rate;
  identical&=std::memcmp(x->wave_data.get(),y->wave_data.get(),frames*sizeof(float)*vital::Wavetable::kWaveformSize)==0;
  for(int f=0;f<frames;++f){size_t bytes=sizeof(vital::poly_float)*vital::Wavetable::kPolyFrequencySize;
   identical&=std::memcmp(x->frequency_amplitudes[f],y->frequency_amplitudes[f],bytes)==0;
   identical&=std::memcmp(x->normalized_frequencies[f],y->normalized_frequencies[f],bytes)==0;
   identical&=std::memcmp(x->phases[f],y->phases[f],bytes)==0;
  }
  check(identical,"prepared transfer preserves every waveform and spectral data byte");
  check(target.getVersion()!=version&&target.getName()==oldPath.getName()&&target.isShepardTable()==oldPath.isShepardTable(),"prepared transfer invalidates oscillator cache and preserves metadata");
 }
 for(bool control:{false,true}){
  LineGenerator curve;curve.setNumPoints(4);curve.setPoint(0,{0,1});curve.setPoint(1,{.25f,.3f});curve.setPoint(2,{.6f,.8f});curve.setPoint(3,{1,0});curve.render();
  vital::SynthLfo lfo(&curve);lfo.setSampleRate(1000);lfo.setControlRate(control);
  std::array<vital::Output,vital::SynthLfo::kNumInputs> in;for(int i=0;i<int(in.size());++i)lfo.plug(&in[i],i);
  in[vital::SynthLfo::kSyncType].buffer[0]=vital::SynthLfo::kPointCycle;in[vital::SynthLfo::kPhase].buffer[0]=.83f;
  auto trigger=[&](vital::SynthLfo& target){in[vital::SynthLfo::kNoteTrigger].trigger_value=vital::poly_float(vital::kVoiceOn,vital::kVoiceOn,0,0);target.process(1);in[vital::SynthLfo::kNoteTrigger].clearTrigger();return vital::utils::decodePhaseAndVoice(target.output(vital::SynthLfo::kOscPhase)->buffer[0]).first[0];};
  bool ok=std::abs(trigger(lfo))<1e-5;
  std::unique_ptr<vital::SynthLfo> clone(static_cast<vital::SynthLfo*>(lfo.clone()));
  ok&=std::abs(trigger(*clone)-.25f)<1e-5&&std::abs(trigger(lfo)-.6f)<1e-5;
  in[vital::SynthLfo::kFrequency].buffer[0]=8;for(int i=0;i<8;++i)lfo.process(32);
  float end=vital::utils::decodePhaseAndVoice(lfo.output(vital::SynthLfo::kOscPhase)->buffer[0]).first[0];ok&=std::abs(end-1)<1e-4;
  check(ok,control?"Point Cycle control-rate segments, shared cursor and endpoint":"Point Cycle audio-rate segments, shared cursor and endpoint");
  lfo.resetPointCycle();curve.setPoint(2,{.25f,.8f});curve.render();in[vital::SynthLfo::kFrequency].buffer[0]=0;
  ok=std::abs(trigger(lfo))<1e-5&&std::abs(trigger(lfo)-.25f)<1e-5&&std::abs(trigger(lfo))<1e-5;
  check(ok,"Point Cycle skips duplicate-time segments");
 }
 // The engine checks start from the Init state; the start sound is checked through the VST entry point below.
 auto synth=std::make_unique<Vitaleon>();synth->testNoStartSound();synth->prepare(44100,512);
 displayEffect.object=synth.get();synth->effect_=&displayEffect;synth->host_=displayHost;
 auto id=[&](const std::string& name){for(int i=0;i<synth->numParams();++i)if(synth->param(i).id==name)return i;throw std::runtime_error("Missing parameter "+name);};
 float left[512]{},right[512]{};float* outputs[]={left,right};mpc::ProcessContext ctx;ctx.outputs=outputs;ctx.numOutputs=2;ctx.numFrames=128;
 bool finite=true;auto render=[&]{mpc::ScopedDenormalsOff denormals;audit=true;synth->process(ctx);audit=false;for(int i=0;i<ctx.numFrames;++i)finite&=std::isfinite(left[i])&&std::isfinite(right[i]);};
 auto settle=[&]{ctx.numMidiEvents=0;for(int i=0;i<10000&&synth->busy();++i){render();std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!synth->busy(),"asset service settled");};
 std::array<mpc::MidiEvent,8> notes;for(int i=0;i<8;++i)notes[i]=mpc::MidiEvent::noteOn(0,48+i,90);ctx.midi=notes.data();ctx.numMidiEvents=8;render();ctx.numMidiEvents=0;
 float peak=0;for(int b=0;b<100;++b){render();for(float x:left)peak=std::max(peak,std::abs(x));}check(peak>1e-5f,"full graph produces sound");
 // MIDI inside a block: notes are placed on their sample and cut the block; controllers, bend and pressure do not.
 {auto calls=[&](std::vector<mpc::MidiEvent> events){ctx.midi=events.data();ctx.numMidiEvents=int(events.size());const long before=synth->testEngineCalls();render();ctx.numMidiEvents=0;ctx.midi=notes.data();return int(synth->testEngineCalls()-before);};
  auto bend=[](int value,int offset){mpc::MidiEvent e;e.sampleOffset=offset;e.size=3;e.data[0]=0xe0;e.data[1]=0;e.data[2]=uint8_t(value);return e;};
  auto pressure=[](int value,int offset){mpc::MidiEvent e;e.sampleOffset=offset;e.size=2;e.data[0]=0xd0;e.data[1]=uint8_t(value);return e;};
  check(calls({})==1,"a block without MIDI is one engine call");
  check(calls({mpc::MidiEvent::cc(0,1,64,40),bend(70,64),pressure(90,90)})==1,"controllers, bend and pressure inside a block do not cut it");
  check(calls({mpc::MidiEvent::noteOn(0,72,90,64)})==2,"a note inside a block is placed on its sample");
  check(calls({mpc::MidiEvent::noteOff(0,72,20),mpc::MidiEvent::cc(0,1,0,30),bend(64,60),mpc::MidiEvent::noteOn(0,74,90,100),mpc::MidiEvent::noteOff(0,74,100),pressure(0,110)})==3,"only the note positions cut a block with mixed events");}
 synth->set(id("edit_env"),3);synth->set(id("edit_env_attack"),.7f);check(std::abs(synth->get(synth->controlIndex("env_3_attack"))-.7f)<1e-5,"envelope proxy edits selected envelope");
 synth->set(synth->controlIndex("env_3_sustain"),.123f);check(std::abs(synth->get(id("edit_env_sustain"))-.123f)<1e-5,"canonical edits refresh selected proxy");
 float previousAttack=synth->getNormalised(synth->controlIndex("env_1_attack"));
 synth->setNormalised(id("home_3"),.5f);
 check(std::abs(synth->getNormalised(synth->controlIndex("env_1_attack"))-.5f)<1e-5&&std::abs(synth->get(synth->controlIndex("env_3_attack"))-.7f)<1e-5,"overview attack edits amplitude envelope, not editor selection");
 synth->setNormalised(id("home_3"),previousAttack);
 synth->setNormalised(id("home_5"),1);check(synth->getInt(synth->controlIndex("osc_1_unison_voices"))==16,"overview unison reaches full-engine range");synth->setNormalised(id("home_5"),0);
 auto choice=[&](int p,const std::string& value){const auto& choices=synth->param(p).choices;for(size_t i=0;i<choices.size();++i)if(choices[i]==value)return float(i);throw std::runtime_error("Choice missing: "+value);};
 // Deterministically place an audio-block refresh between the host's atomic
 // parameter store and its parameterChanged callback. Rereading a proxy here
 // loses the gesture; the callback's supplied plain value is authoritative.
 auto interleavedEdit=[&](int proxy,float normalized){
  synth->publishNormalised(proxy,normalized);render();
  synth->parameterChanged(proxy,synth->param(proxy).toPlain(normalized));
 };
 interleavedEdit(id("home_1"),.82f);
 check(std::abs(synth->getNormalised(synth->controlIndex("filter_1_cutoff"))-.82f)<1e-5,"HOME cutoff survives audio refresh during host gesture");
 interleavedEdit(id("edit_env_attack"),.61f);
 check(std::abs(synth->getNormalised(synth->controlIndex("env_3_attack"))-.61f)<1e-5,"envelope gesture survives audio refresh");
 synth->set(id("advanced_parameter"),0);interleavedEdit(id("advanced_value"),.37f);
 check(std::abs(synth->getNormalised(synth->controlIndex(synth->param(id("advanced_parameter")).choices[0]))-.37f)<1e-5,"advanced gesture survives audio refresh");
 synth->set(id("route_1_source"),choice(id("route_1_source"),"lfo_1"));synth->set(id("route_1_destination"),choice(id("route_1_destination"),"osc_1_wave_frame"));
 synth->set(synth->controlIndex("modulation_1_amount"),.8f);settle();
 float min=1,max=0;for(int b=0;b<700;++b){render();float p=synth->get(id("display_position_0"));min=std::min(min,p);max=std::max(max,p);}check(max-min>.1f,"wavetable display follows actual LFO modulation");std::cout<<"frame_display_min="<<min<<" max="<<max<<std::endl;
 if(argc>2){nlohmann::json values=nlohmann::json::array();for(int i=0;i<synth->numParams();++i)values.push_back({{"id",synth->param(i).id},{"name",synth->param(i).name},{"index",i},{"value",synth->getNormalised(i)},{"display",synth->displayText(i)}});std::ofstream file(argv[2]);file<<values.dump(2);}
 for(const char* fx:{"chorus_on","compressor_on","delay_on","distortion_on","eq_on","filter_fx_on","flanger_on","phaser_on","reverb_on"}){synth->set(synth->controlIndex(fx),1);for(int b=0;b<12;++b)render();synth->set(synth->controlIndex(fx),0);}
 check(finite,"all nine effects render finite audio");check(allocations==0,"no C++ allocations in callback (full graph, modulation, effects)");std::cout<<"callback_allocations="<<allocations<<std::endl;
 synth->set(id("effect_slot"),1);synth->set(id("effect_type"),8);int order[9];vital::utils::decodeFloatToOrder(order,synth->get(synth->controlIndex("effect_chain_order")),9);check(order[0]==8,"FX reorder controls update engine order");
 auto saved=synth->internalSaveState();float sustain=synth->get(synth->controlIndex("env_3_sustain"));synth->set(synth->controlIndex("env_3_sustain"),.9f);check(synth->internalLoadState(saved.data(),saved.size()),"state parser accepts saved chunk");settle();check(std::abs(synth->get(synth->controlIndex("env_3_sustain"))-sustain)<1e-6,"canonical state round trip");
 auto savedAssets=synth->testAssets();check(!synth->internalLoadState(saved.data(),saved.size()-10),"truncated chunk rejected");check(synth->get(synth->controlIndex("env_3_sustain"))==sustain,"invalid chunk leaves parameters unchanged");
 check(savedAssets.count("sample")&&savedAssets["sample"].count("samples"),"default sample embedded in project state");
 for(int turn=0;turn<6;++turn){synth->testInit();if(turn%2)synth->set(synth->controlIndex("osc_2_on"),1);settle();check(synth->internalLoadState(saved.data(),saved.size()),"project restores across sound changes");settle();check(synth->testAssets()==savedAssets,"embedded tables, sample and curves restored exactly");}
 nlohmann::json invalid={{"synth_version","1.0.6"},{"settings",savedAssets}};invalid["settings"]["env_3_sustain"]=.99f;invalid["settings"]["lfos"][0]["num_points"]=101;
 bool rejected=false;try{synth->testImport(invalid);}catch(const std::exception&){rejected=true;}
 check(rejected&&synth->get(synth->controlIndex("env_3_sustain"))==sustain,"invalid preset rejected before changing sound parameters");
 auto beforeLateFailure=synth->testAssets();auto lateFailure=invalid;
 lateFailure["settings"]=beforeLateFailure;lateFailure["settings"]["sample"]["samples"]="broken";
 lateFailure["settings"]["wavetables"][0]["name"]="Prepared but must not commit";
 rejected=false;try{synth->testImport(lateFailure);}catch(...){rejected=true;}
 check(rejected&&synth->testAssets()==beforeLateFailure&&synth->get(synth->controlIndex("env_3_sustain"))==sustain,"sample failure after table preparation preserves previous sound and all assets");
 nlohmann::json ramps={{"synth_version","1.6.4"},{"settings",{{"filter_1_cutoff",63},{"modulation_1_ramp_up",-10},{"modulation_64_ramp_down",-10}}}};
 synth->testImport(ramps);check(synth->get(synth->controlIndex("filter_1_cutoff"))==63,"1.6 minimum ramp fields import with documented compatibility bypass");
 for(const auto& field:{"modulation_1_ramp_up","modulation_64_ramp_down","modulation_65_ramp_up"}){auto bad=ramps;bad["settings"][field]=-9;bool rejected=false;try{synth->testImport(bad);}catch(...){rejected=true;}check(!rejected&&synth->get(synth->controlIndex("filter_1_cutoff"))==63&&synth->testImportNotes().size()==1,"configured or unknown ramp loads and is reported as left out");}
 auto modRamp=ramps;modRamp["settings"]["modulations"]=nlohmann::json::array({{{"source","lfo_1"},{"destination","modulation_1_ramp_up"}}});rejected=false;try{synth->testImport(modRamp);}catch(...){rejected=true;}check(!rejected&&synth->testImportNotes().size()==1&&synth->testImportNotes()[0].find("Mod target skipped")==0&&synth->getInt(id("route_1_destination"))==0,"route to an unknown target is skipped and reported, not connected elsewhere");
 {nlohmann::json modern={{"synth_version","1.5.5"},{"settings",{{"filter_1_cutoff",64},{"osc_1_spectral_morph_type",14},{"osc_2_spectral_morph_type",3},{"osc_1_spectral_morph_phase",.25},{"pitch_env_type",0}}}};
  bool threw=false;try{synth->testImport(modern);}catch(...){threw=true;}
  check(!threw&&synth->get(synth->controlIndex("filter_1_cutoff"))==64,"preset with newer spectral mode and unknown setting loads");
  check(synth->getInt(synth->controlIndex("osc_1_spectral_morph_type"))==0&&synth->getInt(synth->controlIndex("osc_2_spectral_morph_type"))==3,"unavailable spectral mode is switched off, supported modes are kept");
  check(synth->testImportNotes().size()==3,"every omitted feature is reported once");
  auto broken=modern;broken["settings"]["filter_1_model"]=99;threw=false;try{synth->testImport(broken);}catch(...){threw=true;}
  check(threw&&synth->get(synth->controlIndex("filter_1_cutoff"))==64,"out-of-range engine value is still rejected before sound mutation");
  synth->testImport(ramps);check(synth->testImportNotes().empty(),"clean import clears earlier notes");}
 // Effects mixed to exactly 0 behind a macro are not processed; anything else keeps running.
 {auto settleFx=[&]{for(int i=0;i<40;++i){render();std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
  nlohmann::json gated={{"synth_version","1.0.6"},{"settings",{{"reverb_on",1},{"reverb_dry_wet",0},{"delay_on",1},{"delay_dry_wet",0},{"chorus_on",1},{"chorus_dry_wet",0.3},{"macro_control_1",0},
   {"modulations",nlohmann::json::array({{{"source","macro_control_1"},{"destination","reverb_dry_wet"}},{{"source","lfo_1"},{"destination","delay_dry_wet"}}})}}}};
  synth->testImport(gated);settleFx();
  check(synth->getBool(synth->controlIndex("reverb_on"))&&!synth->testEngineSwitch("reverb_on"),"reverb at mix 0 behind a closed macro is skipped while its switch stays on");
  check(synth->testEngineSwitch("delay_on"),"effect whose mix is modulated by an LFO keeps running");
  check(synth->testEngineSwitch("chorus_on"),"effect with a non-zero mix keeps running");
  synth->set(synth->controlIndex("macro_control_1"),.4f);settleFx();
  check(synth->testEngineSwitch("reverb_on"),"opening the macro starts the effect again");
  synth->set(synth->controlIndex("macro_control_1"),0);settleFx();synth->set(synth->controlIndex("reverb_dry_wet"),.2f);settleFx();
  check(synth->testEngineSwitch("reverb_on"),"raising the mix itself starts the effect again");
  synth->set(synth->controlIndex("reverb_on"),0);settleFx();check(!synth->testEngineSwitch("reverb_on"),"switching the effect off still works");
  synth->testImport(ramps);settleFx();}
 nlohmann::json stack={{"synth_version","1.5.5"},{"settings",savedAssets}};stack["settings"]["osc_1_spectral_morph_phase"]=.5;
 for(int choice:{5,6,8,11,12}){stack["settings"]["osc_1_stack_style"]=choice;synth->testImport(stack);int expected=choice==5?11:choice>=6&&choice<=11?choice-1:choice;check(synth->get(synth->controlIndex("osc_1_stack_style"))==expected,"1.5 stack IDs translated without changing intervals");}
 stack["synth_version"]="1.0.6";stack["settings"].erase("osc_1_spectral_morph_phase");stack["settings"]["osc_1_stack_style"]=8;synth->testImport(stack);check(synth->get(synth->controlIndex("osc_1_stack_style"))==8,"1.0 stack IDs preserved");
 auto captureTable=[&](bool square){
  LineGenerator line;if(square)line.initSquare();else line.initSin();vital::Wavetable table(257);WavetableCreator creator(&table);creator.jsonToState(line.stateToJson());
  nlohmann::json sound={{"synth_version","1.0.6"},{"settings",savedAssets}};sound["settings"]["wavetables"][0]=creator.stateToJson();sound["settings"]["osc_1_random_phase"]=0;sound["settings"]["env_1_attack"]=0;sound["settings"]["env_1_sustain"]=1;sound["settings"]["polyphony"]=1;
  synth->testImport(sound);synth->reset();auto note=mpc::MidiEvent::noteOn(0,60,100);ctx.midi=&note;ctx.numMidiEvents=1;render();ctx.numMidiEvents=0;
  for(int i=0;i<40;++i)render();std::vector<float> capture;for(int i=0;i<8;++i){render();capture.insert(capture.end(),left,left+ctx.numFrames);}ctx.midi=notes.data();return capture;
 };
 auto sine=captureTable(false),square=captureTable(true),sineAgain=captureTable(false);double changed=0,restored=0;
 for(size_t i=0;i<sine.size();++i){changed+=std::abs(sine[i]-square[i]);restored+=std::abs(sine[i]-sineAgain[i]);}
 check(changed/sine.size()>.005&&restored/sine.size()<1e-5,"same-frame-count wavetable replacement changes audio and restores exactly");
 std::cout<<"wavetable_changed_mean="<<changed/sine.size()<<" restored_mean="<<restored/sine.size()<<std::endl;
 if(argc>1&&argv[1][0]){int accepted=0,rejected=0;for(const auto& e:std::filesystem::recursive_directory_iterator(argv[1]))if(e.path().extension()==".vital"){
   try{std::ifstream in(e.path());nlohmann::json preset;in>>preset;synth->testImport(preset);++accepted;ctx.numMidiEvents=8;render();ctx.numMidiEvents=0;for(int b=0;b<8;++b)render();std::cout<<"IMPORTED "<<e.path().string()<<std::endl;}
   catch(const std::exception& ex){++rejected;std::cout<<"REJECTED "<<e.path().string()<<" : "<<ex.what()<<std::endl;}}
   std::cout<<"presets_imported="<<accepted<<" presets_rejected="<<rejected<<std::endl;check(accepted>0,"real local presets imported");}
 check(finite,"finite audio after imported presets");
 if(argc>3){
  int selector=id("preset_select");auto chooseFile=[&](const std::string& name){for(int n=1;n<=int(synth->param(selector).max);++n){std::string label;synth->formatValue(selector,float(n),label);if(label==name){synth->set(selector,float(n));synth->set(id("library_load"),synth->getBool(id("library_load"))?0:1);settle();return true;}}return false;};
  auto tap=[&](const std::string& name){int p=id(name);synth->set(p,synth->getBool(p)?0:1);};
  synth->testInit();settle();
  check(synth->getBool(Vitaleon::FullMode)&&synth->displayText(id("library_loaded_0"))=="Init","Init sound uses the single full engine");
  synth->setNormalised(id("home_1"),.5f);synth->setNormalised(id("home_5"),1);
  check(std::abs(synth->getNormalised(synth->controlIndex("filter_1_cutoff"))-.5f)<1e-5&&synth->getInt(synth->controlIndex("osc_1_unison_voices"))==16,"overview controls always use full-engine parameters and ranges");
  tap("library_row_0");
  check(!synth->busy()&&synth->getBool(Vitaleon::FullMode),"touch browsing does not stop audio or switch engine");
  check(synth->displayText(id("library_selected_0"))=="01 Import Test","touch row selects actual indexed filename");
  check(chooseFile("03 Unsupported")&&synth->getBool(Vitaleon::FullMode),"unsupported preset preserves current engine");
  check(chooseFile("01 Import Test")&&synth->get(synth->controlIndex("filter_1_cutoff"))==60,"Content folder preset loads through worker");
  check(synth->getBool(Vitaleon::FullMode),"valid import remains on the single engine");
  check(synth->displayText(id("library_loaded_0"))=="01 Import Test","loaded label identifies actual successful import");
  check(chooseFile("02 Bank Test")&&synth->get(synth->controlIndex("filter_1_cutoff"))==77,"compressed bank preset loads through worker");
  check(chooseFile("03 Unsupported")&&synth->get(synth->controlIndex("filter_1_cutoff"))==77&&synth->getInt(id("load_status"))==2,"unsupported file preserves previous sound controls and reports error");
  check(synth->displayText(id("library_loaded_0"))=="02 Bank Test","failed load retains previous loaded-name label");
  std::string message;for(int i=0;i<4;++i){auto part=synth->displayText(id("display_load_text_"+std::to_string(i)));check(part.size()<=31,"status segment fits MPC host string limit");message+=part;}
  check(message==synth->displayText(id("load_status"))&&message.find("filter_1_model")!=std::string::npos,"MPC status segments retain full unsupported-feature reason");
  tap("library_load");settle();check(synth->getInt(id("load_status"))==2,"same failed preset can be retried without changing selection");
  tap("library_next");check(synth->getInt(id("library_page"))==1&&!synth->busy(),"touch page navigation is read-only");
  /* Release makes no write with persistent command bits. */ check(synth->getInt(id("library_page"))==1,"button release needs no second parameter write");
  tap("library_bank_next");check(synth->getInt(id("library_page"))==0&&synth->displayText(id("library_bank_text_0"))=="Imported Bank","bank navigation resets list to first page");
  tap("library_row_0");check(synth->displayText(id("library_selected_0"))=="02 Bank Test","filtered bank row maps to correct preset");
  tap("library_load");settle();check(synth->get(synth->controlIndex("filter_1_cutoff"))==77,"LOAD applies preset selected by touch row");
  synth->set(synth->controlIndex("filter_1_cutoff"),23);tap("library_load");settle();check(synth->get(synth->controlIndex("filter_1_cutoff"))==77,"LOAD reloads same preset after edits");
  auto beforeBlank=synth->getInt(selector);tap("library_row_5");check(synth->getInt(selector)==beforeBlank,"empty row cannot select or load another sound");
  if(argc>2){nlohmann::json values=nlohmann::json::array();for(int i=0;i<synth->numParams();++i)values.push_back({{"id",synth->param(i).id},{"name",synth->param(i).name},{"index",i},{"value",synth->getNormalised(i)},{"display",synth->displayText(i)}});std::ofstream file(std::string(argv[2])+"-library.json");file<<values.dump(2);}
  auto loadedState=synth->internalSaveState();synth->testInit();settle();check(synth->internalLoadState(loadedState.data(),loadedState.size()),"imported state accepted");settle();check(synth->displayText(id("library_loaded_0"))=="02 Bank Test"&&synth->get(synth->controlIndex("filter_1_cutoff"))==77,"loaded filename and sound survive project restore");
 }
 if(argc>4){
  nlohmann::json skin;std::ifstream input(argv[4]);input>>skin;
  bool browserCommands=false;
  for(const auto& tab:skin["pageData"]["tabs"])
   for(const auto& entry:tab["componentDefinition"]["componentsData"]){
    std::string name=entry["componentData"]["name"];
    browserCommands|=name=="SELECT"||name=="LOAD";
   }
  check(!browserCommands,"failed custom SELECT/LOAD browser is absent from the shipped skin");
 }
 // Verify audible filter action, rather than checking only the displayed value.
 // Both HOME and FILTER 1 use the same parameter handles.
 {
  synth->testInit();settle();
  synth->set(id("home_filter_on"),1);
  synth->set(synth->controlIndex("osc_1_wave_frame"),170);synth->set(synth->controlIndex("env_1_sustain"),1);
  auto energy=[&](float cutoff,bool enabled){
   synth->set(id("home_filter_on"),enabled?1:0);synth->setNormalised(id("home_1"),cutoff);
   synth->reset();auto note=mpc::MidiEvent::noteOn(0,60,100);ctx.midi=&note;ctx.numMidiEvents=1;render();ctx.numMidiEvents=0;
   for(int b=0;b<150;++b)render();double e=0;for(int b=0;b<60;++b){render();for(int n=0;n<ctx.numFrames;++n)e+=left[n]*left[n]+right[n]*right[n];}
   ctx.midi=notes.data();return e;
  };
  double low=energy(.08f,true),high=energy(.9f,true),bypass=energy(.08f,false);
  std::cout<<"filter_audio_program=Init low="<<low<<" high="<<high<<" bypass="<<bypass<<std::endl;
  check(high>low*4&&high>1e-8&&bypass>low*4,"Init HOME/FILTER 1 cutoff changes rendered audio; bypass is explicit");
 }
 auto legacy=std::make_unique<VitaleonLegacy>();legacy->set(Vitaleon::Cutoff,812.5f);legacy->set(Vitaleon::Unison,8);auto oldState=legacy->internalSaveState();
 check(synth->internalLoadState(oldState.data(),oldState.size()),"0.1.4 state accepted");settle();
 check(synth->getBool(Vitaleon::FullMode)&&synth->getInt(synth->controlIndex("osc_1_unison_voices"))==8&&std::abs(vital::utils::midiNoteToFrequency(synth->get(synth->controlIndex("filter_1_cutoff")))-812.5f)<.1f,"old project cutoff and unison migrate into the single full engine");
 check(synth->displayText(id("library_loaded_0"))=="Converted project","old project restore cannot retain an unrelated preset label");
 // Freeze telemetry for byte-exact serialization: settle() processes audio,
 // and its scheduling can otherwise change only the live waveform readouts.
 synth->set(id("visuals"),0);
 auto converted=synth->internalSaveState();auto convertedAssets=synth->testAssets();synth->testInit();settle();
 check(synth->internalLoadState(converted.data(),converted.size()),"converted project reload accepted");settle();
 auto convertedAgain=synth->internalSaveState();
 if(convertedAgain!=converted&&argc>2){std::ofstream a(std::string(argv[2])+".state-before",std::ios::binary),b(std::string(argv[2])+".state-after",std::ios::binary);a.write(reinterpret_cast<const char*>(converted.data()),converted.size());b.write(reinterpret_cast<const char*>(convertedAgain.data()),convertedAgain.size());}
 check(synth->testAssets()==convertedAssets&&convertedAgain==converted,"converted waveforms and state round-trip exactly");
 // 0.2.x saved many more than sixteen parameters while its old mode was active.
 synth->publishNormalised(Vitaleon::FullMode,0);auto oldExpanded=synth->internalSaveState();
 check(synth->internalLoadState(oldExpanded.data(),oldExpanded.size()),"expanded old-mode project accepted");settle();
 check(synth->getBool(Vitaleon::FullMode)&&std::abs(vital::utils::midiNoteToFrequency(synth->get(synth->controlIndex("filter_1_cutoff")))-812.5f)<.1f,"0.2.x old-mode flag triggers conversion rather than an inactive UI");
 synth->set(Vitaleon::FullMode,0);check(synth->getBool(Vitaleon::FullMode),"retired mode parameter cannot reactivate a second engine");
 synth->testInit();settle();check(synth->getBool(Vitaleon::FullMode),"Init after a converted project uses the one engine");
 // Exercise the exported entry point and real VST dispatcher used by the
 // host's PRESET dropdown, independently of the retired custom browser.
 if(argc>3){
  AEffect* effect=VSTPluginMain(displayHost);
  auto* native=static_cast<Vitaleon*>(effect->object);
  auto dispatch=[&](int opcode,int index=0,VstIntPtr value=0,void* ptr=nullptr,float opt=0){return effect->dispatcher(effect,opcode,index,value,ptr,opt);};
  dispatch(effOpen);dispatch(effSetSampleRate,0,0,nullptr,44100);dispatch(effSetBlockSize,0,512);dispatch(effMainsChanged,0,1);
  auto waitNative=[&]{for(int i=0;i<10000&&native->busy();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!native->busy(),"native program service settled");};
  int loose=-1,bank=-1,secondBank=-1,unsupported=-1;std::set<std::string> names;bool validNames=true;
  for(int p=0;p<effect->numPrograms;++p){char name[256]{};validNames&=dispatch(effGetProgramNameIndexed,p,0,name)==1;std::string text=name;validNames&=!text.empty()&&text.size()<=24&&names.insert(text).second;nlohmann::json utf8=text;try{utf8.dump();}catch(...){validNames=false;}if(text=="01 Import Test")loose=p;if(text=="02 Bank Test")bank=p;if(text=="03 Unsupported")unsupported=p;if(text=="02 Bank Test #2")secondBank=p;}
  check(effect->numPrograms==11&&loose==0&&bank>0&&secondBank>0&&unsupported>0,"VST entry point exposes exactly the eleven sounds from two banks and loose files, no built-in programs");
  check(validNames&&dispatch(effGetProgram)==0,"native program enumeration has unique valid names and keeps the first sound current");
  auto select=[&](int p){dispatch(effSetProgram,0,p);waitNative();};
  int cutoff=native->controlIndex("filter_1_cutoff");
  {waitNative();char started[256]{};dispatch(effGetProgramName,0,0,started);
   check(native->get(cutoff)==60&&dispatch(effGetProgram)==0&&std::string(started)=="01 Import Test"&&native->displayText(id("library_loaded_0"))=="01 Import Test","a new instance starts with the first installed sound loaded");
   check(native->displayText(id("library_page_text"))=="11 installed sounds","library names the number of installed sounds");}
  select(bank);char current[256]{};dispatch(effGetProgramName,0,0,current);
  check(native->get(cutoff)==77&&dispatch(effGetProgram)==bank&&std::string(current)=="02 Bank Test","native PRESET selection loads compressed bank and reports current program");
  select(secondBank);check(native->get(cutoff)==88&&native->displayText(id("library_bank_text_0"))=="Second Bank","second bank with duplicate sound name selects correct archive");
  select(bank);check(native->get(cutoff)==77&&native->displayText(id("library_bank_text_0"))=="Imported Bank","switching back restores first bank without stale sound data");
  auto bankAssets=native->testAssets();
  native->set(cutoff,23);select(bank);check(native->get(cutoff)==77,"native selection reloads same bank sound after edits");
  select(loose);check(native->get(cutoff)==60,"native PRESET selection loads loose Vital file");
  // 0.3.27: HOME shows what the sound says about itself.
  {auto text=[&](const char* name){return native->displayText(id(name));};
   check(text("display_sound_info_0")=="Test Author  |  Lead"&&text("display_sound_note_0")=="A short note about this sound."&&text("display_macro_name_0")=="BRIGHT"&&text("display_macro_name_1")=="MOVE"&&text("display_macro_name_3")=="Macro 4",
       "HOME shows author, style, comment and macro names of the loaded sound; an unnamed macro is Macro N");
   void* saved=nullptr;auto savedBytes=dispatch(effGetChunk,0,0,&saved);std::vector<uint8_t> project((uint8_t*)saved,(uint8_t*)saved+savedBytes);
   native->testInit();waitNative();
   check(text("display_sound_info_0").empty()&&text("display_sound_note_0").empty()&&text("display_macro_name_0")=="Macro 1","Init clears the texts of the sound");
   dispatch(effSetChunk,0,project.size(),project.data());waitNative();
   check(text("display_sound_info_0")=="Test Author  |  Lead"&&text("display_macro_name_2")=="TONE"&&native->get(cutoff)==60,"the texts of the sound come back with the project");}
  select(unsupported);check(native->get(cutoff)==60&&native->displayText(id("library_loaded_0"))=="01 Import Test"&&native->getInt(id("load_status"))==2,"unsupported native program preserves last successful sound and reports failure");
  dispatch(effSetProgram,0,bank);dispatch(effSetProgram,0,loose);dispatch(effSetProgram,0,bank);waitNative();check(native->get(cutoff)==77,"rapid native program changes load the final selection");
  native->set(cutoff,42);bankAssets=native->testAssets();void* chunk=nullptr;auto bytes=dispatch(effGetChunk,0,0,&chunk);std::vector<uint8_t> edited(static_cast<uint8_t*>(chunk),static_cast<uint8_t*>(chunk)+bytes);
  dispatch(effSetProgram,0,loose);dispatch(effSetChunk,0,edited.size(),edited.data());waitNative();
  if(std::abs(native->get(cutoff)-42)>=1e-4||native->testAssets()!=bankAssets||native->displayText(id("library_loaded_0"))!="02 Bank Test")std::cout<<"RESTORE DETAIL cutoff="<<native->get(cutoff)<<" name="<<native->displayText(id("library_loaded_0"))<<" status="<<native->displayText(id("load_status"))<<" diff="<<nlohmann::json::diff(bankAssets,native->testAssets()).dump()<<std::endl;
  check(std::abs(native->get(cutoff)-42)<1e-4&&native->testAssets()==bankAssets&&native->displayText(id("library_loaded_0"))=="02 Bank Test","project chunk overrides pending native selection and restores edited bank sound");
  native->testInit();waitNative();check(native->get(cutoff)!=42&&native->displayText(id("library_loaded_0"))=="Init","Init replaces imported sound");select(bank);check(native->get(cutoff)==77,"native bank loads again after Init");
  bool audioFinite=true;for(int i=0;i<40;++i){effect->processReplacing(effect,nullptr,outputs,128);for(int n=0;n<128;++n)audioFinite&=std::isfinite(left[n])&&std::isfinite(right[n]);}check(audioFinite,"native VST processReplacing stays finite across program changes");
  check(displayUpdates.load()>0&&!displayOnAudio.load(),"host display invalidation is reentrant and stays off the audio callback");
  // Drive the real browser command parameters and actual exported program table.
  auto command=[&](int n){int p=id("storage_command_"+std::to_string(n));effect->setParameter(effect,p,native->getNormalised(p)>.5f?0:1);waitNative();};
  auto openItem=[&](const std::string& text){
    // An opened bank is shown like a folder (0.3.19): leave it before looking for the next item.
    {auto place=native->displayText(id("storage_path_0"));auto ends=[&](const char* tail){std::string t=tail;return place.size()>=t.size()&&place.compare(place.size()-t.size(),t.size(),t)==0;};
     if(ends(".vitalbank")||ends(".zip"))command(1);}
    for(int page=0;page<10;++page){for(int row=0;row<6;++row){auto name=native->displayText(id("storage_row_"+std::to_string(row*2)))+native->displayText(id("storage_row_"+std::to_string(row*2+1)));if(name.find(text)!=std::string::npos){command(6+row);return true;}}command(4);}return false;
  };
  command(0);check(openItem("Plugin Content")&&openItem("Storage Tests"),"storage browser navigates drive and directory through UI command parameters");
  float unchanged=native->get(cutoff);
  check(openItem("A small"),"browser opens external bank outside Content/Banks");
  check(native->get(cutoff)==unchanged&&effect->numPrograms>=1&&effect->numPrograms<=2,"bank indexing publishes names without changing sounding parameters");
  char externalName[256]{};dispatch(effGetProgramNameIndexed,0,0,externalName);
  check(std::string(externalName)=="Same name","selected bank sounds appear in native PRESET program names");
  select(0);check(native->get(cutoff)==31,"native PRESET loads selected external bank");
  command(3);check(openItem("B large"),"browser selects larger bank");
  check(effect->numPrograms==4&&native->get(cutoff)==31,"larger bank refresh preserves current sound in a valid retained slot");
  for(int n=0;n<1000&&cachedProgramCount.load()!=effect->numPrograms;++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  check(cachedProgramCount.load()==4&&validReentrantNames.load(),"host refresh callback reentrantly reads the new program count and all names");
  select(2);check(native->get(cutoff)==73,"larger bank last sound reaches correct archive entry");
  command(3);check(openItem("C same size"),"browser selects smaller bank");
  check(effect->numPrograms==2&&dispatch(effGetProgramNameIndexed,2,0,externalName)==0,"smaller bank removes stale names and rejects removed program indices");
  select(0);check(native->get(cutoff)==81,"same preset filename in different bank loads correct sound");
  void* bankChunk=nullptr;auto bankBytes=dispatch(effGetChunk,0,0,&bankChunk);std::vector<uint8_t> savedBank((uint8_t*)bankChunk,(uint8_t*)bankChunk+bankBytes);
  command(3);openItem("D empty");check(native->get(cutoff)==81&&effect->numPrograms==2,"empty bank preserves current catalog and sound");
  command(3);openItem("E broken");check(native->get(cutoff)==81&&effect->numPrograms==2,"malformed bank preserves current catalog and sound");
  command(3);openItem("A small");select(0);check(native->get(cutoff)==31,"external bank A/B/A switching is stable");
  dispatch(effSetChunk,0,savedBank.size(),savedBank.data());waitNative();
  check(native->get(cutoff)==81&&native->displayText(id("active_bank_0"))=="C same size"&&dispatch(effGetProgram)==0,"project restore recovers bank identity and current native slot");
  auto removed=std::filesystem::path(argv[3])/"Content/Storage Tests/C same size.vitalbank";
  auto heldPath=removed;heldPath+=".test-unavailable";std::filesystem::rename(removed,heldPath);
  dispatch(effSetChunk,0,savedBank.size(),savedBank.data());waitNative();
  check(native->get(cutoff)==81&&native->displayText(id("library_loaded_0"))=="Same name","embedded project sound restores when original bank drive is unavailable");
  {AEffect* fresh=VSTPluginMain(displayHost);auto* instance=static_cast<Vitaleon*>(fresh->object);fresh->dispatcher(fresh,effOpen,0,0,nullptr,0);fresh->dispatcher(fresh,effSetSampleRate,0,0,nullptr,44100);fresh->dispatcher(fresh,effMainsChanged,0,1,nullptr,0);fresh->dispatcher(fresh,effSetChunk,0,savedBank.size(),savedBank.data(),0);
   for(int n=0;n<10000&&instance->busy();++n){fresh->processReplacing(fresh,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   char retainedName[256]{};fresh->dispatcher(fresh,effGetProgramName,0,0,retainedName,0);
   check(!instance->busy()&&instance->get(cutoff)==81&&std::string(retainedName)=="[Current sound]","fresh instance restores missing-bank project into a valid current-sound slot");fresh->dispatcher(fresh,effClose,0,0,nullptr,0);}
  std::filesystem::rename(heldPath,removed);
  command(5);check(effect->numPrograms==12&&native->get(cutoff)==81,"Installed banks returns original catalog while retaining external sound");
  command(0);check(!native->displayText(id("storage_row_0")).empty(),"Drives rescans storage roots");
  openItem("Plugin Content");openItem("Storage Tests");openItem("Folder 0");bool emptyRows=true;for(int row=0;row<6;++row)emptyRows&=native->getNormalised(id("storage_available_"+std::to_string(row)))==0;
  check(emptyRows,"empty folder hides all six OPEN buttons and keeps navigation available");command(0);
  // 0.3.19 library: a tapped button goes back to off, a bank opens like a folder,
  // its sounds are LOAD rows, UP leaves the bank, removable drives come first.
  {void* before=nullptr;auto beforeBytes=dispatch(effGetChunk,0,0,&before);std::vector<uint8_t> kept((uint8_t*)before,(uint8_t*)before+beforeBytes);
   check(native->getNormalised(id("storage_command_0"))==0,"a tapped library button returns to off");
   check(native->displayText(id("storage_row_0"))=="DRIVE"&&native->getNormalised(id("storage_available_0"))==0.5f,"the drive list shows kind and an OPEN row");
   openItem("Plugin Content");openItem("Storage Tests");
   // The folder has more than six entries: look through the pages.
   auto findRow=[&](const std::string& name){for(int page=0;page<10;++page){for(int row=0;row<6;++row)if(native->displayText(id("storage_row_"+std::to_string(row*2+1)))==name)return row;command(4);}return -1;};
   int bankAt=findRow("B large");bool bankRow=bankAt>=0&&native->displayText(id("storage_row_"+std::to_string(bankAt*2)))=="BANK"&&native->getNormalised(id("storage_available_"+std::to_string(bankAt)))==0.5f;
   check(bankRow,"a bank is listed by name without extension and offers OPEN");
   openItem("B large");
   check(native->displayText(id("storage_row_0"))=="SOUND"&&native->displayText(id("storage_row_1"))=="Same name"&&native->displayText(id("storage_row_3"))=="Sound 1"&&native->getNormalised(id("storage_available_1"))==1.0f&&native->getNormalised(id("storage_available_3"))==0,"an opened bank lists its sounds as LOAD rows");
   command(7);check(native->get(cutoff)==72&&native->testAssets().value("preset_name",std::string())=="Sound 1","LOAD on a bank row loads that sound");
   command(1);bool back=findRow("B large")>=0;
   check(back,"UP leaves the bank and shows its folder again");
   check(vitaleon_storage::drivePriority("vfat")<vitaleon_storage::drivePriority("ext4")&&vitaleon_storage::drivePriority("exfat")==1,"removable file systems are listed before internal ones");
   {std::string path,type;check(vitaleon_storage::parseMount("98 61 8:17 / /media/USB\\040STICK rw,noexec - vfat /dev/sdb1 rw,fmask=0022",path,type)&&path=="/media/USB STICK"&&type=="vfat","mount lines give the path and the file system");}
   command(5);command(0);dispatch(effSetChunk,0,kept.size(),kept.data());waitNative();}
  // 0.3.25 import: the library loads wavetables, WAV files and LFO shapes straight from a drive.
  {void* before=nullptr;auto beforeBytes=dispatch(effGetChunk,0,0,&before);std::vector<uint8_t> kept((uint8_t*)before,(uint8_t*)before+beforeBytes);
   const auto imports=std::filesystem::path(argv[3])/"Content/Storage Tests/Imports";
   // A wavetable file: the current table of oscillator 1 under another name.
   {auto table=native->testAssets()["wavetables"][0];table["name"]="Made Table";std::ofstream file(imports/"Made Table.vitaltable",std::ios::binary);file<<table.dump();}
   auto setTarget=[&](int n){effect->setParameter(effect,id("import_target"),float(n)/3);waitNative();};
   auto status=[&]{std::string text;for(int i=0;i<4;++i)text+=native->displayText(id("storage_status_"+std::to_string(i)));return text;};
   auto has=[&](const std::string& part){return status().find(part)!=std::string::npos;};
   auto enter=[&]{command(0);return openItem("Plugin Content")&&openItem("Storage Tests")&&openItem("Imports");};
   auto load=[&](const std::string& name){return enter()&&openItem(name);};
   auto kindOf=[&](const std::string& name){for(int page=0;page<10;++page){for(int row=0;row<6;++row)if(native->displayText(id("storage_row_"+std::to_string(row*2+1)))==name)
       return native->displayText(id("storage_row_"+std::to_string(row*2)))+(native->getNormalised(id("storage_available_"+std::to_string(row)))==1.0f?" LOAD":" OPEN");command(4);}return std::string("missing");};
   // How much of a table frame is one sine with this many cycles (1 = all of it).
   auto sineShare=[&](int oscillator,int frame,int cycles){const float* wave=native->testWaveFrame(oscillator,frame);double s=0,c=0,energy=0;
       for(int i=0;i<2048;++i){const double phase=2*3.14159265358979323846*cycles*i/2048.0;s+=wave[i]*std::sin(phase);c+=wave[i]*std::cos(phase);energy+=double(wave[i])*wave[i];}
       return energy>0?std::sqrt((s*s+c*c)/(energy*1024)):0.0;};
   auto name=[&](int oscillator){return native->testAssets()["wavetables"][oscillator].value("name",std::string());};
   check(enter()&&kindOf("Cycle 600")=="WAV LOAD","the library lists a WAV file with LOAD");
   check(enter()&&kindOf("Made Table")=="TABLE LOAD"&&enter()&&kindOf("Shape")=="LFO LOAD","the library lists wavetable files and LFO shapes with LOAD");
   const int osc2On=native->controlIndex("osc_2_on");native->set(osc2On,0);waitNative();
   setTarget(1);check(load("Cycle 600")&&name(1)=="Cycle 600"&&has("Table loaded: Cycle 600 > OSC 2"),"LOAD on a WAV puts it into the chosen oscillator and says so");
   check(sineShare(1,0,1)>.99,"a WAV shorter than two 2048-sample cycles is read as one cycle of its own length");
   check(native->get(osc2On)==1&&has("switched on")&&native->getInt(id("table_2"))==0,"the oscillator that received the table is switched on; its file selector shows no installed file");
   setTarget(0);check(load("Clm 1024")&&name(0)=="Clm 1024"&&sineShare(0,0,1)>.99&&sineShare(0,256,2)>.99,"the cycle length a WAV states (clm) is used");
   setTarget(2);check(load("Table 4x2048")&&name(2)=="Table 4x2048"&&sineShare(2,0,1)>.99&&sineShare(2,256,4)>.99,"a plain wavetable WAV is cut into 2048-sample cycles");
   setTarget(3);const int sampleOn=native->controlIndex("sample_on");native->set(sampleOn,0);waitNative();
   check(load("Kick")&&native->testAssets()["sample"].value("name",std::string())=="Kick"&&native->testAssets()["sample"].count("samples_stereo")&&native->get(sampleOn)==1&&has("Sample loaded: Kick"),"with the target on Sample a WAV becomes the sample, stereo kept, sampler on");
   check(native->getNormalised(id("display_sample_0"))>native->getNormalised(id("display_sample_127"))+.2f,"the picture of the sample follows the loaded sample (a decaying hit starts high and ends low)");
   check(native->getNormalised(id("display_sample_fine_0"))>native->getNormalised(id("display_sample_fine_255"))+.2f,"so does the finer picture of the SAMPLE page, in 256 slices");
   check(load("Made Table")&&name(2)=="Made Table"&&has("Table loaded: Made Table > OSC 3"),"a wavetable file goes to the oscillator chosen last while the target is Sample");
   {const auto assets=native->testAssets();
    check(load("Broken")&&has("Cannot open")&&has("not a readable WAV")&&native->testAssets()==assets,"an unreadable WAV is refused with its reason and changes nothing");}
   {const int lfo=native->getInt(id("edit_lfo"));
    check(load("Shape")&&native->testAssets()["lfos"][lfo-1].value("num_points",0)==3&&has("LFO shape loaded: Shape > LFO "+std::to_string(lfo)),"an LFO shape goes to the LFO that is selected");}
   {const auto assets=native->testAssets();void* saved=nullptr;auto savedBytes=dispatch(effGetChunk,0,0,&saved);std::vector<uint8_t> project((uint8_t*)saved,(uint8_t*)saved+savedBytes);
    native->testInit();waitNative();bool cleared=name(1)!="Cycle 600";dispatch(effSetChunk,0,project.size(),project.data());waitNative();
    if(!cleared||native->testAssets()!=assets){auto diff=nlohmann::json::diff(assets,native->testAssets()).dump();std::cout<<"IMPORT RESTORE DETAIL cleared="<<cleared<<" status="<<status()<<" diff="<<diff.substr(0,600)<<std::endl;}
    check(cleared&&native->testAssets()==assets&&name(1)=="Cycle 600","imported tables and the sample come back with the project, without the files");}
   // 0.3.28: TABLE LIST on an oscillator page and SAMPLE LIST on the sample page open the library
   // on the plugin's own folder, with the WAV target set to where the button was pressed.
   {auto place=[&]{return native->displayText(id("storage_path_0"));};
    auto ends=[&](const std::string& tail){auto p=place();return p.size()>=tail.size()&&p.compare(p.size()-tail.size(),tail.size(),tail)==0;};
    native->set(id("ui_page"),5);waitNative();native->set(id("ui_browse"),2);waitNative();
    check(native->getInt(id("ui_page"))==3&&native->getInt(id("import_target"))==1&&native->getInt(id("ui_browse"))==0&&ends("Wavetables"),
        "TABLE LIST opens the library on the plugin's wavetables with the target on that oscillator");
    check(openItem("Browse Table")&&name(1)=="Browse Table"&&has("Table loaded: Browse Table > OSC 2"),"a table tapped in that list loads into the oscillator the list was opened from");
    native->set(id("ui_browse"),4);waitNative();
    check(native->getInt(id("import_target"))==3&&ends("Samples")&&openItem("Browse Hit")&&native->testAssets()["sample"].value("name",std::string())=="Browse Hit",
        "SAMPLE LIST opens the plugin's samples and a tap loads one");
    command(1);check(ends("Content")||place().find("Content")!=std::string::npos,"UP leaves the folder towards the other drives");}
   // 0.3.31: the sample on one page: ten controls on the Q-Links, the name of the loaded sample.
   {auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
    const auto pages=nlohmann::json::parse(surfaceMapJson);check(pages[7]["chart"]=="sample"&&pages[7]["sections"].size()==1&&pages[7]["sections"][0]["items"].size()==10&&pages[7]["extra"].size()==1&&pages[8]["chart"]=="filters",
        "the sample has one page without sections: ten controls on the Q-Links, the on switch beside them");
    check(native->displayText(id("display_sample_name"))=="Browse Hit","the SAMPLE page names the loaded sample");
    native->set(id("ui_page"),7);native->set(id("ui_section"),0);waitNative();
    native->setNormalised(id("ui_knob_0"),.33f);native->setNormalised(id("ui_knob_1"),.71f);native->setNormalised(id("ui_knob_3"),.64f);
    const float loop=at("sample_loop"),path=at("sample_destination");
    native->setNormalised(id("ui_knob_4"),loop>.5f?0.0f:1.0f);native->setNormalised(id("ui_knob_7"),1.0f);native->setNormalised(id("ui_knob_8"),path>.5f?0.0f:1.0f);
    check(std::abs(at("sample_level")-.33f)<1e-6&&std::abs(at("sample_pan")-.71f)<1e-6&&std::abs(at("sample_tune")-.64f)<1e-6&&at("sample_loop")!=loop&&at("sample_random_phase")==1&&at("sample_destination")!=path,
        "SAMPLE Q-Links: level, pan, pitch, tune / loop, bounce, key tracking, random phase / path, pitch quantise");
    native->set(native->controlIndex("sample_loop"),loop>.5f?1.0f:0.0f);native->set(native->controlIndex("sample_random_phase"),0);
    native->setNormalised(native->controlIndex("sample_destination"),path);native->setNormalised(native->controlIndex("sample_level"),.707107f);native->setNormalised(native->controlIndex("sample_pan"),.5f);native->setNormalised(native->controlIndex("sample_tune"),.5f);
    // Let the Q-Link notifications of this page go out before the page changes.
    for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    native->set(id("ui_page"),0);waitNative();}
   command(5);command(0);dispatch(effSetChunk,0,kept.size(),kept.data());waitNative();}
  // Every rendered module/section must remap Q-Links to its actual parameters.
  auto spec=nlohmann::json::parse(surfaceMapJson);bool mapped=true;int sectionCount=0;nlohmann::json snapshots;
  for(int page=0;page<int(spec.size());++page){native->set(id("ui_page"),float(page));
    for(int sec=0;sec<int(spec[page]["sections"].size());++sec){native->set(id("ui_section"),float(sec));waitNative();++sectionCount;
      int slot=0;for(const auto& value:spec[page]["sections"][sec]["items"]){std::string name=value;if(name.empty()){mapped&=native->displayText(id("ui_knob_"+std::to_string(slot)))=="Unused";++slot;continue;}int target=-1;for(int p=0;p<native->numParams();++p)if(native->param(p).name==name)target=p;if(target<0){std::replace(name.begin(),name.end(),'_',' ');for(int p=0;p<native->numParams();++p)if(native->param(p).name==name)target=p;}mapped&=target>=0&&std::abs(native->getNormalised(id("ui_knob_"+std::to_string(slot)))-native->getNormalised(target))<1e-6;++slot;}
      if(argc>2){auto values=nlohmann::json::array();for(int p=0;p<native->numParams();++p)values.push_back({{"index",p},{"name",native->param(p).name},{"value",native->getNormalised(p)},{"display",native->displayText(p)}});snapshots[std::to_string(page)+"-"+std::to_string(sec)]=std::move(values);}
    }
  }
  check(mapped&&sectionCount==91,"all 43 module views and 91 sections map Q-Links to visible controls");
  if(argc>2){std::ofstream f(std::string(argv[2])+"-surface.json");f<<snapshots.dump();}
  native->set(id("ui_page"),8);native->set(id("ui_section"),0);native->setNormalised(id("ui_knob_0"),.37f);check(std::abs(native->getNormalised(cutoff)-.37f)<1e-6,"filter cutoff Q-Link writes the actual full-engine filter");
  native->set(id("ui_page"),15);native->setNormalised(id("ui_knob_0"),.4f);check(std::abs(native->getNormalised(native->controlIndex("env_6_attack"))-.4f)<1e-6,"Envelope 6 view edits Envelope 6 through Q-Link");
  native->set(id("ui_page"),23);native->setNormalised(id("ui_knob_0"),.6f);check(std::abs(native->getNormalised(native->controlIndex("lfo_8_frequency"))-.6f)<1e-6,"LFO 8 view edits LFO 8 through Q-Link");
  // The sound parameter is announced once the control rests (0.3.23), not while it moves.
  for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
  check(native->testSlotsAtRest()&&automatedTarget.load()==native->controlIndex("lfo_8_frequency"),"Q-Link automation reports stable LFO 8 parameter rather than navigation proxy");
  // 0.3.22: knobs and faders on screen are bound to their Q-Link slot, so the host moves
  // the control it wrote. 0.3.23: while a control moves, the plugin calls the host not
  // once (0.3.21 redrew the page on every change and all controls stuttered; 0.3.22 sent
  // audioMasterAutomate on every change and the controls felt sluggish on the device).
  {native->set(id("visuals"),0);native->set(id("ui_page"),0);native->set(id("ui_section"),0);
   auto idle=[&](int blocks){for(int i=0;i<blocks;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
   auto rest=[&]{for(int i=0;i<1000&&!native->testSlotsAtRest();++i)idle(1);idle(20);};
   auto named=[&](const std::string& name){for(int p=0;p<native->numParams();++p)if(native->param(p).name==name)return p;return -1;};
   // HOME, first Q-Link: the first macro (0.3.27; it was the master volume).
   int knob=id("ui_knob_0"),sound=native->controlIndex("macro_control_1"),volume=sound;effect->setParameter(effect,knob,.5f);rest();
   unsigned shown=displayUpdates.load(),calls=automateCalls.load();float turned=.8f;automatedTarget.store(-1);
   for(int step=1;step<=20;++step){effect->setParameter(effect,knob,.5f+step*.015f);idle(2);}
   check(displayUpdates.load()==shown&&automateCalls.load()==calls,"a moving control makes no call into the host");
   check(std::abs(native->getNormalised(sound)-turned)<1e-5&&std::abs(effect->getParameter(effect,knob)-turned)<1e-5,"a Q-Link turn reaches the sound and the slot keeps the value the host wrote");
   rest();
   check(automateCalls.load()==calls+1&&automatedTarget.load()==volume&&displayUpdates.load()==shown,"a rested control announces its sound parameter once, without a page redraw");
   check(native->getInt(id("ui_bank"))==0,"the first four Q-Links are group 1");
   // Another knob group: the frame parameter is announced at once, the page redrawn at rest.
   unsigned framed=readoutUpdates.load();shown=displayUpdates.load();
   for(int step=0;step<10;++step){effect->setParameter(effect,id("ui_knob_5"),.3f+step*.01f);idle(2);}
   check(native->getInt(id("ui_bank"))==1&&readoutUpdates.load()==framed+1&&displayUpdates.load()==shown,"a knob of the second group moves the Q-Link frame without a page redraw while it turns");
   rest();check(displayUpdates.load()==shown+1,"the page is redrawn once after the knob group changed and the control rests");
   effect->setParameter(effect,knob,turned);rest();check(native->getInt(id("ui_bank"))==0,"a knob of the first group moves the frame back");
   // Small Q-Link increments on a stepped control add up instead of snapping back to the step.
   {native->set(id("ui_page"),1);native->set(id("ui_section"),0);idle(20);   // Voices lives on the VOICES page since 0.3.27
    int voices=named("Voices"),slot=id("ui_knob_0");native->set(voices,4);idle(10);float start=effect->getParameter(effect,slot);int count=native->getInt(voices);
    for(int step=1;step<=24;++step){effect->setParameter(effect,slot,effect->getParameter(effect,slot)+.004f);idle(3);}
    check(std::abs(effect->getParameter(effect,slot)-(start+.096f))<1e-4&&native->getInt(voices)==count+1,"small Q-Link increments on a stepped control add up");rest();
    // 0.3.24: the voice control ends at eight, also for a project saved with more.
    effect->setParameter(effect,voices,1.0f);check(native->getInt(voices)==8&&native->param(voices).max==8,"the voice control ends at eight voices");
    {native->set(voices,3);rest();auto saved=native->internalSaveState();std::vector<uint8_t> chunk(saved.begin(),saved.end());
     const std::string key=std::string(1,char(6))+"voices";auto at=std::search(chunk.begin(),chunk.end(),key.begin(),key.end());bool found=at!=chunk.end();
     if(found){size_t pos=size_t(at-chunk.begin())+key.size();float plain=24.0f,norm=23.0f/31.0f;std::memcpy(&chunk[pos],&plain,4);std::memcpy(&chunk[pos+4],&norm,4);}
     bool accepted=found&&native->internalLoadState(chunk.data(),chunk.size());for(int i=0;i<3000&&native->busy();++i)idle(1);rest();
     check(found&&accepted&&native->getInt(voices)==8,"a project saved with 24 voices is restored at eight");}}
   // A list on screen is bound to its own parameter: the host hears of its step at once.
   native->set(id("ui_page"),16);native->set(id("ui_section"),0);idle(20);int list=named("LFO sync");
   bool stepped=list>=0&&native->param(list).kind!=mpc::Param::Kind::Float;float old=stepped?native->getNormalised(list):0;automatedTarget.store(-1);
   effect->setParameter(effect,id("ui_knob_3"),old>.5f?0.0f:1.0f);for(int i=0;i<100&&automatedTarget.load()<0;++i)idle(1);
   check(stepped&&native->getNormalised(list)!=old&&automatedTarget.load()==list,"a list turned by Q-Link is announced to the host for redrawing");
   rest();native->set(id("ui_page"),0);native->set(id("visuals"),1);}
  // Single .vital files on any drive: tap = load, folder becomes the PRESET list.
  command(0);openItem("Plugin Content");openItem("Storage Tests");openItem("Loose Sounds");
  check(openItem("Second loose"),"browser lists single .vital files");
  check(native->get(cutoff)==53&&native->displayText(id("library_loaded_0"))=="Second loose","tapping a single .vital file loads that sound");
  check(native->displayText(id("load_status")).find("Loaded with changes")==0,"status names what a tolerant import left out");
  check(native->displayText(id("active_bank_0"))=="Loose Sounds"&&dispatch(effGetProgramNameIndexed,0,0,externalName)==1&&std::string(externalName)=="First loose","folder of loose sounds becomes the native PRESET list");
  select(0);check(native->get(cutoff)==52&&native->displayText(id("load_status"))=="Loaded: First loose","other sounds of that folder load from PRESET");
  {void* chunk2=nullptr;auto n2=dispatch(effGetChunk,0,0,&chunk2);std::vector<uint8_t> saved((uint8_t*)chunk2,(uint8_t*)chunk2+n2);command(5);
   dispatch(effSetChunk,0,saved.size(),saved.data());waitNative();
   check(native->get(cutoff)==52&&native->displayText(id("active_bank_0"))=="Loose Sounds","project restore recovers a folder-based sound list");}
  // 0.3.39: the bank selector (one installed bank in PRESET) and the install switch.
  {void* held=nullptr;auto heldBytes=dispatch(effGetChunk,0,0,&held);std::vector<uint8_t> before((uint8_t*)held,(uint8_t*)held+heldBytes);
   namespace fs=std::filesystem;std::error_code ec;const fs::path content=fs::path(argv[3])/"Content";
   auto bankName=[&]{return native->displayText(id("active_bank_0"))+native->displayText(id("active_bank_1"));};
   auto statusText=[&]{std::string text;for(int i=0;i<4;++i)text+=native->displayText(id("storage_status_"+std::to_string(i)));return text;};
   auto step=[&](int which){effect->setParameter(effect,id("bank_step"),float(which)/3);waitNative();};
   auto lists=[&](const std::string& wanted){for(int p=0;p<effect->numPrograms;++p){char name[256]{};dispatch(effGetProgramNameIndexed,p,0,name);if(wanted==name)return true;}return false;};
   const float sounding=native->get(cutoff);
   step(2);
   {char first[256]{};dispatch(effGetProgramNameIndexed,0,0,first);(void)sounding;
    check(bankName()=="Imported Bank"&&lists("02 Bank Test")&&!lists("01 Import Test")&&!lists("[Current sound]")&&native->displayText(id("library_loaded_0"))==first
          &&dispatch(effGetProgram,0,0,nullptr)==0&&native->getInt(id("bank_step"))==0,
       "BANK > puts the first installed bank into PRESET, alone, and loads its first sound");}
   step(2);const bool second=bankName()=="Second Bank";
   step(2);const bool looseOnes=bankName()=="Loose presets"&&lists("01 Import Test")&&!lists("02 Bank Test");
   step(2);const bool all=bankName()=="All installed banks"&&effect->numPrograms>=11;step(2);
   if(!(second&&looseOnes&&all))std::cout<<"BANK DETAIL second="<<second<<" loose="<<looseOnes<<" all="<<all<<" now="<<bankName()<<" programs="<<effect->numPrograms<<std::endl;
   check(second&&looseOnes&&all&&bankName()=="Imported Bank","the selector walks through every installed bank, the loose sounds and all sounds, and wraps around");
   step(1);const bool back=bankName()=="All installed banks";step(1);step(1);
   // 0.4.0: the step loads the bank's first sound, and the status line ends on what was loaded.
   {char first[256]{};dispatch(effGetProgramNameIndexed,0,0,first);const bool loaded=statusText()==std::string("Loaded: ")+first&&native->displayText(id("library_loaded_0"))==first;
    if(!(back&&bankName()=="Second Bank"&&loaded))std::cout<<"BANK BACK DETAIL back="<<back<<" bank="<<bankName()<<" status="<<statusText()<<" first="<<first<<std::endl;
    check(back&&bankName()=="Second Bank"&&loaded,"< BANK walks the other way, and the status line names the sound that was loaded");}
   select(0);check(native->get(cutoff)==88&&native->displayText(id("library_loaded_0"))=="02 Bank Test","a sound of the chosen bank loads from PRESET");
   native->set(id("ui_page"),0);waitNative();step(3);
   auto rows=[&]{std::string text;for(int row=0;row<6;++row)text+=native->displayText(id("storage_row_"+std::to_string(row*2)))+":"+native->displayText(id("storage_row_"+std::to_string(row*2+1)))+"|";return text;};
   if(rows().find("BANK:Imported Bank|BANK:Second Bank|")!=0)std::cout<<"BANK LIST DETAIL "<<rows()<<" status="<<statusText()<<std::endl;
   check(native->getInt(id("ui_page"))==3&&rows().find("BANK:Imported Bank|BANK:Second Bank|")==0&&statusText().find("Installed banks.")==0,"BANK LIST opens LIBRARY with the installed banks as rows");
   check(openItem("Imported Bank")&&bankName()=="Imported Bank"&&lists("02 Bank Test"),"OPEN on a row of that list puts the bank into PRESET");
   // Install: a bank from another place is copied into the plugin's Banks folder only with the switch on.
   const fs::path smallCopy=content/"Banks/A small.vitalbank",copy=content/"Banks/B large.vitalbank",source=content/"Storage Tests/B large.vitalbank";
   fs::remove(smallCopy,ec);fs::remove(copy,ec);
   command(0);openItem("Plugin Content");openItem("Storage Tests");openItem("A small");
   check(!fs::exists(smallCopy)&&bankName()=="A small"&&native->getNormalised(id("bank_keep"))==0,"with INSTALL off a bank from another drive is used where it lies");
   effect->setParameter(effect,id("bank_keep"),1);waitNative();const bool told=statusText().find("INSTALL on")==0;
   command(0);openItem("Plugin Content");openItem("Storage Tests");openItem("B large");
   fs::path part=copy;part+=".part";
   const bool copied=fs::exists(copy)&&fs::file_size(copy,ec)==fs::file_size(source,ec)&&!fs::exists(part);
   if(!(told&&copied))std::cout<<"INSTALL DETAIL told="<<told<<" copied="<<copied<<" status="<<statusText()<<std::endl;
   check(told&&copied&&bankName()=="B large"&&statusText().find("Installed on the MPC. 3 sounds.")==0&&effect->numPrograms==4,"with INSTALL on the bank is copied into the plugin's Banks folder and opened from there");
   fs::path away=source;away+=".test-unavailable";fs::rename(source,away,ec);
   select(2);const bool fromCopy=native->get(cutoff)==73;fs::rename(away,source,ec);
   check(fromCopy,"the installed bank plays without the drive it came from");
   step(2);const bool after=bankName()=="Imported Bank";step(1);
   check(after&&bankName()=="B large","the installed bank is one of the banks of the selector");
   command(0);openItem("Plugin Content");openItem("Storage Tests");openItem("B large");
   check(!fs::exists(content/"Banks/B large (2).vitalbank")&&bankName()=="B large","a bank that is installed already is not copied a second time");
   effect->setParameter(effect,id("bank_keep"),0);waitNative();
   check(statusText().find("INSTALL off")==0,"the status line says what the INSTALL switch does");
   dispatch(effSetChunk,0,before.size(),before.data());waitNative();
   fs::remove(copy,ec);
   check(!fs::exists(copy)&&bankName()=="Loose Sounds"&&native->get(cutoff)==sounding,"the test leaves the banks folder and the project as it found them");
   command(0);native->set(id("ui_page"),0);waitNative();}
  // Matrix: PICK on a row -> group page -> entry page -> back on the row's matrix page.
  {int matrix=-1,groups=-1,targets=-1;for(int page=0;page<int(spec.size());++page){if(!spec[page]["chart"].is_string())continue;std::string chart=spec[page]["chart"];if(chart=="mxtable")matrix=page;if(chart=="pickgroup")groups=page;if(chart=="picktarget")targets=page;}
   check(matrix>=0&&groups>=0&&targets>=0&&spec[groups]["hidden"]==true&&spec[targets]["hidden"]==true,"matrix table and hidden picker views exist");
   native->set(id("ui_page"),float(matrix));waitNative();native->set(id("ui_pick"),10);waitNative();
   check(native->getInt(id("ui_page"))==groups&&native->getInt(id("ui_section"))==0&&native->getInt(id("edit_route"))==10&&native->getInt(id("ui_pick"))==0,"PICK on a matrix row selects that route, opens the group page and re-arms itself");
   // 0.3.43: the same page holds the sources of the route. A source is set and the page stays.
   const int sourceProxy=id("edit_route_0"),realSource=id("route_10_source"),realTarget=id("route_10_destination");
   auto named=[&](int parameter,const char* name){const auto& all=native->param(parameter).choices;for(int i=0;i<int(all.size());++i)if(all[size_t(i)]==name)return i;return -1;};
   const int lfo2=named(realSource,"lfo_2"),env3=named(realSource,"env_3"),macro1=named(realSource,"macro_control_1");
   native->set(sourceProxy,float(lfo2));waitNative();
   check(lfo2>0&&native->getInt(realSource)==lfo2&&native->getInt(id("ui_page"))==groups&&native->getInt(id("edit_route"))==10,"a source chosen on the page of a route sets that route's source, and the page stays for the target");
   native->set(sourceProxy,0);waitNative();
   check(native->getInt(realSource)==0&&native->getInt(id("ui_page"))==groups,"NO SOURCE clears the source of the route");
   native->set(sourceProxy,float(lfo2));waitNative();
   native->set(id("ui_section"),3);waitNative();
   check(native->getInt(id("ui_page"))==targets&&native->getInt(id("ui_section"))==3,"a group button opens that group's entries");
   int proxy=id("edit_route_1"),real=id("route_10_destination");native->set(proxy,57);waitNative();
   check(native->getInt(real)==57&&native->getInt(id("ui_page"))==matrix&&native->getInt(id("ui_section"))==1,"choosing an entry sets that route's target and returns to its matrix page");
   native->set(id("ui_pick"),10);waitNative();check(native->getInt(id("ui_page"))==groups,"the same row can be picked again");native->set(proxy,0);waitNative();
   check(native->getInt(real)==0&&native->getInt(id("ui_page"))==matrix,"NO TARGET clears the route and returns to the matrix");
   // 0.3.43: the matrix shows each source and target in the colour of its section. Route 10 is
   // the second row of the second page of routes: 0 none, 1 OSC, 2 FILTER, 3 ENV, 4 LFO, 5 FX.
   {auto settle=[&]{for(int i=0;i<80;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
    auto tints=[&]{settle();return std::make_pair(native->getInt(id("mx_source_tint_2")),native->getInt(id("mx_target_tint_2")));};
    auto route=[&](int source,const char* target){native->set(realSource,float(source));native->set(realTarget,float(named(realTarget,target)));waitNative();};
    check(native->getInt(id("ui_section"))==1&&native->getInt(realSource)==lfo2&&tints()==std::make_pair(4,0),"an LFO as the source shows in the colour of the LFOs, a route without a target in none");
    route(lfo2,"osc_1_level");const auto osc=tints();
    route(env3,"filter_2_cutoff");const auto filter=tints();
    route(macro1,"reverb_dry_wet");const auto fx=tints();
    route(env3,"lfo_1_frequency");const auto lfo=tints();
    route(lfo2,"env_1_attack");const auto env=tints();
    route(lfo2,"sample_level");const auto sample=tints();
    route(lfo2,"filter_fx_cutoff");const auto fxFilter=tints();
    check(osc==std::make_pair(4,1)&&filter==std::make_pair(3,2)&&fx==std::make_pair(0,5)&&lfo==std::make_pair(3,4)&&env==std::make_pair(4,3)&&sample.second==1&&fxFilter.second==5,
        "the matrix knows the section of every source and target: OSC and sample, FILTER, ENV, LFO, FX with its filter; a macro has no section");
    const float live=native->get(id("visuals"));native->set(id("visuals"),0);route(env3,"osc_2_pan");
    check(tints()==std::make_pair(3,1),"the colours follow with Live Graphics off as well");
    native->set(id("visuals"),live);
    native->set(id("ui_section"),0);waitNative();
    check(tints()==std::make_pair(0,0),"the colours belong to the routes on screen: another page of routes shows its own");
    native->set(realSource,0);native->set(realTarget,0);waitNative();settle();}
   native->set(id("ui_group"),0);waitNative();native->set(id("ui_group"),6);waitNative();
   check(native->getInt(id("ui_page"))==matrix,"the OTHER group remembers the matrix, never a hidden picker view");
   native->set(id("ui_page"),0);waitNative();}
  // Live graphs (0.3.18): a changed picture asks the host for ONE display update;
  // readouts are never sent as single parameter notifications (they delayed the
  // MPC's audio callback), and a picture that does not change asks for nothing.
  {native->set(id("ui_page"),4);waitNative();for(int i=0;i<120;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   unsigned single=readoutUpdates.load(),shown=displayUpdates.load();int frame=native->controlIndex("osc_1_wave_frame");native->set(frame,native->get(frame)<100?170.0f:20.0f);
   for(int i=0;i<2000&&displayUpdates.load()==shown;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   check(displayUpdates.load()>shown,"a changed oscillator graph asks the host for a display update");
   for(int i=0;i<150;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   unsigned still=displayUpdates.load();
   for(int i=0;i<200;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   check(displayUpdates.load()==still,"a graph that does not change asks for nothing");
   check(readoutUpdates.load()==single,"graph readouts are never sent as single parameter notifications");
   // 256 columns in three readout blocks (96 + 96 + 64, the last appended in 0.3.32); the frame is moved to a second shape so all three must follow.
   auto column=[&](int c){return native->getNormalised(c<96?id("display_wave_0_0")+c:c<192?id("display_wave_bank_0_0")+c-96:id("display_wave_fine_0_0")+c-192);};
   std::array<float,256> first{};for(int c=0;c<256;++c)first[size_t(c)]=column(c);
   native->set(frame,60.0f);for(int i=0;i<150;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   int low=0,middle=0,high=0;for(int c=0;c<256;++c)if(column(c)!=first[size_t(c)])(c<96?low:c<192?middle:high)++;
   if(!(low>8&&middle>8&&high>4))std::cout<<"WAVE COLUMNS DETAIL "<<low<<" "<<middle<<" "<<high<<std::endl;
   check(low>8&&middle>8&&high>4,"the oscillator graph is written in 256 columns across all three readout blocks");
   check(!displayOnAudio.load(),"display updates never run on the audio callback");}
  // 0.3.27: HOME's Q-Links reach the macros, the filter with its switch, the volume and the amp envelope.
  {native->set(id("ui_page"),0);native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_1"),.6f);native->setNormalised(id("ui_knob_4"),.37f);native->setNormalised(id("ui_knob_5"),.52f);native->setNormalised(id("ui_knob_7"),.7f);
   native->setNormalised(id("ui_knob_8"),.2f);native->setNormalised(id("ui_knob_9"),.33f);native->setNormalised(id("ui_knob_10"),.66f);native->setNormalised(id("ui_knob_11"),.45f);
   auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
   check(std::abs(at("macro_control_2")-.6f)<1e-6&&std::abs(at("filter_1_cutoff")-.37f)<1e-6&&std::abs(at("filter_1_resonance")-.52f)<1e-6&&std::abs(at("volume")-.7f)<1e-6,
       "HOME: Q-Links reach the macros, cutoff, resonance and volume");
   check(std::abs(at("env_1_attack")-.2f)<1e-6&&std::abs(at("env_1_decay")-.33f)<1e-6&&std::abs(at("env_1_sustain")-.66f)<1e-6&&std::abs(at("env_1_release")-.45f)<1e-6,
       "HOME: one Q-Link group holds attack, decay, sustain and release of the amp envelope");
   native->setNormalised(id("ui_knob_6"),1.0f);bool on=at("filter_1_on")==1;native->setNormalised(id("ui_knob_6"),0.0f);
   check(on&&at("filter_1_on")==0,"HOME: the filter switch sits between resonance and volume and follows its Q-Link");}
  // 0.3.29: an oscillator page has no sections; its sixteen Q-Links are four groups.
  {native->set(id("ui_page"),5);native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_0"),.42f);native->setNormalised(id("ui_knob_1"),.3f);native->setNormalised(id("ui_knob_7"),.61f);native->setNormalised(id("ui_knob_9"),.27f);native->setNormalised(id("ui_knob_15"),.73f);
   auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
   check(spec.size()==43&&spec[5]["sections"].size()==2&&spec[5]["sections"][0]["items"].size()==16&&spec[5]["sections"][1]["items"].size()==11&&spec[5]["extra"].size()==1,
       "an oscillator page shows its controls in two pages: sixteen main controls and the eleven others");
   check(std::abs(at("osc_2_level")-.42f)<1e-6&&std::abs(at("osc_2_wave_frame")-.3f)<1e-6&&std::abs(at("osc_2_distortion_phase")-.61f)<1e-6&&std::abs(at("osc_2_unison_detune")-.27f)<1e-6&&std::abs(at("osc_2_unison_blend")-.73f)<1e-6,
       "oscillator PAGE 1: the four large dials first (level, frame, spectral, warp), then pitch, unison and phase, types and routing");
   float before=at("osc_2_spectral_morph_type");native->setNormalised(id("ui_knob_12"),before>.5f?0.0f:1.0f);
   check(at("osc_2_spectral_morph_type")!=before,"a type list on an oscillator page follows its Q-Link");
   native->set(native->controlIndex("osc_2_spectral_morph_type"),0);
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   // PAGE 2 (0.3.41): the same sixteen knobs reach the eleven other controls, and only those.
   native->set(id("ui_section"),1);waitNative();
   const bool placed=native->getInt(id("ui_page"))==5&&native->getInt(id("ui_section"))==1;
   native->setNormalised(id("ui_knob_0"),.36f);native->setNormalised(id("ui_knob_4"),.58f);native->setNormalised(id("ui_knob_6"),.21f);
   check(placed&&std::abs(at("osc_2_stereo_spread")-.36f)<1e-6&&std::abs(at("osc_2_frame_spread")-.58f)<1e-6&&std::abs(at("osc_2_distortion_spread")-.21f)<1e-6&&std::abs(at("osc_2_level")-.42f)<1e-6
         &&native->displayText(id("ui_knob_11"))=="Unused"&&native->displayText(id("ui_knob_12"))=="Unused",
       "oscillator PAGE 2: the knobs reach stereo spread, frame spread and the other fine controls, and leave the main ones alone");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   // The wave picture stays on screen on PAGE 2: a changed frame reaches it there.
   {const int frame=native->controlIndex("osc_2_wave_frame");const unsigned shown=displayUpdates.load();native->set(frame,native->get(frame)<100?170.0f:20.0f);
    for(int i=0;i<2000&&displayUpdates.load()==shown;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(displayUpdates.load()>shown,"the wave picture is kept up to date on PAGE 2 of an oscillator");}
   native->set(id("ui_page"),6);waitNative();
   check(native->getInt(id("ui_section"))==0,"another oscillator opens on its PAGE 1");
   native->set(id("ui_section"),0);native->set(id("ui_page"),0);waitNative();}
  // 0.3.40: MOD holds the matrix; HOME > OTHER holds random and expression, HOME > ADVANCED any
  // parameter and the curve of a route. One Q-Link layer each.
  {auto at=[&](const std::string& key){return native->getNormalised(native->controlIndex(key));};
   check(spec[37]["chart"]=="other"&&spec[37]["group"]==0&&spec[37]["tab"]=="OTHER"&&spec[37]["sections"].size()==1&&spec[2]["chart"]=="advanced"&&spec[2]["sections"].size()==1&&spec[35]["redirect"]==37&&spec[36]["redirect"]==37&&spec[9]["redirect"]==8&&spec[34]["group"]==6,
       "HOME > OTHER holds random and expression, HOME > ADVANCED the route curve, one Q-Link layer each; the old RANDOM and EXPRESSION pages point to OTHER");
   native->set(id("ui_page"),35);waitNative();const bool fromRandom=native->getInt(id("ui_page"))==37&&native->getInt(id("ui_group"))==0;
   native->set(id("ui_page"),36);waitNative();
   check(fromRandom&&native->getInt(id("ui_page"))==37,"a project saved on RANDOM or EXPRESSION opens HOME > OTHER");
   native->set(id("ui_group"),6);waitNative();const bool matrix=native->getInt(id("ui_page"))==34;
   native->set(id("ui_group"),0);waitNative();
   check(matrix&&native->getInt(id("ui_page"))==37,"the MOD group opens the matrix, and HOME remembers its last page");
   native->set(id("ui_page"),37);native->set(id("ui_section"),0);waitNative();
   const float wheel=at("mod_wheel"),rate=at("random_1_frequency");
   native->set(id("edit_random"),1);native->setNormalised(id("ui_knob_1"),.42f);native->setNormalised(id("ui_knob_12"),.3f);
   check(std::abs(at("random_1_frequency")-.42f)<1e-6&&std::abs(at("mod_wheel")-.3f)<1e-6&&native->displayText(id("ui_knob_7"))=="Unused","HOME > OTHER: random source, its style, key tracking and expression, one knob bank each");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   native->set(id("ui_section"),1);waitNative();
   check(native->getInt(id("ui_section"))==0,"a project saved on the old second layer of OTHER opens its one layer");
   native->set(id("ui_page"),2);native->set(id("ui_section"),0);waitNative();const bool routeCurve=native->getInt(id("curve_target"))==1;
   native->set(id("edit_route"),5);waitNative();
   const float curve=at("modulation_5_power");native->setNormalised(id("ui_knob_5"),curve>.5f?.2f:.8f);
   check(routeCurve&&at("modulation_5_power")!=curve&&native->displayText(id("ui_knob_3"))=="Unused","HOME > ADVANCED shows the curve of the chosen route: any parameter, the curve and its point, one knob bank each");
   native->set(id("advanced_parameter"),0);native->setNormalised(id("ui_knob_1"),.61f);
   check(std::abs(native->getNormalised(native->controlIndex(native->param(id("advanced_parameter")).choices[0]))-.61f)<1e-5,"HOME > ADVANCED: the Value knob reaches the parameter chosen by name");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   native->setNormalised(native->controlIndex("modulation_5_power"),curve);native->setNormalised(native->controlIndex("mod_wheel"),wheel);native->setNormalised(native->controlIndex("random_1_frequency"),rate);
   native->set(id("edit_route"),1);native->set(id("ui_section"),0);native->set(id("ui_page"),0);waitNative();}
  // 0.3.36: no second page in FX. The overview shows the chain in its order; each effect has one page.
  {auto at=[&](const std::string& key){return native->getNormalised(native->controlIndex(key));};
   static const char* keys[9]={"chorus","compressor","delay","distortion","eq","filter_fx","flanger","phaser","reverb"};
   static const char* names[9]={"Chorus","Compressor","Delay","Distortion","EQ","FX Filter","Flanger","Phaser","Reverb"};
   bool one=spec[24]["chart"]=="effects"&&spec[24]["sections"].size()==1&&spec[24]["sections"][0]["items"].size()==9;
   for(int page=25;page<34;++page)one&=spec[page]["chart"]=="fx"&&spec[page]["sections"].size()==size_t(page==31?2:1)&&spec[page]["extra"].size()==1;
   check(one&&spec[34]["chart"]=="mxtable","the FX overview and eight effects have one section each, the compressor two Q-Link layers");
   native->set(id("ui_page"),24);native->set(id("ui_section"),0);waitNative();
   auto chain=[&](int* order){vital::utils::decodeFloatToOrder(order,native->get(native->controlIndex("effect_chain_order")),9);};
   int before[9];chain(before);
   bool named=true;for(int i=0;i<9;++i)named&=native->displayText(id("display_fx_slot_0")+i)==names[before[i]];
   check(named,"the FX overview names the effect at each place of the chain");
   // The switch of a place belongs to the effect that stands there, by knob and by finger.
   const std::string third=std::string(keys[before[2]])+"_on";
   native->setNormalised(id("ui_knob_2"),1.0f);const bool byKnob=at(third)==1;
   native->set(id("fx_slot_on_2"),0);const bool byFinger=at(third)==0;
   native->set(native->controlIndex(third),1);for(int i=0;i<20;++i)effect->processReplacing(effect,nullptr,outputs,128);
   const bool shown=native->getInt(id("fx_slot_on_2"))==1;native->set(native->controlIndex(third),0);
   check(byKnob&&byFinger&&shown,"a switch on the overview turns the effect at that place on and off and shows its state");
   // Choose place 3 and move it one earlier, then back.
   native->set(id("effect_slot"),3);native->set(id("ui_fx_move"),1);waitNative();
   int moved[9];chain(moved);
   check(moved[1]==before[2]&&moved[2]==before[1]&&moved[0]==before[0]&&native->getInt(id("effect_slot"))==2&&native->getInt(id("ui_fx_move"))==0&&native->displayText(id("display_fx_slot_1"))==names[before[2]],
       "EARLIER moves the chosen effect one place towards the start; the choice follows it");
   native->set(id("ui_fx_move"),2);waitNative();int back[9];chain(back);bool same=true;for(int i=0;i<9;++i)same&=back[i]==before[i];
   native->set(id("effect_slot"),1);native->set(id("ui_fx_move"),1);waitNative();int edge[9];chain(edge);bool still=true;for(int i=0;i<9;++i)still&=edge[i]==before[i];
   check(same&&native->getInt(id("effect_slot"))==1&&still,"LATER moves it back, and at the first place EARLIER does nothing");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   // An effect page: the reverb's twelve controls on one layer, the compressor's on two.
   const float mix=at("reverb_dry_wet"),decay=at("reverb_decay_time"),shelf=at("reverb_low_shelf_cutoff");
   native->set(id("ui_page"),30);native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_0"),.44f);native->setNormalised(id("ui_knob_1"),.61f);native->setNormalised(id("ui_knob_12"),.27f);native->setNormalised(id("ui_knob_6"),.9f);
   check(std::abs(at("reverb_dry_wet")-.44f)<1e-6&&std::abs(at("reverb_decay_time")-.61f)<1e-6&&std::abs(at("reverb_low_shelf_cutoff")-.27f)<1e-6&&native->displayText(id("ui_knob_6"))=="Unused",
       "reverb Q-Links, one knob bank per group: mix, decay, size, pre delay / chorus / input cuts / shelves; a knob its group does not need is unused");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   native->setNormalised(native->controlIndex("reverb_dry_wet"),mix);native->setNormalised(native->controlIndex("reverb_decay_time"),decay);native->setNormalised(native->controlIndex("reverb_low_shelf_cutoff"),shelf);
   // 0.3.37 (device: "why are the Q-Links labelled UI Control?"): a slot is called after the
   // control it reaches on the page on screen; a knob its group does not need is "Unused".
   {const std::string first=native->param(id("ui_knob_0")).name,second=native->param(id("ui_knob_1")).name,shelf=native->param(id("ui_knob_12")).name,unused=native->param(id("ui_knob_6")).name;
    if(first!="Mix"||unused!="Unused")std::cout<<"SLOT NAME DETAIL '"<<first<<"' '"<<second<<"' '"<<shelf<<"' '"<<unused<<"'"<<std::endl;
    check(first=="Mix"&&second.find("Decay")!=std::string::npos&&shelf.find("UI Control")==std::string::npos&&!shelf.empty()&&unused=="Unused"&&native->param(id("ui_knob_0")).id=="ui_knob_0",
        "on the reverb page the Q-Link slots are named after their controls, without the word the whole page shares");
    native->set(id("ui_page"),24);waitNative();const std::string place=native->param(id("ui_knob_2")).name;int order[9];chain(order);
    check(place==names[order[2]],"on the FX overview a slot is named after the effect at its place");
    AEffect* fresh=VSTPluginMain(displayHost);auto* idle=static_cast<Vitaleon*>(fresh->object);fresh->dispatcher(fresh,effOpen,0,0,nullptr,0);
    for(int n=0;n<200&&idle->busy();++n)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    check(idle->param(id("ui_knob_0")).name=="UI Control 1","before audio runs the slots keep their fixed names, as the build tools read them");
    fresh->dispatcher(fresh,effClose,0,0,nullptr,0);
    native->set(id("ui_page"),30);waitNative();}
   const float threshold=at("compressor_low_upper_threshold"),attack=at("compressor_attack");
   native->set(id("ui_page"),31);native->set(id("ui_section"),1);waitNative();native->setNormalised(id("ui_knob_0"),.33f);
   check(std::abs(at("compressor_low_upper_threshold")-.33f)<1e-6&&at("compressor_attack")==attack,"compressor, second layer: the thresholds and ratios of the three bands");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   native->setNormalised(native->controlIndex("compressor_low_upper_threshold"),threshold);
   native->set(id("ui_section"),0);native->set(id("ui_page"),0);waitNative();}
  // 0.3.35: an LFO on one page; 0.3.42: its controls in two pages. Frequency and shape lead.
  {auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
   bool one=true;for(int page=16;page<24;++page)one&=spec[page]["chart"]=="lfo"&&spec[page]["sections"].size()==2&&spec[page]["sections"][0]["items"].size()==11&&spec[page]["sections"][1]["items"].size()==10;
   check(one&&spec[16]["sections"][0]["items"][0]=="LFO frequency"&&spec[16]["sections"][0]["items"][1]=="LFO Shape"&&spec[16]["sections"][1]["items"][0]=="Point"&&spec[16]["sections"][1]["items"][7]==""&&spec[24]["chart"]=="effects",
       "each LFO shows its controls in two pages of eleven and nine; frequency and shape come first, the point editor is on PAGE 2");
   native->set(id("ui_page"),18);native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_0"),.36f);native->setNormalised(id("ui_knob_5"),.41f);native->setNormalised(id("ui_knob_7"),.28f);native->setNormalised(id("ui_knob_10"),.52f);
   const int shapeBefore=native->getInt(id("lfo_shape"));native->setNormalised(id("ui_knob_1"),shapeBefore==2?1.0f:.4f);
   check(std::abs(at("lfo_3_frequency")-.36f)<1e-6&&std::abs(at("lfo_3_phase")-.41f)<1e-6&&std::abs(at("lfo_3_delay_time")-.28f)<1e-6&&std::abs(at("lfo_3_smooth_time")-.52f)<1e-6&&native->getInt(id("lfo_shape"))!=shapeBefore
         &&native->displayText(id("ui_knob_11"))=="Unused",
       "LFO PAGE 1: frequency and shape with tempo and sync / mode, phase, stereo, delay / fade and smoothing");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   native->set(id("ui_section"),1);waitNative();
   const int pointsBefore=native->getInt(id("lfo_points"));
   native->setNormalised(id("ui_knob_9"),.8f);native->setNormalised(id("ui_knob_4"),.2f);
   const bool second=std::abs(at("lfo_3_keytrack_tune")-.8f)<1e-6&&native->getInt(id("lfo_points"))!=pointsBefore&&native->getInt(id("ui_bank"))==1&&std::abs(at("lfo_3_frequency")-.36f)<1e-6
         &&native->displayText(id("ui_knob_7"))=="Unused"&&native->displayText(id("ui_knob_10"))=="Unused";
   native->set(id("lfo_points"),float(pointsBefore));
   check(second,
       "LFO PAGE 2: the point editor / point count, curve smoothing and shape file / key tracking; PAGE 1 stays as it is");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   // A knob turned last in group 3, then a layer with two groups: the frame must not stay on a
   // group that layer does not have (device, 0.3.35: "Q-LINKS MORE is empty"). Both pages of an
   // LFO have three groups since 0.3.42, so this is checked on the compressor (three, then two).
   {const float high=at("compressor_high_upper_threshold");
    native->set(id("ui_page"),31);native->set(id("ui_section"),1);waitNative();native->setNormalised(id("ui_knob_8"),high>.5f?.3f:.7f);const int bankBefore=native->getInt(id("ui_bank"));
    for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    native->set(id("ui_section"),0);waitNative();
    check(bankBefore==2&&native->getInt(id("ui_bank"))==0,"the Q-Link frame moves to the first group when the new layer has nothing in the current one");
    native->setNormalised(native->controlIndex("compressor_high_upper_threshold"),high);
    native->set(id("ui_page"),18);native->set(id("ui_section"),1);waitNative();}
   native->setNormalised(native->controlIndex("lfo_3_keytrack_tune"),.5f);native->setNormalised(native->controlIndex("lfo_3_delay_time"),0);
   // Device, 0.3.35: "the shape knob is messed up, and many knobs make a glitchy sound when I move
   // them". The shape stays where it is put, reads Custom once a point is changed, and no curve
   // control stops the engine any more.
   {waitNative();const long passes=native->testServicePasses();
    native->set(id("lfo_shape"),2);waitNative();const bool sine=native->getInt(id("lfo_shape"))==2;
    native->set(id("lfo_shape"),4);waitNative();const bool saw=native->getInt(id("lfo_shape"))==4;
    native->set(id("lfo_shape"),1);waitNative();const bool triangle=native->getInt(id("lfo_shape"))==1;
    check(sine&&saw&&triangle,"the LFO shape dial stays on the shape it was turned to");
    native->set(id("lfo_point"),2);waitNative();native->set(id("lfo_point_y"),.35f);waitNative();
    check(native->getInt(id("lfo_shape"))==0,"once a point is changed the shape reads Custom");
    native->set(id("lfo_points"),5);waitNative();native->set(id("lfo_point_power"),3);waitNative();native->set(id("lfo_curve_smooth"),1);waitNative();
    check(native->testServicePasses()==passes,"shape, points and the point controls are applied without stopping the engine");
    native->set(id("lfo_curve_smooth"),0);waitNative();native->set(id("lfo_shape"),1);waitNative();
    native->set(id("ui_page"),19);waitNative();const bool other=native->getInt(id("lfo_shape"))==1;
    native->set(id("ui_page"),18);waitNative();
    check(other&&native->getInt(id("lfo_shape"))==1,"the dial shows the shape of the LFO on screen");}
   native->set(id("ui_section"),0);native->set(id("ui_page"),0);waitNative();}
  // 0.3.44: SAVE SOUND writes the sound that is playing as a .vital file into <drive>/Vitaleon/Sounds
  // and lists it in the bank "My sounds".
  {namespace fs=std::filesystem;
   auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
   auto text=[&](const char* key){return native->displayText(id(key));};
   auto jobs=[&]{for(int i=0;i<8000&&native->busy();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}waitNative();};
   auto key=[&](int code){native->set(id("ui_key"),float(code));waitNative();};
   auto status=[&]{return text("save_status_0")+text("save_status_1")+text("save_status_2");};
   auto named=[&](int parameter,const char* name){const auto& all=native->param(parameter).choices;for(int i=0;i<int(all.size());++i)if(all[size_t(i)]==name)return i;return -1;};
   int page=-1;for(int n=0;n<int(spec.size());++n)if(spec[n]["chart"].is_string()&&spec[n]["chart"]=="savesound")page=n;
   check(page==41&&spec[page]["hidden"]==true&&spec[page]["group"]==0,"the SAVE SOUND page is a hidden view of HOME and kept its number when later pages were appended");
   const fs::path folder(Vitaleon::userFolder("Sounds")),file=folder/"Test 1.vital";
   check(folder.parent_path().filename()=="Vitaleon"&&folder.string().find("test-content")==std::string::npos,"saved sounds go to a Vitaleon folder beside the plugin's own folder, not into it");
   // Make the sound recognisable: two controls and a route.
   const int source=id("route_5_source"),target=id("route_5_destination"),lfo2=named(source,"lfo_2"),pan=named(target,"osc_1_pan");
   native->setNormalised(native->controlIndex("filter_1_cutoff"),.37f);native->setNormalised(native->controlIndex("osc_2_level"),.61f);
   native->set(source,float(lfo2));native->set(target,float(pan));native->setNormalised(native->controlIndex("modulation_5_amount"),.8f);waitNative();
   native->set(id("ui_page"),0);waitNative();native->set(id("ui_save"),1);waitNative();
   check(native->getInt(id("ui_page"))==page&&native->getInt(id("ui_save"))==0&&!text("save_name").empty()&&text("save_name").back()=='_'&&status().find("Type a name")!=std::string::npos,
       "SAVE SOUND on HOME opens the name entry with the name of the loaded sound and re-arms itself");
   key(40);native->set(id("ui_save"),2);jobs();
   check(text("save_name")=="_"&&status()=="Type a name first."&&native->getInt(id("ui_page"))==page,"CLEAR empties the name, and a sound is not saved without one");
   for(int code:{20,5,19,20,37,37,28})key(code);
   const bool typed=text("save_name")=="Test 2_";key(39);key(27);
   check(typed&&text("save_name")=="Test 1_"&&native->getInt(id("ui_key"))==0,"the keys write the name: a capital at the start of a word, one space at most, DEL takes the last character");
   native->set(id("save_caps"),1);key(39);key(39);key(24);key(27);const bool caps=text("save_name")=="TestX1_";native->set(id("save_caps"),0);key(39);key(39);key(37);key(27);
   check(caps&&text("save_name")=="Test 1_","with CAPS on a letter is a capital anywhere in a word");
   native->set(id("ui_save"),2);jobs();
   nlohmann::json saved;{std::ifstream in(file,std::ios::binary);if(in)saved=nlohmann::json::parse(in,nullptr,false);}
   const bool written=saved.is_object()&&saved.value("preset_name",std::string())=="Test 1"&&saved.value("synth_version",std::string())=="1.0.6"&&saved.count("settings")&&saved["settings"].is_object();
   check(written&&native->getInt(id("ui_page"))==0&&text("library_loaded_0")=="Test 1"&&text("active_bank_0")=="My sounds",
       "SAVE writes <name>.vital, returns to HOME under the new name, and PRESET lists the bank My sounds");
   const auto& settings=saved["settings"];
   check(written&&settings.value("modulations",nlohmann::json::array()).size()==64&&settings["modulations"][4].value("source",std::string())=="lfo_2"&&settings["modulations"][4].value("destination",std::string())=="osc_1_pan"
         &&settings["modulations"][4].count("line_mapping")&&settings["modulations"][0].value("source",std::string("x")).empty()
         &&settings.value("wavetables",nlohmann::json::array()).size()==3&&settings.value("lfos",nlohmann::json::array()).size()==8&&settings.count("sample")&&settings.count("polyphony")
         &&settings.count("filter_1_cutoff")&&settings.count("osc_3_unison_voices")&&settings.count("reverb_dry_wet")&&saved.count("macro1")&&saved.count("author"),
       "the file is a Vital preset: every control, the 64 routes with their curves, eight LFO shapes, three wavetables, the sample, the macro names");
   {char name[256]{};effect->dispatcher(effect,effGetProgramName,0,0,name,0);
    check(std::string(name)=="Test 1","the saved sound is the program that is selected in PRESET");}
   // Change the sound, then load the file again: everything comes back.
   const float cutoff=at("filter_1_cutoff"),level=at("osc_2_level"),amount=at("modulation_5_amount");
   native->setNormalised(native->controlIndex("filter_1_cutoff"),.9f);native->setNormalised(native->controlIndex("osc_2_level"),.1f);native->set(source,0);native->set(target,0);waitNative();
   int program=-1;for(int n=0;n<effect->numPrograms;++n){char name[256]{};effect->dispatcher(effect,effGetProgramNameIndexed,n,0,name,0);if(std::string(name)=="Test 1")program=n;}
   effect->dispatcher(effect,effSetProgram,0,program,nullptr,0);jobs();
   check(program>=0&&std::abs(at("filter_1_cutoff")-cutoff)<1e-5&&std::abs(cutoff-.37f)<1e-5&&std::abs(at("osc_2_level")-level)<1e-5&&std::abs(at("modulation_5_amount")-amount)<1e-5
         &&native->getInt(source)==lfo2&&native->getInt(target)==pan&&text("library_loaded_0")=="Test 1",
       "a saved sound loads again exactly as it was saved: controls, route and amount");
   // The same name again: asked once, then replaced.
   const auto firstSize=fs::file_size(file);
   native->setNormalised(native->controlIndex("filter_1_cutoff"),.52f);
   native->set(id("ui_save"),1);waitNative();const bool offered=text("save_name")=="Test 1_";
   native->set(id("ui_save"),2);jobs();
   nlohmann::json kept;{std::ifstream in(file,std::ios::binary);if(in)kept=nlohmann::json::parse(in,nullptr,false);}
   const bool asked=status().find("already in My sounds")!=std::string::npos&&native->getInt(id("ui_page"))==page&&kept.is_object()&&kept["settings"]["filter_1_cutoff"]==saved["settings"]["filter_1_cutoff"];
   native->set(id("ui_save"),2);jobs();
   nlohmann::json replaced;{std::ifstream in(file,std::ios::binary);if(in)replaced=nlohmann::json::parse(in,nullptr,false);}
   check(offered&&asked&&firstSize>0&&replaced.is_object()&&replaced["settings"]["filter_1_cutoff"]!=saved["settings"]["filter_1_cutoff"]&&native->getInt(id("ui_page"))==0&&!fs::exists(fs::path(file.string()+".part")),
       "a name that exists is asked about once; SAVE again replaces the file");
   // The project remembers the saved sound and its bank.
   {std::vector<uint8_t> chunk;void* data=nullptr;const auto size=effect->dispatcher(effect,effGetChunk,0,0,&data,0);chunk.assign(static_cast<uint8_t*>(data),static_cast<uint8_t*>(data)+size);
    const std::string all(chunk.begin(),chunk.end());
    check(all.find("Test 1.vital")!=std::string::npos&&all.find("\"bank_path\"")!=std::string::npos&&all.find("Sounds")!=std::string::npos,"the project stores the path of the saved sound and of its folder");}
   // 0.3.45: the library shows the folder of the saved sounds, and EXPORT copies them to another drive.
   {auto libraryStatus=[&]{std::string all;for(int i=0;i<4;++i)all+=text(("storage_status_"+std::to_string(i)).c_str());return all;};
    auto step=[&](int which){native->set(id("library_step"),float(which));jobs();};
    native->set(id("storage_command_0"),1);jobs();
    bool listed=false;for(int row=0;row<6;++row)if(text(("storage_row_"+std::to_string(row*2+1)).c_str())=="My sounds"&&text(("storage_row_"+std::to_string(row*2)).c_str())=="FOLDER")listed=true;
    step(2);const bool needsDrive=libraryStatus().find("Open a USB stick or SD card first")!=std::string::npos;
    step(1);
    check(listed&&needsDrive&&native->getInt(id("library_step"))==0&&text("storage_row_0")=="SOUND"&&text("storage_row_1")=="Test 1"&&native->getNormalised(id("storage_available_0"))==1.0f
          &&text("storage_path_0").find("Sounds")!=std::string::npos&&libraryStatus().find("My sounds")!=std::string::npos,
        "the library lists My sounds among the drives, and MY SOUNDS opens the folder with a LOAD row for each saved sound");
    step(2);
    check(libraryStatus().find("Open a USB stick or SD card first")!=std::string::npos,"EXPORT inside My sounds asks for a drive: the sounds are not copied onto themselves");
    const fs::path stick=fs::path(Vitaleon::userFolder("Sounds")).parent_path().parent_path()/"export-drive";std::error_code gone;fs::remove_all(stick,gone);fs::create_directories(stick);
    const std::string said=native->exportSounds(stick.string());const fs::path copy=stick/"Vitaleon"/"Sounds"/"Test 1.vital";
    nlohmann::json exported;{std::ifstream in(copy,std::ios::binary);if(in)exported=nlohmann::json::parse(in,nullptr,false);}
    check(said.find("Exported 1 sound to export-drive/Vitaleon/Sounds.")!=std::string::npos&&exported.is_object()&&exported==replaced&&fs::file_size(copy)==fs::file_size(file)&&!fs::exists(fs::path(copy.string()+".part")),
        "EXPORT copies every saved sound to Vitaleon/Sounds on the open drive, byte for byte");
    {std::ofstream second(folder/"Another.vital",std::ios::binary);second<<replaced.dump();}
    {std::ofstream stale(copy,std::ios::binary|std::ios::trunc);stale<<"old";}
    const std::string again=native->exportSounds(stick.string());
    check(again.find("Exported 2 sounds")!=std::string::npos&&fs::file_size(copy)==fs::file_size(file)&&fs::exists(stick/"Vitaleon"/"Sounds"/"Another.vital"),"a second EXPORT adds new sounds and replaces a file of the same name");
    const std::string same=native->exportSounds(fs::path(Vitaleon::userFolder("Sounds")).parent_path().parent_path().string());
    const std::string inPlugin=native->exportSounds((fs::path(argv[3])/"Content").string());
    check(same.find("on this drive already")!=std::string::npos&&inPlugin.find("Open a USB stick or SD card first")!=std::string::npos&&native->exportSounds("").find("Open a USB stick")!=std::string::npos,
        "EXPORT refuses the drive the sounds are on, the plugin's own folder and the list of drives");
    fs::remove(folder/"Another.vital",gone);fs::remove_all(stick,gone);native->set(id("storage_command_0"),1);jobs();}
   native->setNormalised(native->controlIndex("filter_1_cutoff"),cutoff);native->set(source,0);native->set(target,0);native->set(id("ui_page"),0);waitNative();}
  // 0.3.50: INIT SOUND on HOME. The first tap only turns the switch on, the second tap resets
  // the sound and nothing else; a switch that is left alone goes back by itself.
  {auto text=[&](const char* key){return native->displayText(id(key));};
   auto jobs=[&]{for(int i=0;i<8000&&native->busy();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}waitNative();};
   auto message=[&]{std::string all;for(int i=0;i<4;++i)all+=text(("display_load_text_"+std::to_string(i)).c_str());return all;};
   auto named=[&](int parameter,const char* name){const auto& all=native->param(parameter).choices;for(int i=0;i<int(all.size());++i)if(all[size_t(i)]==name)return i;return -1;};
   const int init=id("ui_init"),cut=native->controlIndex("filter_1_cutoff"),level=native->controlIndex("osc_2_level"),amount=native->controlIndex("modulation_5_amount");
   const int source=id("route_5_source"),target=id("route_5_destination");
   check(init==3283&&native->numParams()==3392&&!native->param(init).automatable,"INIT SOUND has its own parameter, appended after every older one");
   native->setNormalised(cut,.37f);native->setNormalised(level,.61f);native->set(source,float(named(source,"lfo_2")));native->set(target,float(named(target,"osc_1_pan")));native->setNormalised(amount,.8f);
   native->set(id("ui_page"),4);waitNative();
   const int page=native->getInt(id("ui_page"));const std::string loaded=text("library_loaded_0"),bank=text("active_bank_0");const float edited=native->get(cut);
   native->set(init,1);jobs();
   check(native->getInt(init)==1&&native->get(cut)==edited&&text("library_loaded_0")==loaded&&native->getInt(source)!=0&&message().find("Tap CONFIRM INIT")==0,
       "the first tap on INIT SOUND changes nothing in the sound and asks for the second tap");
   native->set(init,0);jobs();
   if(text("library_loaded_0")!="Init"||native->get(cut)!=native->param(cut).def)std::cout<<"INIT DETAIL name="<<text("library_loaded_0")<<" cutoff="<<native->get(cut)<<" def="<<native->param(cut).def<<" message="<<message()<<std::endl;
   check(text("library_loaded_0")=="Init"&&native->get(cut)==native->param(cut).def&&native->get(level)==native->param(level).def&&native->get(amount)==native->param(amount).def
         &&native->getInt(source)==0&&native->getInt(target)==0&&text("display_sound_info_0").empty()&&message()=="Init sound.",
       "the second tap resets the controls, the routes and the texts of the sound to Init");
   check(native->getInt(init)==0&&native->getInt(id("ui_page"))==page&&page!=0&&text("active_bank_0")==bank&&loaded!="Init","INIT SOUND leaves the page and the bank in PRESET as they are");
   // Left alone: the switch goes back and the sound is untouched; a tap after that is a first tap again.
   Vitaleon::initConfirmMs_.store(60);native->setNormalised(cut,.5f);waitNative();const float kept=native->get(cut);
   native->set(init,1);waitNative();const bool asked=message().find("Tap CONFIRM INIT")==0;
   for(int i=0;i<400&&native->getInt(init)!=0;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   jobs();
   const bool back=native->getInt(init)==0&&native->get(cut)==kept&&message()=="Init sound.";
   native->set(init,0);jobs();
   check(asked&&back&&native->get(cut)==kept,"a switch that is not confirmed goes back by itself, puts the old message back and resets nothing");
   Vitaleon::initConfirmMs_.store(5000);
   native->set(id("ui_page"),0);waitNative();}
  // 0.3.56: a page change is announced to the host once, and that one announcement already
  // carries the picture of the new page. (Device, 0.3.55: a page with a picture was drawn twice,
  // the second time when its picture had been made, and the MPC needs 0.8 s for each.)
  {auto run=[&](int ms){for(int i=0;i<ms;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
   native->set(id("ui_page"),0);waitNative();run(600);
   // Change what the pictures of other pages show while those pages are not on screen.
   const int cut=native->controlIndex("filter_1_cutoff"),frame=native->controlIndex("osc_1_wave_frame");
   native->setNormalised(cut,native->getNormalised(cut)>.5f?.21f:.83f);native->setNormalised(frame,native->getNormalised(frame)>.5f?.1f:.9f);run(600);
   auto turn=[&](int page,const char* first,int count,bool& complete,bool& moved){
       watchFirst.store(id(first));watchCount.store(count);const uint64_t old=watchHash(native);const int before=displayUpdates.load();
       native->set(id("ui_page"),float(page));run(900);
       const int updates=displayUpdates.load()-before;const uint64_t now=watchHash(native);complete=watchSeen.load()==now;moved=now!=old;watchFirst.store(-1);return updates;};
   bool filterDone=false,filterMoved=false,waveDone=false,waveMoved=false,homeDone=false,homeMoved=false;
   const int filters=turn(8,"display_filter_0_0",144,filterDone,filterMoved),overview=turn(40,"display_wave_0_0",96,waveDone,waveMoved),home=turn(0,"display_wave_0_0",96,homeDone,homeMoved);
   std::cout<<"page_redraws filters="<<filters<<" picture_in_it="<<filterDone<<" picture_new="<<filterMoved<<" overview="<<overview<<" picture_in_it="<<waveDone<<" picture_new="<<waveMoved<<" home="<<home<<std::endl;
   check(filters==1&&overview==1&&home==1,"a page change is announced to the host once, also when the picture of the page had to be made anew");
   check(filterMoved&&filterDone&&waveMoved&&waveDone,"and that one announcement already carries the new picture: the filter curve after a change of cutoff, the wave after a change of frame");
   // A picture that changes while its page is on screen is still announced.
   {native->set(id("ui_page"),8);run(900);const int before=displayUpdates.load();native->setNormalised(cut,native->getNormalised(cut)>.5f?.3f:.7f);run(900);
    check(displayUpdates.load()>before,"a picture that changes while its page is on screen is announced as before");}
   native->set(id("ui_page"),0);waitNative();}
  // 0.3.47: no click when the sound changes while it plays. As on the device, one thread renders
  // audio without a pause while another selects a program. Up to 0.3.46 the output was cut to
  // zero at the edge of a block and started again in the middle of a wave.
  {Vitaleon::parkFadeWaitMs_.store(80);
   std::vector<float> stream(900000,0.0f);std::atomic<size_t> written{0};std::atomic<bool> run{true};float left[128]{},right[128]{};float* audio[]={left,right};
   // A host hands MIDI over on its audio thread, between two blocks. While the renderer thread
   // runs, a note is therefore queued for it (0.3.52: sent from this thread, a note that arrived
   // while a block was being rendered was lost now and then, and the check failed for no fault
   // of the plugin).
   std::atomic<int> queued{0};std::atomic<bool> rendering{false};
   auto send=[&](int status){VstMidiEvent note{};note.type=kVstMidiType;note.byteSize=sizeof(note);note.midiData[0]=char(status);note.midiData[1]=45;note.midiData[2]=110;
       VstEvents events{};events.numEvents=1;events.events[0]=reinterpret_cast<VstEvent*>(&note);dispatch(effProcessEvents,0,0,&events);};
   auto midi=[&](int status){if(!rendering.load()){send(status);return;}
       queued.store(status);for(int i=0;i<4000&&queued.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));};
   effect->dispatcher(effect,effSetProgram,0,0,nullptr,0);waitNative();
   native->setNormalised(native->controlIndex("env_1_sustain"),1.0f);waitNative();
   midi(0x90);
   std::thread renderer([&]{rendering.store(true);while(run.load()){if(const int status=queued.exchange(0))send(status);effect->processReplacing(effect,nullptr,audio,128);const size_t at=written.load();
       if(at+128<=stream.size()){std::copy(left,left+128,stream.begin()+long(at));written.store(at+128);}
       std::this_thread::sleep_for(std::chrono::microseconds(1200));}});
   auto settle=[&](int ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));};
   auto idle=[&]{for(int i=0;i<4000&&native->busy();++i)settle(2);};
   settle(300);const size_t first=written.load();
   // Another program than the one that is playing.
   auto title=[&](int n){char name[256]{};effect->dispatcher(effect,effGetProgramNameIndexed,n,0,name,0);return std::string(name);};
   int other=0;for(int n=effect->numPrograms-1;n>0;--n)if(title(n)!=title(0)&&title(n)!="[Current sound]"){other=n;break;}
   effect->dispatcher(effect,effSetProgram,0,other,nullptr,0);settle(30);idle();settle(200);const size_t changed=written.load();
   midi(0x90);settle(350);const size_t again=written.load();   // a new note on the new sound
   {void* data=nullptr;effect->dispatcher(effect,effGetChunk,0,0,&data,0);}   // a state read takes the engine too, the note goes on
   settle(300);
   run.store(false);renderer.join();rendering.store(false);const size_t total=written.load();
   auto level=[&](size_t from,size_t to){float peak=0;for(size_t i=from;i<to;++i)peak=std::max(peak,std::abs(stream[i]));return peak;};
   auto step=[&](size_t from,size_t to){float most=0;for(size_t i=std::max<size_t>(from,1);i<to;++i)most=std::max(most,std::abs(stream[i]-stream[i-1]));return most;};
   const float before=level(first>8192?first-8192:0,first);
   // Where the old sound ends: the first run of silence after the change was asked for.
   size_t stop=0;for(size_t i=first;i+64<changed&&!stop;++i){bool silent=true;for(size_t k=0;k<64&&silent;++k)silent=stream[i+k]==0.0f;if(silent)stop=i;}
   const float edge=stop?std::abs(stream[stop-1]):1.0f,leftover=stop?level(stop,changed):1.0f;
   const float after=level(again>8192?again-8192:changed,again),ordinary=step(again>8192?again-8192:changed,again),reading=step(again,total);
   std::cout<<"preset_change level="<<before<<" stop_after="<<(stop?stop-first:0)<<" edge="<<edge<<" leftover="<<leftover<<" new_level="<<after<<" step_playing="<<ordinary<<" step_reading="<<reading<<" other="<<other<<std::endl;
   check(before>.01f&&stop>first&&edge<before*.02f,"a sound that is replaced fades out: its last sample before the silence is next to nothing");
   check(leftover==0.0f,"nothing of the old sound is played after the change (up to 0.3.46: one leftover block of it, cut off)");
   check(after>.01f&&reading<=ordinary*1.5f+after*.02f,"a state read while a note sounds makes no step in the output larger than the sound itself has");
   midi(0x80);Vitaleon::parkFadeWaitMs_.store(0);waitNative();
   native->set(id("ui_page"),0);waitNative();}
  // 0.3.33: an envelope on one page, nine dials on the Q-Links.
  {auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
   bool one=true;for(int page=10;page<16;++page)one&=spec[page]["chart"]=="env"&&spec[page]["sections"].size()==1&&spec[page]["sections"][0]["items"].size()==9;
   check(one&&spec[16]["tab"]=="LFO 1","each envelope has one page without sections, nine controls on the Q-Links");
   native->set(id("ui_page"),12);native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_0"),.31f);native->setNormalised(id("ui_knob_3"),.47f);native->setNormalised(id("ui_knob_4"),.22f);native->setNormalised(id("ui_knob_5"),.18f);native->setNormalised(id("ui_knob_8"),.7f);
   check(std::abs(at("env_3_attack")-.31f)<1e-6&&std::abs(at("env_3_release")-.47f)<1e-6&&std::abs(at("env_3_delay")-.22f)<1e-6&&std::abs(at("env_3_hold")-.18f)<1e-6&&std::abs(at("env_3_release_power")-.7f)<1e-6,
       "envelope Q-Links: attack, decay, sustain, release / delay, hold, attack curve, decay curve / release curve");
   for(int i=0;i<1000&&!native->testSlotsAtRest();++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   for(const char* key:{"env_3_delay","env_3_hold"})native->setNormalised(native->controlIndex(key),0);
   // 0.3.34: the plugin draws the envelope itself and it always fills the picture.
   {auto shape=[&](int c){return native->getNormalised(id("display_env_shape_0")+c);};
    auto run=[&]{for(int i=0;i<200;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
    auto norm=[&](const char* key,float value){native->setNormalised(native->controlIndex(key),value);};
    norm("env_3_delay",0);norm("env_3_attack",.1f);norm("env_3_hold",0);norm("env_3_decay",.4f);native->set(native->controlIndex("env_3_sustain"),.5f);norm("env_3_release",.3f);
    for(const char* key:{"env_3_attack_power","env_3_decay_power","env_3_release_power"})native->set(native->controlIndex(key),0);
    run();
    // Shares of the width: attack 0.1, decay 0.4, held level 0.2, release 0.3.
    const bool drawn=shape(2)<.2f&&shape(31)>.9f&&shape(93)<.8f&&shape(93)>.6f&&std::abs(shape(190)-.5f)<.03f&&shape(265)<.3f&&shape(265)>.1f&&shape(311)<.03f;
    if(!drawn)std::cout<<"ENVELOPE PICTURE DETAIL "<<shape(2)<<" "<<shape(31)<<" "<<shape(93)<<" "<<shape(190)<<" "<<shape(265)<<" "<<shape(311)<<std::endl;
    check(drawn,"the envelope picture rises, falls to the held level and releases, across the whole width");
    // Much shorter times: the picture is still filled, only the shares change.
    norm("env_3_attack",.02f);norm("env_3_decay",.08f);norm("env_3_release",.06f);run();
    check(shape(31)>.9f&&std::abs(shape(190)-.5f)<.03f&&shape(265)>.1f&&shape(311)<.03f,"short times do not shrink the picture: the stages share the width in proportion");
    native->set(native->controlIndex("env_3_decay_power"),-8);run();const float bent=shape(93);
    check(bent<.6f,"the decay curve bends the picture as it bends the sound");
    native->set(native->controlIndex("env_3_decay_power"),0);native->set(native->controlIndex("env_3_sustain"),1);
    // SHAPE / DRAG is a plain switch of the page; it changes no sound parameter and asks for a redraw.
    const float attackBefore=native->getNormalised(native->controlIndex("env_3_attack"));const unsigned redraws=displayUpdates.load();
    native->set(id("ui_env_view"),1);for(int i=0;i<100;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(native->getInt(id("ui_env_view"))==1&&native->getInt(id("ui_page"))==12&&displayUpdates.load()>redraws&&native->getNormalised(native->controlIndex("env_3_attack"))==attackBefore,
        "DRAG switches the envelope page to the draggable picture and leaves the sound alone");
    native->set(id("ui_env_view"),0);}
   native->set(id("ui_page"),0);waitNative();}
  // 0.3.30: both filters on one page, a measured picture each; 0.3.46: two pages of controls.
  {auto at=[&](const char* key){return native->getNormalised(native->controlIndex(key));};
   auto put=[&](const char* key,float plain){native->set(native->controlIndex(key),plain);};
   check(spec[8]["chart"]=="filters"&&spec[8]["sections"].size()==2&&spec[8]["sections"][0]["items"].size()==16&&spec[8]["sections"][1]["items"].size()==16
       &&spec[9]["hidden"]==true&&spec[9]["chart"]=="retired"&&spec[10]["tab"]=="ENV 1",
       "both filters share one page with two pages of controls of sixteen; the old FILTER 2 page keeps its number");
   check(spec[8]["sections"][0]["items"][2]=="filter_1_model"&&spec[8]["sections"][0]["items"][3]=="filter_1_style"&&spec[8]["sections"][0]["items"][10]=="filter_2_model"&&spec[8]["sections"][1]["items"][7]=="filter_1_on",
       "PAGE 1 of the filters leads with cutoff, resonance, model and style; the on switch is the last knob of PAGE 2");
   native->set(id("ui_page"),9);waitNative();
   check(native->getInt(id("ui_page"))==8&&native->getInt(id("ui_group"))==2,"a project saved on the old FILTER 2 page opens the page with both filters");
   native->set(id("ui_page"),8);native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_0"),.41f);native->setNormalised(id("ui_knob_1"),.22f);native->setNormalised(id("ui_knob_7"),.7f);native->setNormalised(id("ui_knob_8"),.63f);native->setNormalised(id("ui_knob_13"),.35f);
   check(std::abs(at("filter_1_cutoff")-.41f)<1e-6&&std::abs(at("filter_1_resonance")-.22f)<1e-6&&std::abs(at("filter_1_keytrack")-.7f)<1e-6&&std::abs(at("filter_2_cutoff")-.63f)<1e-6&&std::abs(at("filter_2_mix")-.35f)<1e-6,
       "FILTERS PAGE 1: knob groups 1 and 2 reach filter 1, groups 3 and 4 reach filter 2");
   float model=at("filter_2_model");native->setNormalised(id("ui_knob_10"),model>.5f?0.0f:1.0f);
   check(at("filter_2_model")!=model,"the model list of filter 2 follows its Q-Link");
   put("filter_2_model",0);
   native->set(id("ui_section"),1);waitNative();
   native->setNormalised(id("ui_knob_1"),.27f);native->setNormalised(id("ui_knob_7"),1.0f);native->setNormalised(id("ui_knob_12"),.58f);native->setNormalised(id("ui_knob_14"),1.0f);
   check(std::abs(at("filter_1_formant_x")-.27f)<1e-6&&at("filter_1_on")==1&&std::abs(at("filter_2_formant_resonance")-.58f)<1e-6&&at("filter_2_filter_input")==1&&std::abs(at("filter_1_cutoff")-.41f)<1e-6,
       "FILTERS PAGE 2: the knobs reach the formant controls, the input switch and the on switch, and leave the main ones alone");
   native->setNormalised(id("ui_knob_7"),0.0f);native->setNormalised(id("ui_knob_14"),0.0f);
   native->set(id("ui_section"),0);waitNative();
   // The pictures are measured from a private copy of the engine's filter, so they follow
   // model, cutoff and blend. Column c stands for 20 Hz * 1000^((c+0.5)/144); the value is the
   // level between -60 dB (0) and +24 dB (1).
   auto picture=[&](int f,int column){return native->getNormalised(id("display_filter_"+std::to_string(f)+"_0")+column);};
   auto settlePicture=[&]{for(int i=0;i<250;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
   put("filter_1_model",0);put("filter_1_style",0);put("filter_1_blend",0);put("filter_1_resonance",.5f);put("filter_1_drive",0);put("filter_1_cutoff",60);
   put("filter_2_model",0);put("filter_2_style",0);put("filter_2_blend",2);put("filter_2_resonance",.5f);put("filter_2_drive",0);put("filter_2_cutoff",108);
   settlePicture();
   if(!(picture(0,10)>.5f&&picture(0,120)<.15f&&picture(1,10)<.15f&&picture(1,138)>.5f))std::cout<<"FILTER PICTURE DETAIL "<<picture(0,10)<<" "<<picture(0,120)<<" "<<picture(1,10)<<" "<<picture(1,138)<<" pictures "<<native->testFilterPictures()<<std::endl;
   check(native->testFilterPictures()>=2&&picture(0,10)>.5f&&picture(0,120)<.15f,"filter 1 as a low-pass at 262 Hz: its picture is high at 33 Hz and at the floor at 6.5 kHz");
   check(picture(1,10)<.15f&&picture(1,138)>.5f,"filter 2 as a high-pass at 4.2 kHz: its picture is at the floor at 33 Hz and high at 15 kHz");
   put("filter_1_cutoff",108);settlePicture();
   check(picture(0,90)>.5f,"the picture follows the cutoff: at 4.2 kHz the low-pass lets 1.5 kHz through");
   put("filter_1_model",6);put("filter_1_cutoff",72);put("filter_1_resonance",.9f);settlePicture();int ripples=0;for(int c=40;c<143;++c)ripples+=(picture(0,c)-picture(0,c-1))*(picture(0,c+1)-picture(0,c))<0;
   if(ripples<4){std::cout<<"COMB PICTURE DETAIL "<<ripples<<":";for(int c=0;c<144;c+=3)std::cout<<" "<<int(picture(0,c)*84-60);std::cout<<std::endl;}
   put("filter_1_model",0);put("filter_1_resonance",.5f);
   check(ripples>=4,"the comb model shows its teeth: the picture is measured, not drawn from a formula for one model");
   // While a control keeps moving nothing is measured and nothing is redrawn (device, 0.3.30:
   // a picture ten times a second made every control on the page stutter).
   {settlePicture();const long start=native->testFilterPictures();
    for(int step=0;step<40;++step){put("filter_1_cutoff",60.0f+step);effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(20));}
    const long during=native->testFilterPictures()-start;settlePicture();
    check(during==0&&native->testFilterPictures()==start+1&&picture(0,60)>.5f,"a filter picture is measured once the control rests, not while it turns");
    put("filter_1_cutoff",108);settlePicture();}
   // Off the page nothing is measured; the picture is brought up to date when the page opens.
   native->set(id("ui_page"),0);waitNative();settlePicture();const long before=native->testFilterPictures();
   put("filter_1_cutoff",50);settlePicture();const bool idle=native->testFilterPictures()==before;
   native->set(id("ui_page"),8);waitNative();settlePicture();
   check(idle&&native->testFilterPictures()>before&&picture(0,100)<.2f,"no picture is measured while another page is on screen; it is brought up to date when the page opens");
   for(const char* key:{"filter_1_cutoff","filter_2_cutoff"})put(key,60);put("filter_2_blend",0);put("filter_1_resonance",.5f);put("filter_1_keytrack",0);put("filter_2_mix",1);put("filter_1_formant_x",.5f);put("filter_2_formant_resonance",.85f);
   native->set(id("ui_page"),0);waitNative();}
  // 0.3.26: the OSC overview shows all three oscillators and the sample on one page.
  {int overview=-1;for(int page=0;page<int(spec.size());++page)if(spec[page]["chart"].is_string()&&spec[page]["chart"]=="oscmix")overview=page;
   check(overview==40&&spec[4]["tab"]=="OSC 1","the OSC overview is appended, so no earlier page changed its number");
   native->set(id("ui_page"),float(overview));native->set(id("ui_section"),0);waitNative();
   native->setNormalised(id("ui_knob_0"),.31f);native->setNormalised(id("ui_knob_3"),.44f);native->setNormalised(id("ui_knob_7"),.82f);native->setNormalised(id("ui_knob_10"),.25f);
   check(std::abs(native->getNormalised(native->controlIndex("osc_1_level"))-.31f)<1e-6&&std::abs(native->getNormalised(native->controlIndex("sample_level"))-.44f)<1e-6
       &&std::abs(native->getNormalised(native->controlIndex("sample_pan"))-.82f)<1e-6&&std::abs(native->getNormalised(native->controlIndex("osc_3_transpose"))-.25f)<.02,
       "the overview's Q-Links reach level, pan and pitch of the oscillators and the sample");
   {native->testInit();waitNative();int filled=0;for(int c=0;c<128;++c)filled+=native->getNormalised(id("display_sample_0")+c)>.2f;
    check(filled>100,"the built-in noise sample fills its picture");native->set(id("ui_page"),float(overview));waitNative();}
   // On the overview the wave pictures of all three oscillators follow their frames.
   auto column=[&](int osc,int c){return native->getNormalised(id("display_wave_"+std::to_string(osc)+"_0")+c);};
   auto settleGraph=[&]{for(int i=0;i<150;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
   bool follows=true;
   for(int osc=0;osc<3;++osc){int frame=native->controlIndex("osc_"+std::to_string(osc+1)+"_wave_frame");native->set(frame,20.0f);settleGraph();
       std::array<float,96> first{};for(int c=0;c<96;++c)first[size_t(c)]=column(osc,c);
       native->set(frame,170.0f);settleGraph();int moved=0;for(int c=0;c<96;++c)moved+=column(osc,c)!=first[size_t(c)];
       if(moved<=8)std::cout<<"OVERVIEW GRAPH DETAIL osc "<<osc+1<<" moved "<<moved<<std::endl;
       follows&=moved>8;}
   check(follows,"on the overview the wave pictures of all three oscillators follow their frames");
   // 0.3.32: the LFO curve in 384 columns. After Init every LFO is a triangle: low at both ends, high in the middle.
   {native->set(id("ui_page"),16);native->set(id("ui_section"),0);waitNative();
    for(int i=0;i<200;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    auto curve=[&](int c){return native->getNormalised(c<96?id("display_lfo_curve_0")+c:c<192?id("display_lfo_bank_0")+c-96:id("display_lfo_fine_0")+c-192);};
    if(!(curve(5)<.2f&&curve(100)>.4f&&curve(191)>.9f&&curve(193)>.9f&&curve(290)<.6f&&curve(378)<.2f))std::cout<<"CURVE DETAIL "<<curve(5)<<" "<<curve(100)<<" "<<curve(191)<<" "<<curve(193)<<" "<<curve(290)<<" "<<curve(378)<<std::endl;
    check(curve(5)<.2f&&curve(100)>.4f&&curve(191)>.9f&&curve(193)>.9f&&curve(290)<.6f&&curve(378)<.2f,"the LFO curve is written in 384 columns across all three readout blocks");
    native->set(id("ui_page"),float(overview));native->set(id("ui_section"),0);waitNative();}
   // Device, 0.3.30: a redraw while a control turns makes it stutter. The picture waits.
   {auto run=[&](int ms){for(int i=0;i<ms/2;++i){effect->processReplacing(effect,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
    const int frame=native->controlIndex("osc_1_wave_frame");native->set(frame,10.0f);run(700);
    const long rested=native->testVisualUpdates();
    for(int step=0;step<40;++step){native->set(frame,20.0f+step*4);run(20);}
    const long during=native->testVisualUpdates()-rested;run(700);
    if(during!=0||native->testVisualUpdates()<=rested)std::cout<<"VISUAL REST DETAIL during "<<during<<" after "<<native->testVisualUpdates()-rested<<std::endl;
    check(during==0&&native->testVisualUpdates()>rested,"a wave picture is not redrawn while a control moves and is redrawn once it rests");}
   {AEffect* fresh=VSTPluginMain(displayHost);auto* instance=static_cast<Vitaleon*>(fresh->object);fresh->dispatcher(fresh,effOpen,0,0,nullptr,0);fresh->dispatcher(fresh,effSetSampleRate,0,0,nullptr,44100);fresh->dispatcher(fresh,effMainsChanged,0,1,nullptr,0);
    for(int n=0;n<10000&&instance->busy();++n){fresh->processReplacing(fresh,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    instance->set(id("ui_group"),1);for(int n=0;n<50;++n){fresh->processReplacing(fresh,nullptr,outputs,128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(instance->getInt(id("ui_page"))==overview,"the OSC group opens with the overview");
    instance->set(id("ui_page"),4);instance->set(id("ui_group"),0);instance->set(id("ui_group"),1);
    check(instance->getInt(id("ui_page"))==4,"after that the OSC group remembers the page used last");fresh->dispatcher(fresh,effClose,0,0,nullptr,0);}
   native->set(id("ui_page"),0);waitNative();}
  check(native->testSplitActive()&&effect->initialDelay==128,"effect chain runs on the worker thread and the 128-sample latency is reported");
  // CPU guard: an overloaded engine steals voices instead of dropping out, and recovers.
  {struct Ev6{int32_t n;intptr_t reserved;VstEvent* events[6];};VstMidiEvent notes[6]{};Ev6 chord{};
   for(int i=0;i<6;++i){notes[i].type=kVstMidiType;notes[i].byteSize=sizeof(VstMidiEvent);notes[i].midiData[0]=char(0x90);notes[i].midiData[1]=char(48+i*3);notes[i].midiData[2]=100;chord.events[i]=reinterpret_cast<VstEvent*>(&notes[i]);}chord.n=6;
   native->set(Vitaleon::Voices,3);for(int i=0;i<4;++i)effect->processReplacing(effect,nullptr,outputs,128);
   native->set(Vitaleon::Voices,8);dispatch(effProcessEvents,0,0,&chord);for(int i=0;i<20;++i)effect->processReplacing(effect,nullptr,outputs,128);
   check(native->testDynamicVoices()==8,"CPU guard leaves the voice limit alone at normal load; a raised voice count applies at once");
   native->testLoad_.store(.95f);for(int i=0;i<400;++i)effect->processReplacing(effect,nullptr,outputs,128);
   check(native->testDynamicVoices()==2,"sustained overload steps the voice limit down to one pair");
   bool guardFinite=true;for(int n=0;n<128;++n)guardFinite&=std::isfinite(left[n])&&std::isfinite(right[n]);check(guardFinite,"audio stays finite while voices are stolen");
   native->testLoad_.store(.2f);for(int i=0;i<int(44100*10/128);++i)effect->processReplacing(effect,nullptr,outputs,128);
   check(native->testDynamicVoices()==8,"the voice limit recovers after calm seconds");
   native->testLoad_.store(-1.0f);for(int i=0;i<6;++i)notes[i].midiData[0]=char(0x80);dispatch(effProcessEvents,0,0,&chord);
   {Ev6 off{};VstMidiEvent cc{};cc.type=kVstMidiType;cc.byteSize=sizeof(VstMidiEvent);cc.midiData[0]=char(0xb0);cc.midiData[1]=120;off.events[0]=reinterpret_cast<VstEvent*>(&cc);off.n=1;dispatch(effProcessEvents,0,0,&off);}
   for(int i=0;i<30;++i)effect->processReplacing(effect,nullptr,outputs,128);}
  // Event order inside one block: note on, sustain pedal, note off. The pedal no longer cuts the
  // block, but it still comes after the note on and before the note off, so the note is held.
  {struct Ev3{int32_t n;intptr_t reserved;VstEvent* events[3];};VstMidiEvent m[3]{};Ev3 ev{};
   for(int i=0;i<3;++i){m[i].type=kVstMidiType;m[i].byteSize=sizeof(VstMidiEvent);m[i].deltaFrames=10+i*10;ev.events[i]=reinterpret_cast<VstEvent*>(&m[i]);}ev.n=3;
   m[0].midiData[0]=char(0x90);m[0].midiData[1]=62;m[0].midiData[2]=100;m[1].midiData[0]=char(0xb0);m[1].midiData[1]=64;m[1].midiData[2]=127;m[2].midiData[0]=char(0x80);m[2].midiData[1]=62;
   const long before=native->testEngineCalls();dispatch(effProcessEvents,0,0,&ev);effect->processReplacing(effect,nullptr,outputs,128);
   check(native->testEngineCalls()-before==3,"note on and note off inside a block cut it, the pedal between them does not");
   for(int i=0;i<int(44100*4/128);++i)effect->processReplacing(effect,nullptr,outputs,128);
   check(native->testActiveVoices()>=1,"a pedal between note on and note off in one block holds the note");
   Ev3 off{};VstMidiEvent cc[2]{};for(int i=0;i<2;++i){cc[i].type=kVstMidiType;cc[i].byteSize=sizeof(VstMidiEvent);cc[i].midiData[0]=char(0xb0);off.events[i]=reinterpret_cast<VstEvent*>(&cc[i]);}
   cc[0].midiData[1]=64;cc[1].midiData[1]=120;off.n=2;dispatch(effProcessEvents,0,0,&off);for(int i=0;i<30;++i)effect->processReplacing(effect,nullptr,outputs,128);
   check(native->testActiveVoices()==0,"pedal up and all sound off end the held note");}
  // Idle sleep: silence parks the engine, the next note wakes it in the same callback.
  {struct Ev{int32_t n;intptr_t reserved;VstEvent* events[1];};VstMidiEvent m{};m.type=kVstMidiType;m.byteSize=sizeof(VstMidiEvent);m.midiData[1]=60;m.midiData[2]=100;Ev ev{};ev.events[0]=reinterpret_cast<VstEvent*>(&m);ev.n=1;
   m.midiData[0]=char(0xb0);m.midiData[1]=120;dispatch(effProcessEvents,0,0,&ev);effect->processReplacing(effect,nullptr,outputs,128);
   check(!native->testAsleep(),"engine is awake right after a MIDI event");
   for(int i=0;i<2400;++i)effect->processReplacing(effect,nullptr,outputs,128);
   bool silent=true;for(int n=0;n<128;++n)silent&=left[n]==0&&right[n]==0;
   check(native->testAsleep()&&silent,"engine sleeps after sustained silence with no active voice");
   m.midiData[0]=char(0x90);m.midiData[1]=60;dispatch(effProcessEvents,0,0,&ev);double energy=0;
   for(int i=0;i<20;++i){effect->processReplacing(effect,nullptr,outputs,128);if(i==0)check(!native->testAsleep(),"note-on wakes the engine in the same callback");for(int n=0;n<128;++n)energy+=double(left[n])*left[n];}
   check(energy>1e-6,"woken engine renders the note");
   m.midiData[0]=char(0x80);dispatch(effProcessEvents,0,0,&ev);}
  dispatch(effMainsChanged,0,0);dispatch(effClose);
 }
 return failures?1:0;
}
