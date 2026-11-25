"""Every sound parameter may appear once in the UI, apart from the listed, intended exceptions."""
import collections,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from surface_spec import VIEWS,GROUPS
alias={'Home Master':'volume','Home Cutoff':'filter_1_cutoff','Home Resonance':'filter_1_resonance','Home Attack':'env_1_attack','Home Release':'env_1_release','Home Unison':'osc_1_unison_voices','Home Detune':'osc_1_unison_detune','Home Filter':'filter_1_on'}
use=collections.defaultdict(list)
for v in VIEWS:
    if v['group'] in (3,4) and v['member']>1:continue  # ENV/LFO pages share one proxy view
    for s in v['sections']:
        for item in s['items']:
            if item:use[alias.get(item,item)].append((v['title'],s['title'],v['chart']))   # '' = an unused knob of a bank
    for item in v.get('extra',[]):use[alias.get(item,item)].append((v['title'],'touch only',v['chart']))
def allowed(key,places):
    rest=[p for p in places if not (p[0]=='Home' and p[1]=='PLAY')]           # HOME shortcuts
    rest=[p for p in rest if p[2]!='oscmix']                                   # OSC overview repeats four controls per source
    if key.endswith('_wave_frame'):return len({p[0] for p in rest})==1          # slider under each graph
    if key.endswith('_on'):rest=[p for p in rest if p[2]!='effects']            # FX overview switches
    if key in ['Point','Point Count','Point Time','Point Level','Point Curve','Smooth Curve','Route Select']:return len(rest)<=2
    return len(rest)<=1
errors={k:v for k,v in use.items() if len(v)>1 and not allowed(k,v)}
assert not errors,errors
same=[(v['title'],s['title']) for v in VIEWS for s in v['sections'] if len(set(i for i in s['items'] if i))!=len([i for i in s['items'] if i])]
assert not same,same
print(f'PASS: {len(use)} shown parameters, none duplicated outside the intended shortcuts')
