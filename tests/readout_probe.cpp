// SPDX-License-Identifier: GPL-3.0-or-later
// Prints the live graph readouts over time while a note is held.
// usage: readout_probe <ui page> <preset.vital>
#define VITALEON_TEST 1
#include "../src/Vitaleon.cpp"
#include <iostream>
MPC_PLUGIN_ENTRY(Vitaleon)
static std::atomic<int> automates{0};
static VstIntPtr host(AEffect*,VstInt32 opcode,VstInt32,VstIntPtr,void*,float){if(opcode==audioMasterAutomate)automates.fetch_add(1);return opcode==audioMasterVersion?2400:0;}
int main(int argc,char** argv){
 if(argc<3)return 2;const int block=128;
 AEffect* e=VSTPluginMain(host);auto* synth=static_cast<Vitaleon*>(e->object);
 e->dispatcher(e,effOpen,0,0,nullptr,0);e->dispatcher(e,effSetSampleRate,0,0,nullptr,44100);e->dispatcher(e,effSetBlockSize,0,block,nullptr,0);e->dispatcher(e,effMainsChanged,0,1,nullptr,0);
 std::vector<float> l(block),r(block);float* out[]={l.data(),r.data()};
 auto id=[&](const std::string& key){for(int i=0;i<synth->numParams();++i)if(synth->param(i).id==key||synth->param(i).name==key)return i;return -1;};
 auto run=[&](int blocks){for(int i=0;i<blocks;++i){e->processReplacing(e,nullptr,out,block);std::this_thread::sleep_for(std::chrono::microseconds(2900));}};
 auto settle=[&]{for(int i=0;i<20000&&synth->busy();++i){e->processReplacing(e,nullptr,out,block);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 nlohmann::json preset;std::ifstream in(argv[2]);in>>preset;synth->testImport(preset);
 e->setParameter(e,id("ui_page"),synth->param(id("ui_page")).toNormalised(float(std::atoi(argv[1]))));settle();
 struct Ev{int32_t n;intptr_t reserved;VstEvent* events[1];};VstMidiEvent m{};m.type=kVstMidiType;m.byteSize=sizeof(VstMidiEvent);m.midiData[0]=char(0x90);m.midiData[1]=60;m.midiData[2]=100;Ev ev{};ev.events[0]=reinterpret_cast<VstEvent*>(&m);ev.n=1;
 int phase=id("display_lfo_phase"),env=id("display_env_value"),pos=id("display_position_0"),lfo=id("display_lfo_value"),seg=id("display_wave_0_10");
 std::printf("live graphics=%d page=%d\n",int(synth->getBool(id("visuals"))),synth->getInt(id("ui_page")));
 std::printf("before note: phase %.3f env %.3f pos %.3f\n",synth->getNormalised(phase),synth->getNormalised(env),synth->getNormalised(pos));
 e->dispatcher(e,effProcessEvents,0,0,&ev,0);
 for(int step=0;step<12;++step){run(34);std::printf("t=%.1fs phase %.3f env %.3f lfo %.3f pos %.3f seg %.3f automates %d\n",(step+1)*0.1,synth->getNormalised(phase),synth->getNormalised(env),synth->getNormalised(lfo),synth->getNormalised(pos),synth->getNormalised(seg),automates.load());}
 e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);return 0;
}
