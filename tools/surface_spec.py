"""One navigation/parameter model shared by native UI generation and Q-Link DSP.

Rule (user request 2026-11-13): every sound parameter appears exactly once.
Intended exceptions only: the HOME > PLAY shortcuts and the HOME filter button,
the FX overview switches, the wave-frame slider under each oscillator graph,
the curve point editor shared by LFO and modulation-route curves, and the
OSC overview (user request 2026-11-19), which repeats level, frame, pitch and
pan of the three oscillators and the sample on one page.
Both filters share one page since 0.3.30 (user request 2026-11-20).
The sample has one page without sections since 0.3.31 (user request 2026-11-20).
So has each envelope since 0.3.33 (user request 2026-11-20).
Each LFO has one page since 0.3.35; it shows its controls in two pages, PAGE 1 and PAGE 2, since 0.3.42 (user requests 2026-11-20).
Each effect has one page since 0.3.36; the FX overview shows the chain in its order (user request 2026-11-20).
Random, expression and the route curve share HOME > OTHER since 0.3.38 (user request 2026-11-20).
Each oscillator page shows its controls in two pages, PAGE 1 and PAGE 2, since 0.3.41 (user request 2026-11-20).
tools/check_duplicates.py enforces this."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parent
CAT=json.loads((ROOT/'catalog.json').read_text())['controls']
GROUPS=['HOME','OSC','FILTER','ENV','LFO','FX','MOD']   # the last one was OTHER up to 0.3.37
VIEWS=[]
def add(group,title,tab,sections,color='osc',chart=None,member=0,hidden=False,order=0,extra=()):
    # order: position of the page's tab inside its group (lower = further left); pages keep
    # the number of their place in this list whatever the order of the tabs.
    # extra: controls shown on the page without a Q-Link (touch only).
    VIEWS.append(dict(group=group,title=title,tab=tab,sections=[dict(title=name,items=items) for name,items in sections],color=color,chart=chart,member=member,hidden=hidden,order=order,extra=list(extra)))
# Modulation targets grouped for the two-step picker (group page -> entry page).
import re as _re
DESTINATIONS=json.loads((ROOT/'catalog.json').read_text())['destinations']
def _target_groups():
    def where(prefixes):return [d for d in DESTINATIONS if d.startswith(tuple(prefixes))]
    groups=[('OSC 1',where(['osc_1_'])),('OSC 2',where(['osc_2_'])),('OSC 3',where(['osc_3_'])),('SAMPLE',where(['sample_'])),
        ('FILTER 1',where(['filter_1_'])),('FILTER 2',where(['filter_2_'])),('FX FILTER',where(['filter_fx_'])),
        ('ENV 1-3',where(['env_1_','env_2_','env_3_'])),('ENV 4-6',where(['env_4_','env_5_','env_6_'])),
        ('LFO 1-2',where(['lfo_1_','lfo_2_'])),('LFO 3-4',where(['lfo_3_','lfo_4_'])),('LFO 5-6',where(['lfo_5_','lfo_6_'])),('LFO 7-8',where(['lfo_7_','lfo_8_'])),
        ('RANDOM',where(['random_'])),('CHORUS',where(['chorus_'])),('COMP',where(['compressor_'])),('DELAY',where(['delay_'])),('DISTORT',where(['distortion_'])),
        ('EQ',where(['eq_'])),('FLANGER',where(['flanger_'])),('PHASER',where(['phaser_'])),('REVERB',where(['reverb_']))]
    number=lambda d:int(_re.match(r'modulation_(\d+)_',d)[1])
    mods=sorted(where(['modulation_']),key=lambda d:(number(d),d))
    for first in range(1,65,16):groups.append((f'MOD {first}-{first+15}',[d for d in mods if first<=number(d)<first+16]))
    used={d for _,items in groups for d in items}
    groups.insert(0,('GLOBAL',[d for d in DESTINATIONS if d not in used]))
    assert sum(len(items) for _,items in groups)==len(DESTINATIONS) and all(0<len(items)<=32 for _,items in groups),[(n,len(i)) for n,i in groups]
    return groups
TARGET_GROUPS=_target_groups()
def keys(prefix):return [c['id'] for c in CAT if c['id'].startswith(prefix)]
def sections(first,rest,name='DETAILS'):
    result=[('SOUND',first)];result += [(name if n==0 else name+' '+str(n//8+1),rest[n:n+8]) for n in range(0,len(rest),8)];return result
# 0.3.27 (user request): HOME names the sound and its author and holds what is played, the
# sound's four macros, cutoff, resonance, the filter switch, the volume and the four stages of
# the amp envelope. Voices, unison and detune left it (VOICES page, OSC > UNISON).
# One Q-Link group each: macros / filter and volume / envelope.
add(0,'Home','HOME',[('PLAY',['macro_control_1','macro_control_2','macro_control_3','macro_control_4',
    'Home Cutoff','Home Resonance','Home Filter','Home Master','Home Attack','env_1_decay','env_1_sustain','Home Release'])],chart='home')
add(0,'Voices','VOICES',[('PLAY',['Voices','voice_priority','voice_override','legato','portamento_time','portamento_force','portamento_scale','portamento_slope']),('EXPRESSION',['velocity_track','pitch_bend_range','mpe_enabled','Lower MPE Members','Upper MPE Members','oversampling','stereo_mode','stereo_routing'])],'env')
# 0.3.40 (user: ADVANCED was nearly empty, OTHER overfilled with two Q-Link layers): ADVANCED
# also holds the curve of a route with its point editor; OTHER keeps random and expression.
# One Q-Link layer each, one knob bank per group.
add(0,'Advanced','ADVANCED',[('PARAMETERS',['Parameter','Value','Live Graphics','','Route Select','Curve','Point Count','Smooth Curve','Point','Point Time','Point Level','Point Curve'])],chart='advanced')
add(0,'Library','LIBRARY',[('BANKS',[])],chart='library')
# Group 1 holds what is played most and gets the large dials: level, frame, spectral, warp.
OSC_MAIN=['level','wave_frame','spectral_morph_amount','distortion_amount','pan','transpose','tune','distortion_phase',
          'unison_voices','unison_detune','phase','random_phase','spectral_morph_type','distortion_type','destination','unison_blend']
OSC_MORE=['stereo_spread','stack_style','detune_power','detune_range','frame_spread','spectral_unison','distortion_spread','spectral_morph_spread',
          'transpose_quantize','midi_track','smooth_interpolation']
for o in range(1,4):
    # Medium 16:9 graph top-left, two controls and the frame slider beside it,
    # a full row of four below: up to six controls per section (0.3.7 compromise).
    # 0.3.29 (user request): no sub pages. Every control of the oscillator is on its one page,
    # laid out after Vital's own oscillator strip. Sixteen are on the Q-Links in four groups
    # (mix and pitch / wave / unison and phase / types and routing); the eleven fine-tuning
    # controls and the on switch are on the page for the finger only. The stepping table
    # selector left the page: TABLE LIST opens the list of tables.
    p=f'osc_{o}_'
    assert set(keys(p))-{p+'view_2d'}=={p+k for k in OSC_MAIN+OSC_MORE+['on']},set(keys(p))^{p+k for k in OSC_MAIN+OSC_MORE+['on']}
    # Later the same day (user: the small controls cannot be reached by Q-Link): the page had two
    # Q-Link layers, MAIN and MORE, with every control on screen in both.
    # 0.3.41 (user request): two pages of controls instead of two Q-Link layers. PAGE 1 shows
    # the sixteen main controls, PAGE 2 the eleven others; the wave picture, the on switch and
    # TABLE LIST stay. The two buttons stand on the page where the layer buttons stood, NOT in
    # the tab row (user, same day: three more tabs cluttered it).
    add(1,f'Oscillator {o}',f'OSC {o}',[('MAIN',[p+k for k in OSC_MAIN]),('MORE',[p+k for k in OSC_MORE])],chart=o-1,member=o,extra=[p+'on'])
# 0.3.31 (user request): the sample on one page, no PLAY and ROUTING sections: picture of the
# sample, SAMPLE LIST, every control. Ten are on the Q-Links in three groups (level, pan,
# pitch, tune / loop, bounce, key tracking, random phase / path, pitch quantise); the on switch
# is on the page for the finger, as on an oscillator page. The stepping file selector left the
# page: SAMPLE LIST opens the list of samples.
SAMPLE_ITEMS=['sample_level','sample_pan','sample_transpose','sample_tune','sample_loop','sample_bounce','sample_keytrack','sample_random_phase','sample_destination','sample_transpose_quantize']
assert set(keys('sample_'))=={*SAMPLE_ITEMS,'sample_on'},keys('sample_')
add(1,'Sample','SAMPLE',[('MAIN',SAMPLE_ITEMS)],chart='sample',extra=['sample_on'])
# 0.3.30 (user request): both filters on one page, each with a picture of its response, the
# controls in three sizes as on the oscillator page. Two Q-Link layers of sixteen: in each,
# the first two knob groups belong to filter 1 and the last two to filter 2.
# 0.3.46 (user request): two pages of controls, as on the oscillator and LFO pages. PAGE 1
# holds what a filter is set with first, the model and the style among it: cutoff, resonance,
# model, style / drive, mix, blend, key tracking. PAGE 2 holds the rest.
FILTER_MAIN=['cutoff','resonance','model','style','drive','mix','blend','keytrack']
FILTER_MORE=['blend_transpose','formant_x','formant_y','formant_transpose','formant_resonance','formant_spread','filter_input','on']
for f in range(1,3):assert set(keys(f'filter_{f}_'))=={f'filter_{f}_{k}' for k in FILTER_MAIN+FILTER_MORE},keys(f'filter_{f}_')
add(2,'Filters','FILTERS',[('MAIN',[f'filter_{f}_{k}' for f in (1,2) for k in FILTER_MAIN]),('MORE',[f'filter_{f}_{k}' for f in (1,2) for k in FILTER_MORE])],'flt',chart='filters')
# FILTER 2 was this page up to 0.3.29. The place stays in the list so that no later page
# changes its number; the plugin sends whoever lands here to the page above.
add(2,'Filters','',[('-',[])],'flt',chart='retired',hidden=True);VIEWS[-1]['redirect']=8
# 0.3.33 (user request): an envelope on one page, no ADSR / TIME / CURVE sections. All nine
# controls are dials on the Q-Links: attack, decay, sustain, release / delay, hold, attack
# curve, decay curve / release curve.
ENV_ITEMS=['Env attack','Env decay','Env sustain','Env release','Env delay','Env hold','Env attack_power','Env decay_power','Env release_power']
for e in range(1,7):add(3,f'Envelope {e}',f'ENV {e}',[('MAIN',ENV_ITEMS)],'env','env',e)
# 0.3.35 (user request): an LFO on one page, no TIMING / PHASE / SHAPE / POINT / SMOOTH
# sections, a narrower picture, frequency and shape as the two large dials. Twenty controls
# in two Q-Link layers. First: rate and shape with tempo and sync / mode, phase, stereo, delay /
# fade and smoothing. Second: the point editor / point count, key tracking, shape file.
# (The first build had sixteen and four; with four, three of the four knob groups of the
# second layer were empty and the user found it empty.)
# 0.3.42 (user request): as on an oscillator page, two pages of controls instead of two Q-Link
# layers over one crowded page. PAGE 1 holds what sound design needs most, one knob bank per
# group: rate (frequency, shape, tempo, sync) / mode, phase, stereo, delay / fade, smoothing
# and its time. PAGE 2 holds the tweaks: the point editor / point count, curve smoothing and
# the shape file / key tracking. '' is an unused knob of a bank.
LFO_MAIN=['LFO frequency','LFO Shape','LFO tempo','LFO sync','LFO sync_type','LFO phase','LFO stereo','LFO delay_time',
          'LFO fade_time','LFO smooth_mode','LFO smooth_time']
LFO_MORE=['Point','Point Time','Point Level','Point Curve','Point Count','Smooth Curve','LFO File','','LFO keytrack_transpose','LFO keytrack_tune']
for l in range(1,9):add(4,f'LFO {l}',f'LFO {l}',[('MAIN',LFO_MAIN),('MORE',LFO_MORE)],'mod','lfo',l)
effects=['distortion','chorus','flanger','phaser','delay','reverb','compressor','eq','filter_fx']
# 0.3.36 (user request): no second page anywhere in FX. The overview shows the nine effects in
# the order of the chain, each place with the name of its effect and that effect's switch; a
# place is chosen with its number and moved with two buttons. The Q-Links are the nine switches.
add(5,'Effects overview','OVERVIEW',[('CHAIN',[f'FX Slot {i}' for i in range(1,10)])],'fx',chart='effects')
# Each effect on one page. Per effect: Q-Link layers, in each layer rows of groups (title,
# controls with their size). A group holds controls that belong together and is one knob
# bank, so it has at most four controls and a layer at most four groups; a bank that a group
# does not fill leaves its last knobs unused ('' in the list of the page). Sizes (user,
# 0.3.36): large wherever the page has room, smaller only where it has not: a row holds seven
# large dials and a page two rows. Only the compressor needs a second layer; the on switch of
# an effect is on its page for the finger, as on an oscillator page.
B,M,S='big','mid','switch'
def _bands(size,keys):return [[(band.upper() if band!='band' else 'MID',[(f'{band}_{key}',kind) for key,kind in keys]) for band in ['low','band','high']]]
FX_PAGES={
 'distortion':[[[('DRIVE',[('drive',B),('mix',B),('type',B)]),('FILTER',[('filter_order',B),('filter_cutoff',B),('filter_resonance',B),('filter_blend',B)])]]],
 'chorus':[[[('CHORUS',[('dry_wet',B),('mod_depth',B),('feedback',B),('voices',B)]),('RATE',[('frequency',B),('sync',B),('tempo',B)])],
            [('DELAYS AND TONE',[('delay_1',B),('delay_2',B),('spread',B),('cutoff',B)])]]],
 'flanger':[[[('FLANGER',[('dry_wet',B),('mod_depth',B),('feedback',B)]),('TONE',[('center',B)])],
             [('RATE AND PHASE',[('frequency',B),('sync',B),('tempo',B),('phase_offset',B)])]]],
 'phaser':[[[('PHASER',[('dry_wet',B),('mod_depth',B),('feedback',B)]),('TONE',[('center',B),('blend',B)])],
            [('RATE AND PHASE',[('frequency',B),('sync',B),('tempo',B),('phase_offset',B)])]]],
 'delay':[[[('DELAY',[('dry_wet',B),('feedback',B),('style',B)]),('FILTER',[('filter_cutoff',B),('filter_spread',B)])],
           [('TIME',[('frequency',B),('sync',B),('tempo',B)]),('SECOND TIME',[('aux_frequency',B),('aux_sync',B),('aux_tempo',B)])]]],
 'reverb':[[[('REVERB',[('dry_wet',B),('decay_time',B),('size',B),('delay',B)]),('CHORUS',[('chorus_amount',B),('chorus_frequency',B)])],
            [('INPUT CUTS',[('pre_low_cutoff',B),('pre_high_cutoff',B)]),('SHELVES',[('low_shelf_cutoff',B),('low_shelf_gain',B),('high_shelf_cutoff',B),('high_shelf_gain',B)])]]],
 'compressor':[[[('COMPRESSOR',[('mix',B),('attack',B),('release',B),('enabled_bands',B)]),('GAIN',[('low_gain',B),('band_gain',B),('high_gain',B)])]],
               _bands(M,[('upper_threshold',M),('lower_threshold',M),('upper_ratio',M),('lower_ratio',M)])],
 'eq':[[_bands(B,[('gain',B),('cutoff',B),('resonance',B),('mode',S)])[0][:2],_bands(B,[('gain',B),('cutoff',B),('resonance',B),('mode',S)])[0][2:]]],
 'filter_fx':[[[('FILTER',[('cutoff',B),('resonance',B),('drive',B),('mix',B)]),('TYPE',[('model',M),('style',M),('blend',M),('keytrack',M)])],
               [('FORMANT',[('formant_x',B),('formant_y',B),('formant_transpose',B),('formant_resonance',B)]),('',[('formant_spread',B),('blend_transpose',B)])]]],
}
for effect in effects:
    titles={'filter_fx':'FX FILTER','compressor':'COMP','distortion':'DISTORT'}
    layers=FX_PAGES[effect];sections=[]
    for number,rows in enumerate(layers):
        groups=[group for row in rows for group in row];assert len(groups)<=4 and all(1<=len(items)<=4 for _,items in groups),effect
        slots=[]
        for _,items in groups:slots+=[effect+'_'+key for key,_ in items]+['']*(4-len(items))
        while slots and slots[-1]=='':slots.pop()
        sections.append((['MAIN','MORE'][number],slots))
    used=[name for _,slots in sections for name in slots if name]
    assert sorted(used+[effect+'_on'])==sorted(keys(effect+'_')),(effect,set(used+[effect+'_on'])^set(keys(effect+'_')))
    add(5,effect.replace('_',' ').title(),titles.get(effect,effect.upper()),sections,'fx',chart='fx',extra=[effect+'_on'])
    VIEWS[-1]['effect']=effect
# Serum-style table (user request): eight routes per page, source - amount - target.
# The Q-Links of a page are the eight amounts.
add(6,'Matrix','MATRIX',[(f'{page*8+1}-{page*8+8}',[f'modulation_{route}_amount' for route in range(page*8+1,page*8+9)]) for page in range(8)],'mod','mxtable',1)
add(6,'Other','',[('-',[])],'mod',chart='retired',hidden=True);VIEWS[-1]['redirect']=37   # RANDOM up to 0.3.37
add(6,'Other','',[('-',[])],'mod',chart='retired',hidden=True);VIEWS[-1]['redirect']=37   # EXPRESSION up to 0.3.37
# No separate routing page: each of its controls already lives in its own
# module (the oscillator pages, SAMPLE, FILTERS, FX FILTER, VOICES).
# 0.3.38 (user request): the last group is MOD and holds the matrix. The random sources, the
# expression controls and the curve of a route share ONE page, HOME > OTHER. It stands where
# MOD CURVE stood in this list (page 37), so later pages keep their numbers; RANDOM and
# EXPRESSION above are retired places that open it. Two Q-Link layers, one knob bank per
# group: random / its style / key tracking / expression, and the route curve / its point.
OTHER_MAIN=['Random Select','Random frequency','Random sync','Random tempo','Random style','Random stereo','Random sync_type','',
            'Random keytrack_transpose','Random keytrack_tune','','','mod_wheel','pitch_wheel','voice_amplitude']
add(0,'Other','OTHER',[('MAIN',OTHER_MAIN)],'mod',chart='other')
# Hidden helper views of the picker; reached only through PICK on a matrix row. 0.3.43 (user:
# there is no way to pick the source; one page, the same PICK button): the first one holds the
# sources of the route above the groups of targets.
add(6,'Target group','',[('GROUPS',[])],'mod','pickgroup',hidden=True)
add(6,'Target','',[('-',[])]+[(name,[]) for name,_ in TARGET_GROUPS],'mod','picktarget',hidden=True)
# 0.3.26 (user request): all three oscillators and the sample on one page. Appended last, so
# no existing page changes its number (tests, the page buttons' step counts and the plugin's
# own page numbers refer to them); 'order' puts its tab at the left of the OSC row.
# One Q-Link group per row: the four levels, frame x3 + sample pan, the four pitches.
add(1,'Osc overview','OVERVIEW',[('MIX',['osc_1_level','osc_2_level','osc_3_level','sample_level',
    'osc_1_wave_frame','osc_2_wave_frame','osc_3_wave_frame','sample_pan',
    'osc_1_transpose','osc_2_transpose','osc_3_transpose','sample_transpose'])],chart='oscmix',order=-1)
# 0.3.44: the page of SAVE SOUND (name entry). Hidden, reached with SAVE SOUND on HOME;
# appended, so no page changes its number.
add(0,'Save sound','',[('-',[])],'osc','savesound',hidden=True)
# 0.3.52: the page of the 32 factory wavetables, a table of small pictures with names. Hidden,
# reached with a tap on a wave picture of the OSC overview; appended, so no page changes its
# number. The names are those of the files in Content/Wavetables/Factory ("NN name.wav").
FACTORY_TABLES=['Classic Shapes','Saw Filter','Pulse Width','Sync Saw','Sync Formant','Saw Stack','Square Stack','Reso Saw',
    'Triangle Fold','Sine Drive','Vector Bell','Vector Voice','Vector Organ','Vector Glass','Vector Reed','Vector Wood',
    'Vector Digital','FM Brass','FM Bell','Phase Bend','Trans Resonance','Trans Scanner','Trans Vocal','Trans Crystal',
    'Trans Comb','Odd Even','Pluck Decay','Trans Phaser','Growl','Stairs','Chord Stack','Static']
add(1,'Wavetables','',[('-',[])],'osc','tables',hidden=True)
# The section a modulation source or target belongs to, for its colour in the matrix and in
# the pickers: 0 none, 1 OSC (with the sample), 2 FILTER, 3 ENV, 4 LFO, 5 FX. The plugin has
# the same rule (Vitaleon::tintOf).
def tint_of(name):
    if name.startswith(('osc_','sample_')):return 1
    if name.startswith(('filter_1_','filter_2_')):return 2
    if name.startswith('env_'):return 3
    if name.startswith('lfo_'):return 4
    if name.startswith(('chorus_','compressor_','delay_','distortion_','eq_','flanger_','phaser_','reverb_','filter_fx_')):return 5
    return 0
def write_header():
    payload=json.dumps(VIEWS,separators=(',',':'))
    (ROOT.parent/'src/SurfaceMap.h').write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n// Generated by tools/surface_spec.py; do not edit.\n#pragma once\nstatic const char* surfaceMapJson=R"SURFACE('+payload+')SURFACE";\n',encoding='utf-8')
if __name__=='__main__':write_header();print(len(VIEWS),'module views across',len(GROUPS),'groups')
