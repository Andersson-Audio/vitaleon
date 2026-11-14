// SPDX-License-Identifier: GPL-3.0-or-later
// Compile Engine.cpp in this TU to compare private sample-preparation kernels.
#include "../src/Engine.cpp"
#include <iostream>
#include <cstring>
int main(){
 int checked=0,failures=0;
 for(int size:{4,5,7,13,26,27,28,53,54,55,56,63,64,65,127,128,257,1024,44100}){
  std::vector<float> buffer(size);uint32_t seed=17;
  for(float& x:buffer){seed=seed*1664525u+1013904223u;x=float(seed>>8)/16777216.0f-.5f;}
  for(int index=0;index<size;++index){
   // Original Vital 1.0.6 wrap formula, independent of the optimized branch.
   int radius=vital::SampleSource::kNumDownsampleTaps/2;float expected=0;
   for(int i=index-radius;i<=index+radius;++i)expected+=vital::kDownsampleCoefficients[i-index+radius]*buffer[(i+size*radius)%size];
   float actual=vital::getFilteredLoopSample(buffer.data(),index,size);
   if(std::memcmp(&expected,&actual,sizeof(float))!=0)++failures;
   ++checked;
  }
 }
 std::cout<<(failures?"FAIL ":"PASS ")<<"sample loop filter matches original bit-for-bit: "<<checked<<" positions, "<<failures<<" differences\n";
 return failures?1:0;
}
