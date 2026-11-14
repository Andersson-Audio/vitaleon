// SPDX-License-Identifier: GPL-3.0-or-later
// VST contract test for a future bank catalog. This is NOT an MPC UI test.
#include "../../../framework/mpc_plugin.h"
#include <iostream>
#include <vector>
#include <string>
static std::vector<std::string> cachedNames;
static int notifications=0;
static VstIntPtr host(AEffect* e,VstInt32 op,VstInt32,VstIntPtr,void*,float){
 if(op==audioMasterVersion)return 2400;
 if(op!=audioMasterUpdateDisplay)return 0;
 ++notifications;cachedNames.clear();
 for(int n=0;n<e->numPrograms;++n){char name[256]{};e->dispatcher(e,effGetProgramNameIndexed,n,0,name,0);cachedNames.emplace_back(name);}
 return 1;
}
class Probe:public mpc::Plugin {
 static mpc::PluginInfo info(){mpc::PluginInfo p;p.name="Bank contract test";p.vendor="Andersson Audio";p.uniqueId=MPC_FOURCC('T','s','t','B');return p;}
public:
 Probe():mpc::Plugin(info()){addParam(mpc::floatParam("volume","Volume",0,1,.5f));addProgram("A One",{});addProgram("A Two",{});}
 void process(mpc::ProcessContext&) override{}
 void bank(const std::vector<std::string>& names){
  // Prototype host-thread operation only. Production needs synchronized immutable
  // snapshots and persistent bank identity before worker-driven catalogs are safe.
  std::vector<mpc::Program> next;for(const auto& name:names)next.push_back({name,{}});
  programs_.swap(next);currentProgram_=0;effect_->numPrograms=numPrograms();requestDisplayUpdate();
 }
};
int main(){
 auto* p=new Probe;AEffect* e=mpc::detail::createEffect(p,host);int failures=0;
 auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';failures+=!ok;};
 p->set(0,.75f);p->requestDisplayUpdate();check(cachedNames==std::vector<std::string>{"A One","A Two"},"host model queries initial names");
 p->bank({"B One","B Two","B Three"});check(e->numPrograms==3&&cachedNames.size()==3&&cachedNames[2]=="B Three","increased program count and names are visible during reentrant notification");
 e->dispatcher(e,effSetProgram,0,2,nullptr,0);check(e->dispatcher(e,effGetProgram,0,0,nullptr,0)==2,"new bank program can be selected through VST dispatcher");
 p->bank({"C Only"});check(e->numPrograms==1&&cachedNames==std::vector<std::string>{"C Only"}&&p->currentProgram()==0,"smaller bank removes stale names and clamps current program");
 char invalid[256]{};check(e->dispatcher(e,effGetProgramNameIndexed,2,0,invalid,0)==0,"removed program index is rejected");
 p->bank({"D Renamed"});check(cachedNames[0]=="D Renamed"&&notifications==4&&p->get(0)==.75f,"equal-size bank refresh changes names without changing sound parameters");
 e->dispatcher(e,effClose,0,0,nullptr,0);return failures?1:0;
}
