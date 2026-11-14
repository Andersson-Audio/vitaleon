// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#include <algorithm>
#include "../src/BankArchive.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 2;std::string root=argv[1];int failures=0;
 auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;};
 for(const char* file:{"stored.vitalbank","deflated.vitalbank"}){
  BankArchive bank(root+"/"+file);auto bytes=bank.extract("Presets/Test.vital");
  check(std::string(bytes.begin(),bytes.end())=="{\"synth_version\":\"1.0.6\"}",file);
  auto empty=bank.extract("empty.txt");check(empty.empty(),"empty ZIP entry");
 }
 for(const char* file:{"crc.vitalbank","truncated.vitalbank","encrypted.vitalbank","duplicate.vitalbank","offset.vitalbank"}){
  bool rejected=false;try{BankArchive bank(root+"/"+file);bank.extract("Presets/Test.vital");}catch(const std::exception&){rejected=true;}
  check(rejected,file);
 }
 return failures?1:0;
}
