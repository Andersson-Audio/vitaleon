// SPDX-License-Identifier: GPL-3.0-or-later
// Andersson Audio, 2026-11-13. MPC control surface around the complete Vital graph.
#pragma once
#include "ControlCatalog.h"
#include "PortableAssets.h"
#include "AssetValidation.h"
#include "engine/synth_parameters.h"
#include "engine/modulation_connection_processor.h"
#include "engine/line_generator.h"
#include "engine/value_switch.h"
#include "engine/sample_source.h"
#include "engine/filter_module.h"
#include "engine/smooth_value.h"
#include "wavetable/wavetable_creator.h"
#include "wavetable/wave_source.h"
#include "dsp/wav.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <deque>
#include <condition_variable>
#ifdef __linux__
#include <semaphore.h>
#include <pthread.h>
#endif
#include "StorageBrowser.h"

class Vitaleon : public VitaleonLegacy {
public:
    static constexpr int FullMode=16;
    Vitaleon();
    ~Vitaleon() override;
    void prepare(double sampleRate,int maxBlock) override;
    void reset() override { resetBenchArmed_.store(!getBool(Bench));resetPending_.store(true); }
    void process(mpc::ProcessContext& ctx) override;
    void parameterChanged(int index,float plain) override;
    void saveExtraState(std::vector<uint8_t>& out) override;
    void loadExtraState(const uint8_t* data,size_t size) override;
    void stateRestored(uint32_t count,bool extra) override;
    void restoreParameter(int index,float normalized) override {publishNormalised(index,normalized);}
    // Hides mpc::Plugin::publishNormalised for all code of this class: a write to a
    // sound control must be counted, see controlGeneration_.
    void publishNormalised(int index,float normalized){
        mpc::Plugin::publishNormalised(index,normalized);
        if(index>=0&&index<int(isBinding_.size())&&isBinding_[size_t(index)])controlGeneration_.fetch_add(1,std::memory_order_release);
    }
    void beginStateTransaction(bool writing) override;
    void endStateTransaction(bool writing) override;
    void programSelected(int index) override;
    bool formatValue(int index,float plain,std::string& out) override;
    int numPrograms() const override;
    const char* programName(int index) const override;
    void setProgram(int index) override;
    int controlIndex(const std::string& name)const { auto i=indices_.find(name);return i==indices_.end()?-1:i->second; }
    // Direct integration tests may wait for asynchronous graph/asset edits here.
    bool busy() const {return gate_.load()!=0 || inspectCurve_.load() || curveEditPending_.load() || curveEditBusy_.load() || browserPending_.load()!=0 || requested_.load()!=completed_.load();}
    long testServicePasses()const{return servicePasses_.load();}
#ifdef VITALEON_TEST
    std::array<double,6> testImportTimings(const nlohmann::json& data){
        std::lock_guard<std::mutex> life(lifecycleMutex_);std::lock_guard<std::mutex> state(stateMutex_);
        using Clock=std::chrono::steady_clock;std::array<double,6> times{};auto t=Clock::now();
        auto mark=[&](int i){auto now=Clock::now();times[i]=std::chrono::duration<double,std::milli>(now-t).count();t=now;};
        loadPreset(data);mark(0);syncFullControls(true,true);mark(1);connectRoutes();mark(2);engine_->checkOversampling();mark(3);warmGraph();mark(4);refreshProxies();mark(5);return times;
    }
    void testImport(const nlohmann::json& data){std::lock_guard<std::mutex> life(lifecycleMutex_);std::lock_guard<std::mutex> state(stateMutex_);fxWaitIdle();loadPreset(data);syncFullControls(true,true);connectRoutes();engine_->checkOversampling();warmGraph();refreshProxies();}
    const nlohmann::json& testAssets()const{return savedAssets_;}
    const std::vector<std::string>& testImportNotes()const{return importNotes_;}
    // LOAD of a file row in LIBRARY, without walking there through the drives.
    void testLibraryLoad(const std::string& path){std::lock_guard<std::mutex> lock(browserMutex_);browserJobs_.push_back(BrowserJob{16,path,std::string()});browserPending_.fetch_add(1);}
    // The Init sound (every control at its default), as the "Init" program selects it.
    void testInit(){beginStateTransaction(true);for(size_t k=0;k<initValues_.size();++k)restoreParameter(int(k),param(int(k)).toNormalised(initValues_[k]));selectInit();endStateTransaction(true);reset();}
    // Engine checks start from the Init state instead of the first installed sound.
    void testNoStartSound(){startSound_.store(false);}
    bool testEngineSwitch(const std::string& key)const{for(const auto& b:bindings_)if(b.key==key)return b.value->value()!=0;return false;}
#endif
private:
    using Json=nlohmann::json;
    struct Binding {int index;vital::Value* value;std::string key;float last=-100;bool structural=false;};
    struct ProxyBank {int selector;std::vector<int> proxies;std::vector<std::vector<int>> targets;int selected=-1;};
    struct Route {int source,destination;bool connected=false;vital::modulation_change change{};};
    struct Asset {std::string label,path,entry,name,bank;};
    struct Catalog {std::string path,name;std::vector<Asset> files;std::vector<std::string> names;};
    std::shared_ptr<const Catalog> activeCatalog_,initialCatalog_;
    Asset selectedAsset_,pendingAsset_;
    bool externalPresetPending_=false;
    struct BrowserState {std::string path,root,status="Choose a drive, folder, then a bank.";std::vector<vitaleon_storage::Entry> entries;int page=0;};
    struct BrowserJob {int command;std::string path,root;int program=-1;};
    BrowserState browser_;
    std::mutex browserMutex_;
    std::deque<BrowserJob> browserJobs_;
    std::atomic<int> browserPending_{0};
    int storageCommands_=-1,storageRows_=-1,storagePath_=-1,storageStatus_=-1,storagePage_=-1,activeBankText_=-1;
    int storageAvailable_=-1;
    int uiGroup_=-1,uiPage_=-1,uiSection_=-1,uiKnobs_=-1,uiBank_=-1;
    // What the host still has to hear about the Q-Link slots (0.3.23). A moving control
    // must not call into the host: 0.3.22 sent audioMasterAutomate on every movement and
    // redrew the page when the knob group changed, and the controls felt sluggish on the
    // device. The service thread sends these instead, the sound parameter and the page
    // redraw only once the control has rested for kSlotRestMs.
    static constexpr int kSlotRestMs=300;
    std::array<std::atomic<int>,16> slotAutomated_{}; // sound parameter of a moved slot, -1 none
    std::atomic<int> slotStepped_{-1};                // switch or list that stepped: redraw it now
    std::atomic<bool> bankChanged_{false},bankRedraw_{false};
    std::atomic<long long> slotEditMs_{0};            // time of the last slot write, 0 = at rest
    static long long nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    void flushSlotNotifications();
    // Matrix table and its two-step target picker (helper views are hidden).
    int matrixView_=-1,pickGroupView_=-1,pickTargetView_=-1,routeTargetProxy_=-1,uiPick_=-1;
    // 0.3.43: the matrix shows each source and target in the colour of its section.
    // matrixTint_: sixteen readouts, the colour class of the source (0-7) and of the target
    // (8-15) of the eight routes on screen: 0 none, 1 OSC, 2 FILTER, 3 ENV, 4 LFO, 5 FX.
    int matrixTint_=-1;
    std::vector<unsigned char> sourceTint_,targetTint_;
    static int tintOf(const std::string& name);
    // 0.3.44: SAVE SOUND, see FullSave.inc. uiSave_: 1 opens the page, 2 saves (momentary).
    // uiKey_: one key of the name entry (momentary). saveCaps_: every letter a capital.
    static constexpr size_t kSaveNameLength=24;
    int saveView_=-1,uiSave_=-1,uiKey_=-1,saveCaps_=-1,saveNameText_=-1,saveStatusText_=-1;
    std::mutex saveMutex_;std::string saveName_,saveStatus_,saveConfirm_;
    Json presetJson(const std::string& name);
    static std::string saveNameFrom(const std::string& text);
    void publishSaveTexts();bool formatSaveText(int index,std::string& out);
    void openSave();void saveKey(int key);void saveSound();
    // 0.3.45: the library shows the folder of the saved sounds (MY SOUNDS) and copies them to
    // another drive (EXPORT). libraryStep_: 1 MY SOUNDS, 2 EXPORT (momentary).
    int libraryStep_=-1;void showMySounds();
    // 0.3.50: INIT SOUND on HOME. uiInit_ is a switch. The first tap turns it on (the button
    // then reads CONFIRM INIT) and notes the time in initArmedMs_; the second tap, within
    // initConfirmMs_, turns it off and resets the sound (initSound, job 23). Left alone, the
    // switch goes back by itself and nothing is changed (initService).
    int uiInit_=-1;std::atomic<long long> initArmedMs_{0};std::string initPriorMessage_;
    static constexpr const char* kInitAsk="Tap CONFIRM INIT to reset.";   // one text of the MPC: 31 characters at most
    void initSound();void initService();
    // 0.3.52: wavetables from the OSC overview.
    // tableStep_ (momentary): 1/2 the table before/after the one oscillator 1 plays, 3/4 for
    // oscillator 2, 5/6 for oscillator 3, through every table file that is installed.
    // uiTables_ (momentary, 1 to 3): opens the page of the factory tables for that oscillator.
    // tableTarget_: the oscillator that page works on. tablePick_: the factory table it plays
    // (0: another table); written by a tap on the page and by the plugin, so the page marks it.
    // tableNameText_: the names of the three tables in use.
    static constexpr int kFactoryTables=32;
    int tablesView_=-1,tableStep_=-1,uiTables_=-1,tableTarget_=-1,tablePick_=-1,tableNameText_=-1;
    std::array<int,kFactoryTables+1> factoryTable_{};   // number of a factory table -> its place in the table selector, 0: not installed
    std::array<std::string,3> tableName_;bool tableNamesShown_=false;
    void findFactoryTables();int factoryNumber(int selection)const;
    void stepTable(int oscillator,int direction);void pickTable(int number);void publishTablePick();
    void publishTableNames();bool formatTableName(int index,std::string& out);
    // 0.3.53: pages of the wavetable page, the user's folder of wavetables, pictures drawn by
    // the plugin. See FullTables.inc.
    // tablePage_: 0 the factory tables, 1.. pages of 32 other tables. tablePageStep_
    // (momentary): 1 the page before, 2 the next. tableRowTap_ (32 momentary switches): a tap on
    // a row. tableRowOn_: the row holds a table. tableRowText_: its name.
    // The picture of a row is 24 values, each one segment of a line from one point of the wave
    // to the next, both in 11 levels. 0.3.55: these 768 values are not parameters of their own
    // (0.3.54 had them, and every page change on the MPC took 3.6 s instead of 0.8 s): while a
    // page of user tables is on screen they are written into the 768 values of the three large
    // wave pictures, which are not on screen then (waveColumn). waveStale_ says which of the
    // three wave pictures has to be put back.
    // tableRowMark_: the row whose table the oscillator of the page plays. tableEmpty_: the
    // page of user tables has nothing to show. tablePageText_: 'FACTORY', 'USER 1 OF 3'.
    static constexpr int kTableRows=32,kTablePoints=25,kTableLevels=11;
    std::atomic<int> waveStale_{0};
    struct TableFile{std::string name,path;int selector=0,factory=0;std::string group;};   // selector: place in the table selector, 0 for a file of the user's folder
    // 0.3.59: a page of the wavetable pages. The factory page comes first (its rows are pictures of
    // the skin, it lists no places), then the pages of every folder of tables that ships with the
    // plugin (group: the folder's name, as 'FIZMATIC 1 OF 2'), then the user's ('USER 1 OF n').
    struct TablePage{std::string name;std::vector<int> places;};
    std::vector<TablePage> tablePages();
    std::mutex tableMutex_;                        // guards the six members below; never held while another lock is taken
    std::vector<TableFile> tableFiles_;std::vector<int> userRows_;
    std::array<std::string,3> userTablePath_;      // the file of the user's folder an oscillator plays
    std::array<std::string,kTableRows> tableRowName_;std::string tablePageName_;
    std::map<std::string,std::array<unsigned char,kTablePoints>> tablePictures_;   // browser thread only
    int tablePage_=-1,tablePageStep_=-1,tableRowMark_=-1,tableEmpty_=-1,tablePageText_=-1,tableRowTap_=-1,tableRowOn_=-1,tableRowText_=-1;
    void scanTableFiles();void showTablePage();void tapTableRow(int row);void loadTableFile(int place,int oscillator);
    std::array<unsigned char,kTablePoints> tablePicture(const TableFile& file);
    std::string keepTable(const std::string& source,std::string& kept);bool formatTableRow(int index,std::string& out);
    void requestTablePage(){std::lock_guard<std::mutex> lock(browserMutex_);if(browserJobs_.size()<16){browserJobs_.push_back(BrowserJob{24,std::string(),std::string()});browserPending_.fetch_add(1);}}
#ifdef VITALEON_HOSTLOG
    // Device measurement of the calls the MPC makes, see FullHostLog.inc.
    long long hostLogStart_=0,hostLogBucket_=0,hostLogWritten_=0,hostLogLastMs_=0;int hostLogRepeat_=0;bool hostLogDirty_=false;
    std::string hostLog_,hostLogLast_,hostLogState_;void hostLogService();
#endif
public:
    std::string exportSounds(const std::string& driveRoot);   // returns the sentence for the status line
    static std::string userFolder(const char* part);   // <drive>/Vitaleon/<part>: what the user keeps
private:
    int oscOverviewView_=-1; // the page with all three oscillators and the sample (0.3.26)
    int uiBrowse_=-1,libraryView_=-1; // TABLE LIST / SAMPLE LIST buttons open the library there (0.3.28)
    static constexpr int kSampleColumns=128;int sampleReadout_=-1;uint64_t samplePictureKey_=0;
    static constexpr int kSampleFineColumns=256;int sampleFineReadout_=-1;   // the SAMPLE page (0.3.32)
    // Envelope page (0.3.34): the plugin draws the selected envelope itself, in columns like
    // the other pictures. Device, 0.3.33: the MPC's own envelope picture gives each stage a
    // fixed slot sized for the longest time (32 s), so an ordinary envelope sat squeezed into
    // the left of the picture. Here the five time stages share the width in proportion to
    // their knob positions and always fill it; the held level keeps a fixed share.
    static constexpr int kEnvColumns=312;
    static constexpr float kEnvSustainShare=0.2f;
    int envShapeReadout_=-1;std::array<std::array<int,9>,6> envShapeParams_{};
    // SHAPE / DRAG on an envelope page: 0 = the drawn picture, 1 = the MPC's own envelope
    // with points that can be dragged (user, 0.3.34: "can we still drag by hand?").
    int uiEnvView_=-1;
    // Bank selector and bank install (0.3.39). The PRESET list holds one bank: the factory
    // bank at start unless a project names another. Bank Step: 1 = previous installed bank,
    // 2 = next, 3 = show the installed banks in the library. Install Bank: a bank opened from
    // another drive is first copied into the plugin's own Banks folder, so it stays.
    int bankStep_=-1,bankKeep_=-1;
    struct InstalledBank {std::string name,path;bool folder=false;};
    std::vector<InstalledBank> installedBanks()const;
    std::shared_ptr<const Catalog> catalogFor(const InstalledBank& bank);
    std::string installBank(const std::string& source);
    void startFirstSound();
    // Names of the sixteen Q-Link slots (0.3.37). The MPC shows the name of the parameter a
    // knob is mapped to, and the knobs are mapped to the slots, so it showed "UI Control n"
    // (user on the device). A slot now carries the name of the control it reaches on the page
    // on screen. Only once audio runs: the parameter list that the build tools read keeps the
    // fixed names.
    std::atomic<bool> slotNamesLive_{false},slotNamesDirty_{false};
    void renameSlots();
    std::string slotName(int target);
    // FX overview (0.3.36): the nine effects in the order of the chain. Per position a text
    // (the name of the effect there), an on switch that forwards to that effect, and a
    // command that moves the chosen effect one place earlier or later.
    int fxSlotText_=-1,fxSlotOn_=-1,uiFxMove_=-1;std::array<int,9> fxOnParam_{};
    bool formatFxSlot(int index,float plain,std::string& out);
    void refreshSamplePicture();
    // Name of the loaded sample for the SAMPLE page (0.3.31).
    int sampleNameText_=-1;std::string sampleName_;bool sampleNameShown_=false;
    void publishSampleName();
    bool formatSampleName(int index,std::string& out);
    // FILTERS page (0.3.30): both filters on one page, each with a picture of its measured
    // response, see FullFilterView.inc. tools/generate_surface.py reads the four numbers.
    static constexpr int kFilterColumns=144,kFilterLevels=176;
    static constexpr float kFilterTopDb=24.0f,kFilterFloorDb=-60.0f;
    static constexpr int kFilterProbeRate=44100,kFilterProbeLength=2048,kFilterPictureMs=100,kFilterRestMs=250;
    static constexpr float kFilterImpulse=0.05f;
    struct FilterFollow {int index;vital::Value* value;vital::SmoothValue* smooth;};
    struct FilterProbe {
        vital::FilterModule module;
        vital::Output audio{vital::kMaxBufferSize},keytrack{vital::kMaxBufferSize},midi{vital::kMaxBufferSize};
        std::vector<FilterFollow> follow;   // plugin parameter -> control of the private filter
        std::vector<float> response;uint64_t shown=0,seen=0;long long seenMs=0;
        explicit FilterProbe(const std::string& prefix):module(prefix){}
    };
    std::array<std::unique_ptr<FilterProbe>,2> filterProbes_;
    std::array<int,2> filterReadout_{{-1,-1}};
    // routeCurveView_: the page with the curve of a route, HOME > ADVANCED (0.3.40; HOME > OTHER in 0.3.38)
    int filtersView_=-1,routeCurveView_=-1;long long filterPictureMs_=0;std::atomic<long> filterPictures_{0};
    void createFilterProbes();
    void refreshFilterPicture();
public:
    long testFilterPictures()const{return filterPictures_.load();}
    long testVisualUpdates()const{return visualUpdates_.load();}
private:
    // Texts of the loaded sound on HOME (0.3.27), see formatSoundText in FullAssets.inc.
    int soundInfoText_=-1,soundNoteText_=-1,macroNameText_=-1;size_t soundTextsShown_=0;
    void publishSoundTexts();
    bool formatSoundText(int index,std::string& out);
    void navigateTo(int page,int section);
    struct SurfaceView {int group,member;std::vector<std::vector<int>> sections;bool hidden=false;int redirect=-1;};   // redirect: a retired page opens this one
    std::vector<SurfaceView> surfaceViews_;
    std::array<std::atomic<int>,16> uiTargets_{};
    std::array<int,7> rememberedPages_{};
    std::mutex navigationMutex_;
    void createStorage();
    bool storageChanged(int index);
    bool formatStorage(int index,std::string& out);
    void serviceBrowser();
    void publishBrowser();
    std::shared_ptr<const Catalog> indexBank(const std::string& path);
    void installCatalog(std::shared_ptr<const Catalog> catalog,bool lifecycleHeld=false,bool first=false);
    void createNavigation();
    void updateNavigation();
    bool navigationChanged(int index,float plain);
    void refreshNavigation();
    struct LibraryBank {std::string name;std::vector<int> files;};
    std::vector<LibraryBank> libraryBanks_;
    int factoryProgramCount_=0; // 1 = the "Init" program of a plugin without sounds, else 0
    std::vector<int> filePrograms_; // immutable after construction; native VST programs
    std::vector<float> initValues_; // plain values of the Init sound
    std::atomic<bool> startSound_{false}; // load the first installed sound on first prepare
    void selectInit();
    int libraryBank_=-1,libraryPage_=-1,libraryLoad_=-1,libraryPrevious_=-1,libraryNext_=-1,bankPrevious_=-1,bankNext_=-1;
    int libraryRows_=-1,libraryNames_=-1,libraryBankText_=-1,librarySelectedText_=-1,libraryLoadedText_=-1,libraryPageText_=-1;
    std::atomic<int> pendingPreset_{-1};
    // UI caches must be refreshed explicitly after browser/state changes.
    // The service thread delivers host callbacks outside the engine locks.
    std::atomic<bool> browserDisplayDirty_{false};
    std::string loadedName_="Built-in / current";
    int homeControls_=-1;
    int homeFilter_=-1;
    std::array<int,7> homeFullTargets_{};
    int homeTarget(int slot)const{return homeFullTargets_[slot];}
    void refreshHome(){if(homeControls_>=0)for(int i=0;i<7;++i)publishNormalised(homeControls_+i,getNormalised(homeTarget(i)));if(homeFilter_>=0)publishNormalised(homeFilter_,getNormalised(controlIndex("filter_1_on")));}
    std::vector<Binding> bindings_;
    // The audio thread copied every one of the 770 sound controls into the engine in
    // every block, about a tenth of the cost of a single voice (profiled 0.3.24).
    // Every write to a sound control now counts up, after the value is stored; the
    // audio thread walks the controls only when the count moved.
    std::vector<uint8_t> isBinding_;
    std::atomic<unsigned> controlGeneration_{1};unsigned controlSynced_=0;
    std::map<std::string,int> indices_;
    std::vector<ProxyBank> banks_;
    std::array<Route,64> routes_{};
    std::vector<std::string> sources_,destinations_;
    std::vector<vital::Output*> sourceOutputs_;
    std::array<std::vector<Asset>,4> assets_; // presets, tables, samples, LFO shapes
    std::array<int,3> tableSelect_{};
    int presetSelect_=-1,sampleSelect_=-1,lfoShapeSelect_=-1,loadStatus_=-1;
    int loadText_=-1;
    int lfoSelect_=-1,envSelect_=-1,routeSelect_=-1;
    int pointSelect_=-1,pointX_=-1,pointY_=-1,pointCurve_=-1,pointCount_=-1;
    int lfoShape_=-1,lfoSmooth_=-1;
    int visualEnabled_=-1;
    std::array<int,3> waveReadout_{};
    std::array<int,3> waveBankReadout_{};
    int curveBankReadout_=-1;
    std::array<int,3> wavePosition_{};
    int lfoReadout_=-1,modReadout_=-1,envReadout_=-1,curveReadout_=-1;
    int lfoPhaseReadout_=-1;
    std::array<vital::Output*,3> waveMono_{},wavePoly_{};
    std::array<vital::ValueSwitch*,3> waveMonoSwitch_{},wavePolySwitch_{};
    std::array<const vital::StatusOutput*,8> lfoStatus_{},lfoPhase_{};
    std::array<int,8> lfoSource_{}; // index of lfo_N in sources_
    std::array<const vital::StatusOutput*,6> envStatus_{};
    std::array<std::array<float,128>,3> waveform_{};
    Json savedAssets_=Json::object(),stagedState_;
    std::string loadMessage_="Ready";
    std::vector<std::string> importNotes_; // what the last import left out
    std::shared_ptr<const Catalog> indexFolder(const std::string& folder);
    std::mutex stateMutex_; // ONLY non-audio state/asset service uses this.
    std::atomic<int> gate_{0}; // 0=audio owns engine, 1=park requested, 2=parked, 3=resume, 4=audio is rendering, 5=fade out, then park
    // 0.3.47: no click when a sound changes. While audio runs, the callback fades the output out
    // before it hands the engine over (gate 5) and fades it in again when it takes it back.
    // Before that the output was cut to zero at the edge of a block.
    static constexpr int kFadeSamples=384;   // about 9 ms at 44.1 kHz
    int fadeOutPos_=-1,fadeInPos_=-1;        // audio thread only: samples of a fade already played, -1 = none
    int fadeInSkipped_=0;                    // silent samples since the engine was taken back
    std::atomic<long long> lastProcessMs_{0};
    bool fadeOut(mpc::ProcessContext& ctx);void fadeIn(mpc::ProcessContext& ctx);
public:
    // How long a caller waits for the fade before it takes the engine anyway. 0: never wait
    // (a host or a test that calls from the thread that also renders the audio).
    static inline std::atomic<int> parkFadeWaitMs_{80};
    static inline std::atomic<int> initConfirmMs_{5000};   // how long CONFIRM INIT waits for the second tap
private:
    std::atomic<unsigned> requested_{0},completed_{0};
    std::atomic<bool> graphRebuild_{false};
    std::atomic<bool> quit_{false},resetPending_{false},replayNotes_{false};
    std::atomic<bool> resetBenchArmed_{true};
    std::atomic<unsigned> curveEdits_{0};
    // Device, 0.3.35: every step of a curve control (shape, points, point time, level, curve,
    // smoothing) stopped the engine for a service pass and was heard as a glitch. A curve is
    // edited in place, as in Vital itself, so these edits now run beside the audio.
    std::atomic<bool> curveEditPending_{false},curveEditBusy_{false};
    std::atomic<long> servicePasses_{0};   // passes that parked the engine (tests)
    int curveShape(const LineGenerator* curve)const;
    void captureCurves();
    std::atomic<bool> curveSelectionChanged_{true};
    std::atomic<bool> inspectCurve_{false};
    std::atomic<bool> programReset_{false};
    std::atomic<bool> migrateOldTables_{false};
    std::mutex lifecycleMutex_; // serialize prepare and non-audio service only
    std::thread service_;
    std::array<std::atomic<int>,2048> held_{};
    // Graph readouts already sent to the host (service thread only).
    std::chrono::steady_clock::time_point lastVisual_{};
    // Graph refresh (0.3.18). Measured on the Live II: every audioMasterAutomate call for
    // a readout can delay the MPC's audio callback (240 late callbacks in 10 s at 780
    // calls/s, still 28 in 7 s at 174 calls/s; real-time priority or calling from the
    // audio thread change nothing). ONE audioMasterUpdateDisplay per picture refreshes
    // all readouts and caused no late callback. The audio thread bumps visualVersion_
    // when a value of the visible graph changed; the service thread then asks for one
    // display update, at most every visualIntervalMs_.
    std::atomic<unsigned> visualVersion_{0};unsigned visualShown_=0;std::atomic<int> visualIntervalMs_{50};
    // Device, 0.3.30: a display update redraws the page, and a control that is being turned
    // stutters while that happens. So no picture is redrawn until every control has rested
    // for kVisualRestMs; the readouts are still written, the update only waits.
    static constexpr int kVisualRestMs=250;
    // 0.3.56: a page and its picture in ONE redraw. Device, 0.3.55: a page with a picture was
    // drawn twice, once for the page change and again 0.3 to 0.5 s later when the picture of the
    // new page had been computed, and each redraw takes the MPC about 0.8 s. Now the redraw of
    // a page change waits until the pictures have been computed once for the new page (the next
    // audio block, forced by displayNow_; at most kPageTurnWaitMs when no audio runs), and what
    // that redraw shows is not announced a second time.
    static constexpr int kPageTurnWaitMs=60;
    std::atomic<long long> pageTurnMs_{0};std::atomic<unsigned> displayPasses_{0},pageTurnPass_{0};
    std::atomic<int> displayPassPage_{-1};std::atomic<bool> displayNow_{false};bool filterPictureNow_=false;
    std::atomic<int> tablePageMs_{-1};   // how long the last page of wavetables took to prepare (for the recorder)
    std::atomic<long long> lastEditMs_{0};   // time of the last parameter write that came from outside
    std::atomic<long> visualUpdates_{0};     // display updates asked for by notifyVisuals (tests)
    // Graph resolution; tools/generate_surface.py reads these four numbers.
    static constexpr int kWaveColumns=256,kWaveLevels=108,kCurveColumns=384,kCurveLevels=108;
    // 0.3.32 (user: less pixelated): twice the columns. The first 96 and the next 96 columns
    // are the two old readout blocks; the rest was appended (64 per wave, 192 for the curve).
    std::array<int,3> waveFineReadout_{};int curveFineReadout_=-1;
    int waveColumn(int osc,int column)const{return column<96?waveReadout_[size_t(osc)]+column:column<192?waveBankReadout_[size_t(osc)]+column-96:waveFineReadout_[size_t(osc)]+column-192;}
    int curveColumn(int column)const{return column<96?curveReadout_+column:column<192?curveBankReadout_+column-96:curveFineReadout_+column-192;}
#ifdef VITALEON_GFXDIAG
    // Device measurement (never in a release): the plugin holds a chord itself while
    // any page except HOME is open. On an OSC page it cycles four refresh rates
    // (10, 15, 20, 30 pictures a second), 4 s each; any other page is the baseline.
    // ADVANCED shows the figures.
    struct GfxStat{std::atomic<long> blocks{0},late{0},sumUs{0},maxUs{0},maxGapUs{0},maxWaitUs{0},calls{0},callSumUs{0},callMaxUs{0},frames{0};};
    std::array<GfxStat,5> gfx_;std::atomic<int> gfxMode_{-1};long gfxFrames_=0,gfxSince_=0;bool gfxHeld_=false;int gfxLine_=0;long gfxWaitUs_=0;
    std::chrono::steady_clock::time_point gfxLastStart_{},gfxLastReport_{};
    void gfxStep(int frames);void gfxReport();
#endif
#if defined(VITALEON_TEST)||defined(VITALEON_MIDIDIAG)
    // How MIDI cuts the host blocks (renderBlock): engine calls made, and the calls it took
    // while every event inside a block still cut it. Kinds by status: note off, note on,
    // poly pressure, controller, program, channel pressure, pitch bend, other.
    struct MidiCuts{std::atomic<long> blocks{0},eventBlocks{0},calls{0},callsBefore{0};std::array<std::atomic<long>,8> kinds{},kindsInside{},offsetBins{};
        std::atomic<int> minFrames{1<<30},maxFrames{0};long written=-1;std::chrono::steady_clock::time_point at{};} midiCuts_;
    void midiDiagService();
#endif
#ifdef VITALEON_CPUDIAG
    // Device measurement of every installed sound, see FullCpuDiag.inc.
    struct CpuRow{float mean=0,p99=0,peak=0,fxMean=0,fxPeak=0,twinMean=0,twinPeak=0,level=0;int blocks=0,over=0,late=0,voices=0,guard=0,notesA=0,notesB=0;long maxGapUs=0;};
    struct CpuRun{double sum=0,fxSum=0,twinSum=0,levelSum=0;float peak=0,fxPeak=0,twinPeak=0;int blocks=0,over=0,late=0,voices=0,guard=32;long maxGapUs=0;std::array<int,81> histogram{};};
    std::vector<std::array<CpuRow,3>> cpuRows_; // per sound: 4-note chord, 8-note chord, chord over two engines
    std::atomic<int> cpuRequest_{-1},cpuDone_{0},cpuBlock_{0};std::atomic<bool> cpuLoaded_{false};
    int cpuPhase_=0,cpuSound_=0,cpuLoading_=-1,cpuWritten_=0,cpuLastFrames_=0;long cpuFrames_=0;CpuRun cpuRun_;
    std::chrono::steady_clock::time_point cpuLastStart_{};
    void cpuControl(int frames);void cpuMeasure(int frames,float ratio,float fx,float outputPeak,std::chrono::steady_clock::time_point started);void cpuService();
#endif
    void notifyVisuals();
    bool ready_=false;
    bool asleep_=false;int silentFrames_=0; // audio thread only
    // ---- Effects on a second core -------------------------------------------
    // Measured on the Live II, the effect chain costs about 16 % of a 128-frame
    // block on top of the voices. Vital's voices share intermediate buffers and
    // cannot run in parallel, but voices and effect chain can form a pipeline:
    // the audio thread renders the voices of chunk k while the worker renders
    // the effects of chunk k-1. Output is delayed by kFxLatency samples.
    // Rule: the worker is idle (fxWaitIdle) whenever anything but
    // SoundEngine::processStageA touches the engine.
    struct FxSignal{
#ifdef __linux__
        sem_t semaphore;FxSignal(){sem_init(&semaphore,0,0);}~FxSignal(){sem_destroy(&semaphore);}
        void post(){sem_post(&semaphore);}void wait(){while(sem_wait(&semaphore)!=0){}}
#else
        std::mutex mutex;std::condition_variable condition;int count=0;
        void post(){{std::lock_guard<std::mutex> lock(mutex);++count;}condition.notify_one();}
        void wait(){std::unique_lock<std::mutex> lock(mutex);condition.wait(lock,[&]{return count>0;});--count;}
#endif
    };
    static constexpr int kFxRing=1024,kFxLatency=128;
    FxSignal fxSignal_;std::thread fxThread_;
    std::atomic<int> fxPending_{0};std::atomic<bool> fxQuit_{false},fxSplit_{false},fxWanted_{true};std::atomic<float> fxLoad_{0};
    std::array<std::array<float,kFxRing>,2> fxRing_{};int fxRead_=0,fxWrite_=kFxLatency;bool fxPriority_=false;
    void fxLoop();void fxWaitIdle();void fxReset();
#ifdef VITALEON_CPUDIAG
    // Twin-engine trial, see FullCpuDiag.inc.
    std::unique_ptr<vital::SoundEngine> engineB_;std::array<Route,64> twinRoutes_{};vital::Value* twinPolyphony_=nullptr;
    FxSignal twinSignal_;std::thread twinThread_;std::atomic<int> twinPending_{0};std::atomic<bool> twinQuit_{false};std::atomic<float> twinLoad_{0};
    bool twinActive_=false,twinPriority_=false;int cpuNotesA_=0,cpuNotesB_=0;
    std::vector<float> cpuCopyMs_;long cpuRssBefore_=0,cpuRssAfter_=0,cpuRssAfterCopy_=0;std::string cpuTwinError_;
    static long cpuRssKb();void twinLoop();void twinWaitIdle();void twinCopy();
#endif
public:
    void testSplit(bool enabled){fxWaitIdle();fxWanted_.store(enabled);fxReset();fxSplit_.store(enabled&&fxThread_.joinable()&&engine_->canSplit());}
    bool testSplitActive()const{return fxSplit_.load();}
private:
    // CPU guard (audio thread only). Measured on the Live II: one voice pair of a
    // dense preset costs about 22 % of a 128-frame block, effects another 16 %.
    // Instead of dropouts the oldest voices are stolen while the engine needs
    // more than 80 % of the block time; the limit recovers after 3 calm seconds.
    float cpuLoad_=0;int dynamicVoices_=32,guardUser_=-1;long guardHold_=0,calmFrames_=0;
    void cpuGuard(float ratio,int frames);
public:
    std::atomic<float> testLoad_{-1.0f};
    int testDynamicVoices()const{return dynamicVoices_;}
private:
    // Effects that are switched on but mixed to exactly 0 (the usual "macro opens
    // the effect" preset design) are not processed. See planEffectBypass().
    struct FxBypass{int onParam=-1,mixParam=-1;vital::Value* onValue=nullptr;std::atomic<bool> eligible{false};std::atomic<unsigned> macros{0};};
    std::array<FxBypass,8> fxBypass_;std::array<int,4> macroParam_{{-1,-1,-1,-1}};
    std::atomic<bool> fxPlanDirty_{true};
    void planEffectBypass();
    // Display-refresh diagnostic (ADVANCED page only): a free-running 2 s ramp
    // written to three parameters that differ only in how the host is told.
    int diagConfig_=-1,diagCount_=0,diagShown_=0;long diagFrames_=0;double diagSum_=0;
    std::array<std::atomic<int>,8> diagResult_{};std::atomic<int> diagBlock_{0},diagVersion_{0};
    void diagApply(int config);void diagStep(int frames);void diagReport();
public:
    bool testAsleep()const{return asleep_;}
    int testActiveVoices()const{return engine_->getNumActiveVoices();}
    const float* testWaveFrame(int oscillator,int frame)const{return engine_->getWavetable(oscillator)->getBuffer(frame);}
#if defined(VITALEON_TEST)||defined(VITALEON_MIDIDIAG)
    long testEngineCalls()const{return midiCuts_.calls.load();}
#endif
#ifdef VITALEON_PROFILE
    // Per-block cost of the wrapper's own loops, in microseconds (development only).
    std::array<double,4> testWrapperTimes(){
        using Clock=std::chrono::steady_clock;std::array<double,4> us{};const int n=20000;
        auto time=[&](int slot,auto&& work){auto t=Clock::now();for(int i=0;i<n;++i)work();us[size_t(slot)]=std::chrono::duration<double,std::micro>(Clock::now()-t).count()/n;};
        time(0,[&]{syncFullControls();});time(1,[&]{refreshProxies();});
        time(2,[&]{displayCountdown_=0;updateDisplays(128);});
        time(3,[&]{for(auto& f:fxBypass_){if(f.onParam<0)continue;bool want=getBool(f.onParam);const float value=want?1.0f:0.0f;if(f.onValue->value()!=value)f.onValue->set(value);}});
        return us;}
#endif
    bool testSlotsAtRest()const{return slotEditMs_.load()==0;}
private:
    int displayCountdown_=0,lastPreset_=0,lastSample_=0,lastShape_=0;
    std::array<int,3> lastTable_{{0,0,0}};
    double preparedRate_=44100,lastTimeline_=-1;
    int advancedSelector_=-1,advancedValue_=-1;
    int curveTarget_=-1,effectSlot_=-1,effectType_=-1,effectOrder_=-1,mpeEnabled_=-1,mpeLower_=-1,mpeUpper_=-1;
    std::array<int,16> rpnMsb_{},rpnLsb_{};
    std::array<float,16> memberBendRange_{};
    std::vector<int> advancedTargets_;
    void createControls();
    void createBanks();
    void createReadouts();
    void refreshProxies();
    void syncFullControls(bool force=false,bool structural=false);
    void renderBlock(mpc::ProcessContext& ctx);
    void updateDisplays(int frames);
    void handleFullMidi(const mpc::MidiEvent& e);
    void serviceLoop();
    void parkEngine();
    void serviceChanges(bool rebuild);
    void connectRoutes();
    void clearRoutes();
    void warmGraph();
    void scanAssets();
    void createLibrary();
    void createFilePrograms();
    void refreshLibrary();
    bool libraryChanged(int index,float plain);
    bool formatLibrary(int index,std::string& out);
    int libraryFile(int row)const;
    int libraryPageCount()const;
    static std::string textPart(const std::string& text,int part);
    void publishLoadedName();
    void loadSelections();
    // Import from the library browser (0.3.25), see queueImport/applyImport in FullAssets.inc.
    // kind: 1 wavetable state, 2 wavetable audio, 3 LFO shape, 4 sample.
    struct PendingImport{int kind=0,target=0,rate=44100;std::string name;Json data;std::vector<float> audio;std::string userPath,note;};   // userPath, note: a table of the user's folder (0.3.53)
    PendingImport pendingImport_;bool importPending_=false;int importTarget_=-1,lastTableTarget_=0;
    void queueImport(const std::string& path,int oscillator=-1,bool keep=false);
    void applyImport();
    void loadTableAudio(int oscillator,const std::vector<float>& audio,int rate,const std::string& name);
    void loadPreset(const Json& preset);
    void restoreAssets(const Json& state,bool tables=true,bool samples=true);
    void loadTable(int oscillator,const Json& table);
    void loadSampleState(const Json& sample,const vitaleon_assets::SampleData* decoded=nullptr);
    void loadCurve(LineGenerator* curve,const Json& data);
    void editLfo();
    void refreshCurveControls();
    void setLoadStatus(int status){
        if(status==1)loadMessage_="Loading...";
        publish(loadStatus_,float(status));
        float marker=status==1?.5f:float(std::hash<std::string>{}(loadMessage_)&0xffffff)/16777216.0f;
        if(loadText_>=0)for(int i=0;i<4;++i)publishNormalised(loadText_+i,marker);
        if(storageAvailable_>=0){std::lock_guard<std::mutex> browser(browserMutex_);browser_.status=loadMessage_;publishBrowser();}
        browserDisplayDirty_.store(true);
    }
    LineGenerator* selectedCurve(){return getBool(curveTarget_)?engine_->getModulationBank().atIndex(getInt(routeSelect_)-1)->modulation_processor->lineMapGenerator():engine_->getLfoSource(getInt(lfoSelect_)-1);}
    void defaultTables();
    void migrateOldControls();
    void migrationTables();
    void captureAssets();
    Json readAsset(const Asset& asset);
    std::vector<uint8_t> readAssetBytes(const Asset& asset);
    void requestService(bool rebuild=true){if(ready_){if(rebuild)graphRebuild_.store(true);requested_.fetch_add(1,std::memory_order_release);}}
    int addProxyBank(const std::string& title,const std::string& prefix,int count,const std::vector<std::string>& suffixes);
    int addReadout(const std::string& id);
    void publish(int index,float plain){publishNormalised(index,param(index).toNormalised(plain));}
};

#include "FullControls.inc"
#include "FullAssets.inc"
#include "FullLibrary.inc"
#include "FullStorage.inc"
#include "FullSave.inc"
#include "FullTables.inc"
#include "FullNavigation.inc"
#include "FullFilterView.inc"
#include "FullRuntime.inc"
#include "FullCpuDiag.inc"
#include "FullHostLog.inc"
