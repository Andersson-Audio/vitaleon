from pathlib import Path
import json,re
r=Path(__file__).resolve().parent;p=r.parent;m=json.loads((r/'metadata.json').read_text())
strings=(p/'src/engine/synth_strings.h').read_text();arrays={}
for name,body in re.findall(r'const std::string\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\};',strings,re.S):
 arrays[name]=re.findall(r'"([^"\n]*)"',body)
params=(p/'src/engine/synth_parameters.cpp').read_text();groups={}
# Repeated field names (style, sync_type, stereo) belong to different groups.
# Flattening them silently replaced LFO/Random labels with filter labels.
for group,body in re.findall(r'ValueDetailsLookup::(\w+)\[\]\s*=\s*\{(.*?)\n  \};',params,re.S):
 lookups={}
 for name,entry in re.findall(r'\{\s*"([^"]+)"\s*,(.*?)\}',body,re.S):
  found=re.search(r'strings::(\w+)',entry)
  if found:lookups[name]=found[1]
 groups[group]=lookups
prefix_groups={'osc':'osc_parameter_list','env':'env_parameter_list','lfo':'lfo_parameter_list','filter':'filter_parameter_list','random':'random_lfo_parameter_list','modulation':'mod_parameter_list'}
for c in m['controls']:
 if c['id'].startswith('osc_') and c['id'].endswith('_stack_style'):c['max']=12
 if c['id'].startswith('lfo_') and c['id'].endswith('_sync_type'):c['max']=6
 match=re.match(r'^(osc|env|lfo|filter|random|modulation)_(?:\d+|fx)_(.+)$',c['id'])
 lookup=groups[prefix_groups[match[1]]].get(match[2]) if match else groups['parameter_list'].get(c['id'])
 if lookup in arrays:
  labels=arrays[lookup];a=int(c['min']);b=int(c['max'])
  c['choices']=[labels[i] if i<len(labels) else str(i) for i in range(a,b+1)]
 if c['id'].endswith('_view_2d'):c['choices']=['3D','2D','Spectrum']
 # Upstream destination max is one past the last valid enum. Expose only actual routes.
 if c['id'].endswith('_destination') and c['id'].startswith(('osc_','sample_')):
  c['max']=len(arrays['kDestinationNames'])-1;c['choices']=arrays['kDestinationNames']
 if c['id']=='oversampling':c['default']=0
 if c['id']=='volume':c['default']=5476 # -6 dB init level (0.3.57; it was 3136 = -24 dB); imported levels remain untouched.
 if c['id']=='voice_override':c['default']=1 # Vital kSteal: retain the verified MPC fixed-limit policy.
 # Real saved Vital controls can slightly exceed their displayed UI ranges.
 # Keep those numerical values intact instead of clamping imported sound settings.
 if c['id'].startswith('compressor_') and c['id'].endswith('_ratio'):
  if c['min']<0:c['min']=-1.01
  c['max']=1.01
 if c['id']=='eq_high_gain':c['max']=16.1
(r/'catalog.json').write_text(json.dumps(m,indent=2))
(p/'src/ControlCatalog.h').write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n// Generated from the pinned Vital parameter definitions.\n#pragma once\ninline constexpr const char* kControlCatalog=R"VITALEON('+json.dumps(m,separators=(',',':'))+')VITALEON";\n')
print(len(m['controls']),'controls',len(arrays),'label tables')
