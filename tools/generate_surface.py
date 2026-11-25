"""Generate native MPC skins and a source-derived layout preview, no web runtime."""
from pathlib import Path
import json,sys,math,re,hashlib
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parent; project=root.parent; sdk=project.parents[1]
sys.path.insert(0,str(sdk/'tools'));import gen_ui as g
g.FONT_NAME='Roboto'
# Live II hardware drops unbound Text labels, while bound Name labels render.
# Use the proven WithName path for every fixed caption, including nested modes.
sdk_label_data=g.label_data
def native_label_data(label_type,colour,height=30.0,justification='left',text=None):
    result=sdk_label_data('Name' if label_type=='Text' else label_type,colour,height,justification)
    if label_type=='Text':
        assert text is not None and ']' not in text
        return g.Obj([(key,'WithName['+text+']/Parameter 0' if key=='handleName' else value) for key,value in result])
    return result
g.label_data=native_label_data
skin=project/'skins';skin.mkdir(exist_ok=True)
params=json.loads((project/'build/params.json').read_text())['params']; byname={p['name']:p for p in params}
# The build's parameter dump holds the values of whatever sound the plugin had loaded
# when it was made (since 0.3.20 the first installed sound). Value and text of every
# parameter therefore come from tools/init-values.json: the Init state of a plugin
# without sounds, written by tests/run_full_checks.py. The skin no longer depends on
# the installed sounds.
init_values=json.loads((root/'init-values.json').read_text())
assert len(init_values)==len(params),'tools/init-values.json is stale: run tests/run_full_checks.py and copy build/full-checks/init-values.json'
for p,v in zip(params,init_values):
    assert v['index']==p['index'] and v['name']==p['name'],(p['name'],v['name'])
    p['default']=v['value'];p['display']=v['display']
catalog_data=json.loads((root/'catalog.json').read_text());catalog=catalog_data['controls']
catalog_by_id={c['id']:c for c in catalog}
# The host metadata dump omits very long choice lists. The pinned engine catalog
# still provides their complete, ordered labels for the native menu overlay.
def target_name(d):
    d=re.sub(r'^modulation_(\d+)_amount$',r'mod_\1_amount',d);d=re.sub(r'^modulation_(\d+)_power$',r'mod_\1_curve',d)
    d=d.replace('spectral_morph','spectral').replace('distortion_','warp_',1) if d.startswith('osc_') else d
    d=d.replace('keytrack_transpose','key_transp').replace('keytrack_tune','key_tune').replace('formant_','form_').replace('filter_fx','fx_filter').replace('macro_control','macro')
    return ' '.join(w.upper() if w in ('osc','lfo','env','fx','eq') else w.capitalize() for w in d.split('_'))
def source_name(s):
    if s in ('None','none'):return 'None'
    return ' '.join(w.upper() if w in ('lfo','env') else w.capitalize() for w in s.replace('macro_control','macro').split('_'))
for p in params:
    key=p['name'].replace(' ','_')
    for title,prefix in [('LFO ','lfo_1_'),('Random ','random_1_'),('Env ','env_1_')]:
        if p['name'].startswith(title):key=prefix+p['name'][len(title):]
    c=catalog_by_id.get(key)
    if c and c['scale']==0 and c['min']==0 and len(c.get('choices',[]))>1:
        p['choices']=c['choices'];p['kind']='bool' if c['choices']==['Off','On'] else 'choice'
    if p['name']=='Target' or re.fullmatch(r'Route \d+ Target',p['name']):
        p['choices']=['None']+[target_name(d) for d in catalog_data['destinations']];p['kind']='choice'
    if p['name']=='Source' or re.fullmatch(r'Route \d+ Source',p['name']):
        p['choices']=[source_name(c) for c in p['choices']]
    elif p['name']=='Parameter':
        p['choices']=[c['id'] for c in catalog if c['id'] not in ['polyphony','beats_per_minute']];p['kind']='choice'
    elif p['name']=='Value' or (p['name'].startswith('Home ') and p['name']!='Home Filter'):
        # This normalized proxy can edit any parameter; its initial Off display
        # must never turn it into an on/off switch in the generated skin.
        p['kind']='float';p['choices']=[]
    if p['name'].startswith('UI Control '):
        # A Q-Link slot stands for whatever control the visible page puts on it. The parameter
        # dump is made with HOME on screen; since 0.3.27 HOME's seventh Q-Link is the filter
        # switch, so the dump read 'UI Control 7' as an Off/On switch and the skin gave it two
        # labels: on every page the seventh dial showed Off/On and had two steps (user,
        # 0.3.29: Tune is labelled on/off and jumps the pitch). A slot never has labels.
        p['kind']='float';p['choices']=[]
    # Selectors whose entries come from the Content folder or the library at run time
    # (tables, samples, presets, library pages) must never get a fixed label list: the
    # parameter dump only shows what happened to be installed while it was made.
    if p['name'].startswith('Library ') or p['name']=='Preset' or 'Table' in p['name'] or (p['kind']!='bool' and 'File' in p['name']):
        if p['kind']=='bool' and p['name'].startswith('Library ') and p.get('choices')!=['Off','On']:p['kind']='int'  # a 2-entry list is not a switch
        if p.get('choices')!=['Off','On']:p['choices']=[]
    # The library command buttons set themselves back after a tap. The parameter dump
    # therefore reads them as switches or as plain values, depending on timing
    # (0.3.20 lost their labels that way). They are switches.
    if p['name'] in ['Drives','Up Folder','Refresh Drives','Previous Files','Next Files','Installed Banks']+['Open Row %d'%n for n in range(1,7)]:
        p['kind']='bool';p['choices']=['Off','On']
    if p['name'].startswith(('display sound ','display macro name ','display_sound_','display_macro_name_')):p['kind']='float';p['choices']=[]   # texts of the sound: never a label list
    if p['name'] in ['UI Group','UI Page','UI Section','UI Pick','UI Bank','Import Target','UI Browse','UI Env View','UI FX Move','Bank Step']:
        # Navigation state must never get replacementStrings. The parameter
        # dump sweeps UI Section while the 2-section Home view clamps it, so it
        # is misdetected as a 0/1 switch. A 2-string entry makes the MPC treat
        # it as 2 steps while its buttons and IndexedEnabling use 4 (0.3.1:
        # section buttons tappable, content never switched).
        p['kind']='int';p['choices']=[]
BG='ff181a23';PANEL='ff222531';LINE='ff353949';TEXT='ffedf0f7';MUTED='ffa0a7b7'
# 0.3.28, type roles: what is read (values, the sound's name) is bright or in the module's
# colour; what only names a control is quieter, so the values stand out.
NAME='ffc3c9d6'
# One colour per section (user, 0.3.43): OSC violet, FILTER pink, ENV green, LFO and MOD amber,
# FX blue. Up to 0.3.42 the filters shared the blue of the effects.
COLORS={'osc':'ffbca3ff','env':'ff64d9ba','mod':'fff6bf66','fx':'ff7cb9e8','flt':'ffff8fb3'}
def handle(name):
    if name not in byname:
        if name.replace('_',' ') in byname:name=name.replace('_',' ')
        else:raise ValueError('Unknown parameter '+name)
    return 'Parameter '+str(byname[name]['index'])
def index(name):return int(handle(name).split()[-1])
def ent(name,kind,data,bounds,focus=False,remap=()):return g.entry(name,kind,data,' '.join(map(str,bounds)),'Yes' if focus else 'No','Show','Always',remap)

def conditional(entry,handles):
    bounds=dict(entry)['bounds'];data=dict(bounds)
    data['version']=2;data['showWhenDataModelInvalid']='Hide'
    data['additionalInvalidatingHandles']=data.get('additionalInvalidatingHandles',[])+handles
    bounds[:]=list(data.items())
    return entry
def label(text,bounds,color=TEXT,size=22,value=None,justify='left verticallyCentred'):
    return ent(text,'Label',g.label_data('Value' if value else 'Text',color,size,justify,None if value else text),bounds,remap=[('Data',handle(value))] if value else [])
def rect(name,bounds,color):return ent(name,'Image',{'version':1,'colour':color,'image':''},bounds)
def film(name,bounds,param,color,bank,bankparam):
    data={'version':2,'imageType':'FilmStrip','colour':'0','image':f'segment-{color}-{bank}.png','numFrames':256,'handleName':handle(param),'animated':False}
    return g.entry(name,'Image',data,' '.join(map(str,bounds)),'No','Hide','Always',invalidating=[f'IndexedEnabling/{bank}/4/'+handle(bankparam)])
def graph(children,base,x=32,y=104,w=1216,h=156,color='osc'):
    children.append(rect('graph background',(x-8,y-4,w+16,h+8),'ff1c1f29'))
    for n in range(5):children.append(rect('horizontal grid',(x,y+n*h//4,w,1),'ff303441'))
    for n in range(9):children.append(rect('vertical grid',(x+n*w//8,y,1,h),'ff292d39'))
    for i in range(96):
        a=round(x+i*w/96);b=round(x+(i+1)*w/96)
        bankbase=base.replace('display_wave_','display_wave_bank_').replace('display_lfo_curve_','display_lfo_bank_')
        for bank in range(4):children.append(film('curve segment '+str(i),(a,y,b-a,h),base+str(i),color,bank,bankbase+str(i)))
    if base=='display_lfo_curve_':
        data={'version':2,'imageType':'FilmStrip','colour':'0','image':'lfo-playhead.png','numFrames':512,'handleName':handle('display_lfo_phase'),'animated':False}
        children.append(g.entry('LFO playhead','Image',data,f'{x} {y} {w} {h}','No','Hide','Always',invalidating=['IndexedEnabling/0/2/'+handle('Curve Target')]))
GRAPH=dict((k,int(v)) for k,v in re.findall(r'(kWaveColumns|kWaveLevels|kCurveColumns|kCurveLevels)=(\d+)',(project/'src/FullSynth.h').read_text(encoding='utf-8')))
assert len(GRAPH)==4,GRAPH
FILTER_GRAPH=dict((k,int(v)) for k,v in re.findall(r'(kFilterColumns|kFilterLevels)=(\d+)',(project/'src/FullSynth.h').read_text(encoding='utf-8')))
FILTER_DB=dict((k,float(v)) for k,v in re.findall(r'(kFilterTopDb|kFilterFloorDb)=(-?[\d.]+)f',(project/'src/FullSynth.h').read_text(encoding='utf-8')))
assert len(FILTER_GRAPH)==2 and len(FILTER_DB)==2,(FILTER_GRAPH,FILTER_DB)
ENV_COLUMNS=int(re.search(r'kEnvColumns=(\d+)',(project/'src/FullSynth.h').read_text(encoding='utf-8'))[1])
def wave_bars(children,chart,x,y,w,h,step=1):
    # 0.3.32 (user: less pixelated): 256 columns of 2 px on an oscillator page (was 128 of
    # 4 px since 0.3.18, before that 64). The small pictures of the OSC overview show every
    # second column (step=2), so that page keeps the number of picture parts it had.
    # The number of columns costs no host notifications: the plugin asks for ONE display
    # update per picture (see notifyVisuals in FullRuntime.inc), and none while a control moves.
    columns,levels=GRAPH['kWaveColumns'],GRAPH['kWaveLevels'];shown=columns//step;assert columns%step==0 and w%shown==0;cw=w//shown
    name=f'wavebar-{cw}x{h}-{levels}.png'
    children.append(rect('graph background',(x-8,y-4,w+16,h+8),'ff1c1f29'))
    children.append(rect('zero line',(x,y+h//2,w,1),'ff353949'))
    for n in range(shown):
        i=n*step
        param=f'display_wave_{chart}_{i}' if i<96 else f'display_wave_bank_{chart}_{i-96}' if i<192 else f'display_wave_fine_{chart}_{i-192}'
        data={'version':2,'imageType':'FilmStrip','colour':'0','image':name,'numFrames':levels,'handleName':handle(param),'animated':False}
        children.append(g.entry('wave column '+str(n),'Image',data,f'{x+n*cw} {y} {cw} {h}','No','Show','Always'))
    # The line is a little thicker on the large picture, so that neighbouring columns of a
    # steep edge still touch.
    thick=4 if h>=288 else 3
    strip=Image.new('RGBA',(cw,h*levels));d=ImageDraw.Draw(strip);rgb=tuple(bytes.fromhex(COLORS['osc'][2:]))
    for k in range(levels):
        top=k*h;level=round((1-k/(levels-1))*(h-thick));mid=h//2
        d.rectangle((0,top+min(level,mid),cw-1,top+max(level+thick-1,mid)),fill=rgb+(70,))
        d.rectangle((0,top+level,cw-1,top+level+thick-1),fill=rgb+(255,))
    strip.save(skin/name)
def curve_bars(children,x,y,w,h,step=1):
    # LFO / route curve: 384 columns and 108 levels. The route curve page shows all of them at
    # 3 px; the LFO page (0.3.35, user: the picture was too wide) shows every second one at
    # 3 px, 576 px wide and as high as the wave picture of an oscillator page.
    columns,levels=GRAPH['kCurveColumns'],GRAPH['kCurveLevels'];shown=columns//step;assert columns%step==0 and w%shown==0;cw=w//shown
    assert h*levels<=31104,'picture strip taller than anything shown on the device so far'
    children.append(rect('graph background',(x-8,y-4,w+16,h+8),'ff1c1f29'))
    name=f'curvebar-{cw}x{h}-{levels}.png'
    for n in range(shown):
        i=n*step
        param=f'display_lfo_curve_{i}' if i<96 else f'display_lfo_bank_{i-96}' if i<192 else f'display_lfo_fine_{i-192}'
        data={'version':2,'imageType':'FilmStrip','colour':'0','image':name,'numFrames':levels,'handleName':handle(param),'animated':False}
        children.append(g.entry('curve column '+str(n),'Image',data,f'{x+n*cw} {y} {cw} {h}','No','Show','Always'))
    # The playhead: one strip per picture width, so that its mark keeps its thickness.
    playhead='lfo-playhead.png' if w==1152 else f'lfo-playhead-{w}.png'
    if w!=1152:
        marks=Image.new('RGBA',(w,4*512));md=ImageDraw.Draw(marks)
        for frame in range(512):mx=round(frame*(w-3)/511);md.rectangle((mx,frame*4,mx+2,frame*4+3),fill='#edf0f7')
        marks.save(skin/playhead)
    data={'version':2,'imageType':'FilmStrip','colour':'0','image':playhead,'numFrames':512,'handleName':handle('display_lfo_phase'),'animated':False}
    children.append(g.entry('LFO playhead','Image',data,f'{x} {y} {w} {h}','No','Hide','Always',invalidating=['IndexedEnabling/0/2/'+handle('Curve Target')]))
    thick=4 if h>=288 else 3
    strip=Image.new('RGBA',(cw,h*levels));d=ImageDraw.Draw(strip);rgb=tuple(bytes.fromhex(COLORS['mod'][2:]))
    for k in range(levels):
        top=k*h;level=round((1-k/(levels-1))*(h-thick))
        d.rectangle((0,top+level,cw-1,top+h-1),fill=rgb+(55,))
        d.rectangle((0,top+level,cw-1,top+level+thick-1),fill=rgb+(255,))
    strip.save(skin/name)
def trace(name,bounds,param,color):
    c=COLORS[color]
    return ent(name,'Plotter',{'version':2,'backgroundColour':'ff1c1f29','drawLine':True,'lineDirection':'Up','tightClip':True,'lineColourStart':c,'lineColourEnd':c,'horizLineGradient':False,'lineWidth':2.0,'fillColourStart':'1064d9ba','fillColourEnd':'0064d9ba','horizFillGradient':False,'fillZero':0.0,'dataHandle':handle(param),'drawHistory':True,'historyDirection':'Left','historyColour1':c,'historyColour2':c,'historyTime':3.0,'historySkipFrames':1},bounds)
definitions=[];segment_keys=set()
# 0.3.7 on the Live II: the menu opens (title + back arrow) but the list area
# stays dark. The SDK reference overlay differs from ours in two points:
# transparent background and a focusable overlay child. All menus now use a
# focusable child; the background differs per colour group so ONE device test
# tells which variant shows the entries (osc/env: transparent as documented,
# mod: dark solid, fx: light solid in case the list text is dark).
MENU_OVERLAY={'osc':'menu overlay','env':'menu overlay','mod':'menu overlay dark','fx':'menu overlay light','flt':'menu overlay light'}
for color,accent in COLORS.items():
    for role in ['knob','bipolar','fader','toggle','menu','file']:
        children=[g.entry('focus','Focus',g.focus_data(accent),'0 0 296 156','No','Show','WhenFocussed'),
            ent('name','Label',g.label_data('Name',NAME,25),(0,0,296,32))]
        actions=g.control_actions('value overlay')
        if role in ['knob','bipolar']:
            children.extend([ent('knob','Knob',g.knob_filmstrip_data(f'large-{color}-{role}.png',127,'Vertical'),(0,38,112,112)),
                ent('value','Label',g.label_data('Value',accent,30),(128,66,168,48))])
            if role=='bipolar':actions=g.control_actions('bipolar overlay')
        elif role=='fader':
            children.extend([ent('value','Label',g.label_data('Value',accent,30),(0,38,296,44)),
                ent('fader','Knob',g.knob_filmstrip_data(f'large-{color}-fader.png',127,'Horizontal'),(0,96,280,56))])
        elif role=='toggle':
            children.extend([ent('switch','Button',g.button_data(f'large-{color}-on.png',f'large-{color}-off.png',1,1),(0,60,136,72)),
                ent('state','Label',g.label_data('Value',accent,30),(156,68,140,48))])
            actions=[g.action('Mouse Down','Q-Link','Data','')]
        elif role=='menu':
            # 0.3.9: exactly the slot the SDK's own generator uses for a choice with a
            # menu: the MPC's generic slider (draws name, value and a bar; drag or data
            # wheel steps through the choices) opening the list on double-tap/Enter.
            # 0.3.2-0.3.8 opened an EMPTY list from a label-only cell on Mouse Down.
            # 0.3.10, seen on the Live II through Remote Screen: the MenuOverlay list is
            # empty even in the unmodified SDK example (one blank row with a tick),
            # so it is an SDK/firmware limitation and is no longer used. A choice is
            # a Vitaleon-style fader that steps through its labels (drag, data wheel,
            # Q-Link) and opens the working KnobOverlay on double-tap, like any knob.
            children.extend([ent('value','Label',g.label_data('Value',accent,30),(0,38,296,44)),
                ent('fader','Knob',g.knob_filmstrip_data(f'large-{color}-fader.png',127,'Horizontal'),(0,96,280,56))])
            actions=g.control_actions('value overlay')
        else:
            # 0.3.17: file selectors (wavetable, sample, LFO shape) were wheel-only.
            # They now carry the same fader as every other choice, so a table can be
            # chosen by touch; the data wheel and the double-tap overlay still work.
            children.extend([ent('field','Image',{'version':1,'colour':PANEL,'image':''},(0,38,296,52)),
                ent('value','Label',g.label_data('Value',accent,24),(10,40,276,48)),
                ent('fader','Knob',g.knob_filmstrip_data(f'large-{color}-fader.png',127,'Horizontal'),(0,96,280,56))])
        definitions.append(g.obj(('key',role+'-'+color),('value',g.component_definition(actions,children,'00000000'))))
definitions.append(g.obj(('key','value overlay'),('value',g.overlay_component('KnobOverlay',g.knob_overlay_data(),BG))))
definitions.append(g.obj(('key','bipolar overlay'),('value',g.overlay_component('KnobOverlay',g.knob_overlay_data(True),BG))))
def menu_overlay(key,background):  # kept for reference; unused since 0.3.10
    component=g.overlay_component('MenuOverlay',g.menu_overlay_data(),background)
    bounds=dict(dict(component)['componentsData'][0])['bounds'];bounds[:]=[(k,'Yes' if k=='acceptsHWFocus' else v) for k,v in bounds]
    definitions.append(g.obj(('key',key),('value',component)))
# menu_overlay('menu overlay','0')  # reference form: transparent, focusable list (the 0.3.8 background variants made no difference on the device)

# The play page keeps the same 112px dials while reserving the right third for
# the browser. Values sit below the dials, not squeezed beside them.
for color in COLORS:
    for role in ['knob','bipolar','fader','toggle']:
        children=[g.entry('focus','Focus',g.focus_data(COLORS[color]),'0 0 184 188','No','Show','WhenFocussed'),
            ent('name','Label',g.label_data('Name',NAME,23,'horizontallyCentred verticallyCentred'),(0,0,184,30))]
        if role in ['knob','bipolar']:
            children.append(ent('knob','Knob',g.knob_filmstrip_data(f'large-{color}-{role}.png',127,'Vertical'),(36,32,112,112)))
        elif role=='fader':
            children.append(ent('fader','Knob',g.knob_filmstrip_data(f'home-fader-{color}.png',127,'Vertical'),(64,32,56,112)))
        else:children.append(ent('switch','Button',g.button_data(f'large-{color}-on.png',f'large-{color}-off.png',1,1),(24,56,136,72)))
        children.append(ent('value','Label',g.label_data('Value',COLORS[color],28,'horizontallyCentred verticallyCentred'),(0,146,184,40)))
        actions=g.control_actions('bipolar overlay' if role=='bipolar' else 'value overlay') if role!='toggle' else [g.action('Mouse Down','Q-Link','Data','')]
        definitions.append(g.obj(('key','home-'+role+'-'+color),('value',g.component_definition(actions,children,'00000000'))))

def parameter(name):return params[index(name)]

# Q-Link frame (0.3.22, user request): a thin outline around the four controls of the
# knob group in use, as other plugins show it. The MPC's own frame (qlinkBoundsData)
# is fixed per native tab and cannot follow these views, so the frame is drawn here and
# follows the plugin's 'UI Bank' (the group of the knob turned last).
frame_images=set()
def frame_image(w,h,color):
    name=f'qframe-{w}x{h}-{color}.png'
    if name not in frame_images:
        image=Image.new('RGBA',(w,h),(0,0,0,0));rgb=COLORS[color][2:]
        ImageDraw.Draw(image).rounded_rectangle((1,1,w-2,h-2),radius=10,outline=tuple(int(rgb[i:i+2],16) for i in (0,2,4))+(210,),width=2)
        image.save(skin/name);frame_images.add(name)
    return name
def qlink_frames(cells,section,color):
    frames=[]
    for group in range(0,len(cells),4):
        boxes=[tuple(map(int,dict(dict(c)['bounds'])['bounds'].split())) for c in cells[group:group+4]]
        row=len({(b[1],b[3]) for b in boxes})==1 and all(b2[0]-(b1[0]+b1[2])<=16 for b1,b2 in zip(boxes,boxes[1:]))
        if row:boxes=[(boxes[0][0],boxes[0][1],boxes[-1][0]+boxes[-1][2]-boxes[0][0],boxes[0][3])]
        for x,y,w,h in boxes:
            m=6 if row else 4
            frames.append(g.entry('Q-Link frame','Image',{'version':1,'colour':'0','image':frame_image(w+2*m,h+2*m,color)},f'{x-m} {y-m} {w+2*m} {h+2*m}','No','Hide','Always',
                invalidating=[f'IndexedEnabling/{section}/32/'+handle('UI Section'),f'IndexedEnabling/{group//4}/4/'+handle('UI Bank')]))
    return frames

# Q-Link slots (0.3.22). The 16 hardware knobs write the parameters 'UI Control 1..16';
# the plugin forwards a slot to the control of the visible section. A knob or fader
# on screen is bound to that same slot, so the MPC moves the control it wrote, as
# with any plugin whose knobs map straight to its controls. 0.3.20 and earlier bound
# it to the sound parameter and it stood still while the knob turned; 0.3.21 asked
# for display updates instead, which made every control stutter. 'Target' keeps the
# sound parameter in the entry for the checks; the MPC ignores it. Switches, lists
# and file selectors stay on their own parameter: their steps and labels belong to it.
def slot_data(title,name,slot):return [('Data','WithName['+title+']/'+handle(f'UI Control {slot+1}')),('Target',handle(name))]
def control(name,title,x,y,color='osc',visible=None,compact=False,slot=None):
    p=parameter(name);choices=p.get('choices',[])
    if p['kind']=='bool':role='toggle'
    elif len(choices)>1:role='menu'
    elif 'File' in name or 'Table' in name or name=='Preset':role='file'
    elif name.lower().replace('_',' ').split()[-1] in ['level','volume','master','mix','wet','attack','decay','sustain','release','hold','delay'] or name.endswith(('_decay_time','_delay_time',' delay_time')):role='fader'
    elif abs(p['default']-.5)<.0001 and p['display'].startswith('0'):role='bipolar'
    else:role='knob'
    if 1<len(choices)<=4 and p['kind']!='bool':
        # Short enumerations are immediately tappable, not a rotary mode selector.
        role='segments';key='segments-'+str(p['index'])+'-'+color
        children=[g.entry('focus','Focus',g.focus_data(COLORS[color]),'0 0 296 156','No','Show','WhenFocussed'),ent('name','Label',g.label_data('Name',NAME,25),(0,0,296,32))]
        for n,choice in enumerate(choices):
            px=(n%2)*152;py=44+(n//2)*56
            children.append(ent('choice','Button',g.button_data(f'segment-choice-{color}-on.png',f'segment-choice-{color}-off.png',n,len(choices)),(px,py,144,52)))
            words=choice.split();lines=['']
            for word in words:
                if len(lines[-1])+len(word)>12 and lines[-1]:lines.append(word)
                else:lines[-1]=(lines[-1]+' '+word).strip()
            for line,text in enumerate(lines[:2]):children.append(ent('choice text','Label',g.label_data('Text',TEXT,19,'horizontallyCentred verticallyCentred',text),(px+4,py+(4+line*22 if len(lines)>1 else 12),136,24)))
        if key not in segment_keys:
            definitions.append(g.obj(('key',key),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],children,'00000000'))));segment_keys.add(key)
    else:key=role+'-'+color
    if compact:
        assert role in ['knob','bipolar','fader','toggle'],(name,role)
        if name.startswith('Env ') or name in ['Voices','Home Attack','Home Release']:color='env'
        elif name.startswith('filter_') or name in ['Home Cutoff','Home Resonance']:color='flt'
        elif name.startswith('macro_'):color='mod'
        key='home-'+role+'-'+color
    data=slot_data(title,name,slot) if slot is not None and role in ['knob','bipolar','fader'] else [('Data','WithName['+title+']/'+handle(name))]
    return g.entry(title,key,{'version':1,'handleName':'Data'},f'{x} {y} '+('184 188' if compact else '296 156'),'Yes','Hide' if visible else 'Show','Always',data+([('Menu',handle(name))] if role=='menu' else []),invalidating=visible)

def home_browser(children):
    children.append(rect('browser divider',(800,96,1,516),LINE))
    children.append(label('Loaded sound',(24,98,760,24),MUTED,20))
    for part in range(2):children.append(label('Loaded name',(24+part*384,126,376,32),COLORS['env'],24,value='library_loaded_'+str(part)))
    for part in range(4):children.append(label('Import status',(24+(part%2)*280,166+(part//2)*25,276,24),COLORS['mod'],17,value='display_load_text_'+str(part)))
    data={'version':2,'onImage':'home-filter-on.png','offImage':'home-filter-off.png','buttonId':1,'numButtonsInGroup':1,'handleName':handle('Home Filter'),'gestureBehaviour':'Instant'}
    children.append(ent('Filter 1 enable','Button',data,(584,160,200,52),True))
    children.append(label('FILTER 1',(596,160,116,52),TEXT,19))
    children.append(label('Filter state',(712,160,66,52),TEXT,21,value='Home Filter'))
    children.append(label('MPC PRESET',(828,104,412,40),TEXT,30))
    children.append(label('All sounds are in the top bar.',(828,154,412,32),MUTED,23))
    children.append(label('Choose a name to load it.',(828,190,412,32),MUTED,23))
    children.append(rect('sound count divider',(828,248,404,1),LINE))
    children.append(label('Available sounds',(828,274,412,30),MUTED,22))
    children.append(label('Inventory',(828,316,412,36),COLORS['env'],24,value='library_page_text'))
    children.append(label('Bank',(828,388,412,30),MUTED,22))
    for part in range(2):children.append(label('Bank name',(828,430+part*30,412,30),TEXT,23,value='library_bank_text_'+str(part)))
    children.append(label('Save edits with the MPC save icon.',(828,548,412,30),MUTED,20))

def envelope(children):
    data={'version':1,'envelopeType':'DAHDSR','envelopeStyle':'WithoutHeader','envelopeName':'ENVELOPE','backgroundColour':'ff1c1f29','scaleColour':LINE,'lineColour':COLORS['env'],'fillColour':'2064d9ba','highlightColour':TEXT,'lineWidth':3,'handleWidth':40,'handleHeight':40,'handleShape':'Circle','handleLineWidth':2,'handleLineColour':COLORS['env'],'handleFillColour':BG,'handleTextColour':TEXT,'handleLineWidthOnPressed':2,'handleLineColourOnPressed':TEXT,'handleFillColourOnPressed':COLORS['env'],'handleTextColourOnPressed':BG,'handleArrowLineColourOnPressed':TEXT,'handleArrowFillColourOnPressed':COLORS['env']}
    for stage in ['delay','attack','hold','decay','sustain','release']:data[stage+'Handle']=handle('Env '+stage)
    for stage in ['attack','decay','release']:data[stage+'ShapeHandle']=handle('Env '+stage+'_power')
    data.update(forceHideShapeHandles=False,decayAndReleaseCurvesLinked=False,curveType='Exponential')
    children.append(ent('editable envelope','Envelope',data,(32,100,944,164),True))
    children.append(trace('Envelope output',(1000,100,248,164),'display_env_value','env'))
def pretty(k):
    words=k.replace('_',' ').replace('spectral morph','spectral').replace('distortion','warp').replace('unison voices','unison').replace('keytrack transpose','key transpose').replace('keytrack tune','key tune')
    return words.title()
from surface_spec import VIEWS,GROUPS,TARGET_GROUPS,write_header
write_header()
definitions.append(g.obj(('key','effect-toggle'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    ent('Effect name','Label',g.label_data('Name',NAME,24),(0,0,376,30)),
    ent('Effect switch','Button',g.button_data('large-fx-on.png','large-fx-off.png',1,1),(0,34,136,72)),
    ent('Effect state','Label',g.label_data('Value',COLORS['fx'],26),(156,44,210,48)),
],'00000000'))))
definitions.append(g.obj(('key','wave-frame'),('value',g.component_definition(g.control_actions('value overlay'),[
    ent('Frame name','Label',g.label_data('Name',NAME,22),(0,0,120,28)),
    ent('Frame value','Label',g.label_data('Value',COLORS['osc'],24),(144,0,152,28)),
    ent('Frame slider','Knob',g.knob_filmstrip_data('large-osc-fader.png',127,'Horizontal'),(0,32,280,56)),
],'00000000'))))
pages=[]
NAV_RED='#e0143c'
def nav_button(text,x,y,w,h,param,value=1,count=1,color='osc',ink=None):
    # ink: the colour of the caption (0.3.43: a source or a target in the colour of its section).
    # Native-sized caption assets; no text widget ever covers a button.
    # 0.3.26 (user request): the chosen group, page and section carry a red line along
    # the bottom edge, so the place you are in reads at a glance.
    marked=param in ['UI Group','UI Page','UI Section','UI Env View','FX Position']
    key=hashlib.sha256((f'{text}:{w}:{h}:{color}'+(':marked' if marked else '')+(':'+ink if ink else '')).encode()).hexdigest()[:16]
    for state in ['off','on']:
        caption=text[0 if state=='off' else 1] if isinstance(text,tuple) else text
        filename=f'nav-{key}-{state}.png'
        im=Image.new('RGBA',(w,h),'#373045' if state=='on' else '#222531');d=ImageDraw.Draw(im)
        d.rectangle((0,0,w-1,h-1),outline='#'+(COLORS[color] if state=='on' else LINE)[2:],width=2 if state=='on' else 1)
        if marked and state=='on':d.rectangle((2,h-7,w-3,h-3),fill=NAV_RED)
        size=22;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
        while d.textlength(caption,font=font)>w-12 and size>18:size-=1;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
        assert d.textlength(caption,font=font)<=w-12,(caption,w)
        if caption in ('<','>'):
            # The arrows of a stepper (0.3.50) are drawn: the '<' and '>' of the font are thin and small.
            cx,cy,s=w/2,h/2,(-1 if caption=='<' else 1)
            d.line([(cx-5*s,cy-11),(cx+6*s,cy),(cx-5*s,cy+11)],fill='#edf0f7',width=3,joint='curve');im.save(skin/filename);continue
        bb=d.textbbox((0,0),caption,font=font);d.text(((w-d.textlength(caption,font=font))/2,(h-(bb[3]-bb[1]))/2-bb[1]),caption,font=font,fill='#'+ink[2:] if ink else '#edf0f7');im.save(skin/filename)
    data={'version':2,'onImage':f'nav-{key}-on.png','offImage':f'nav-{key}-off.png','buttonId':value,'numButtonsInGroup':count,'handleName':handle(param),'gestureBehaviour':'Instant'}
    return ent(text[0] if isinstance(text,tuple) else text,'Button',data,(x,y,w,h),True)

def check_button(text,x,y,w,h,param,color='env'):
    # A checkbox (0.3.39): an empty box when off, a ticked box when on, the caption beside it.
    key=hashlib.sha256(f'check:{text}:{w}:{h}:{color}'.encode()).hexdigest()[:16]
    for state in ['off','on']:
        im=Image.new('RGBA',(w,h),'#373045' if state=='on' else '#222531');d=ImageDraw.Draw(im)
        tint='#'+(COLORS[color] if state=='on' else LINE)[2:]
        d.rectangle((0,0,w-1,h-1),outline=tint,width=2 if state=='on' else 1)
        box=24;bx=12;by=(h-box)//2
        d.rectangle((bx,by,bx+box,by+box),outline='#'+(COLORS[color] if state=='on' else MUTED)[2:],width=2)
        if state=='on':d.line((bx+5,by+12,bx+10,by+18,bx+19,by+6),fill=tint,width=3)
        size=22;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size);room=w-bx-box-18
        while d.textlength(text,font=font)>room and size>18:size-=1;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
        assert d.textlength(text,font=font)<=room,(text,w)
        bb=d.textbbox((0,0),text,font=font);d.text((bx+box+10,(h-(bb[3]-bb[1]))/2-bb[1]),text,font=font,fill='#edf0f7');im.save(skin/f'nav-{key}-{state}.png')
    data={'version':2,'onImage':f'nav-{key}-on.png','offImage':f'nav-{key}-off.png','buttonId':1,'numButtonsInGroup':1,'handleName':handle(param),'gestureBehaviour':'Instant'}
    return ent(text,'Button',data,(x,y,w,h),True)

def label_for(handle_name,color,size,justification='left verticallyCentred'):
    # A value label on a named handle of its component (g.label_data always uses "Data").
    return g.obj(*[(k,handle_name if k=='handleName' else v) for k,v in g.label_data('Value',color,size,justification)])
for state in ['off','on']:
    image=Image.new('RGBA',(1084,52),(0,0,0,0) if state=='off' else (55,48,69,255));image.save(skin/f'library-rowtap-{state}.png')
definitions.append(g.obj(('key','library-row'),('value',g.component_definition([g.action('Mouse Down','Toggle Switch','Data',''),g.action('Enter Pressed','Toggle Switch','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['osc']),'0 0 1084 52','No','Show','WhenFocussed'),
    ent('row','Button',g.button_data('library-rowtap-on.png','library-rowtap-off.png',1,1),(0,0,1084,52)),
    ent('File kind','Label',label_for('Kind',MUTED,18),(0,12,110,30)),
    ent('File name','Label',label_for('Text',TEXT,24),(116,4,950,44)),
],'00000000'))))
def add_library(c):
    # 0.3.19 layout: place line, three commands, six rows of KIND | NAME | button.
    # A row offers OPEN for a drive, folder or bank and LOAD for a sound (state 1 / 2
    # of storage_available_N). REFRESH is gone: DRIVES rescans.
    # 0.3.25: the rows also list wavetables, WAV files and LFO shapes (LOAD). A WAV can be a
    # wavetable or a sample; the four buttons beside the commands say where LOAD puts it.
    c.append(label('Directory',(24,76,940,26),MUTED,18,value='storage_path_0'))
    c.append(label('Page',(980,76,268,26),MUTED,18,value='storage_page'))
    # 0.3.39: INSTALLED lists the installed banks (OPEN one to put it into PRESET), and the
    # INSTALL checkbox says whether a bank opened from another drive is copied into Vitaleon.
    c.append(nav_button('DRIVES',24,108,100,52,'Drives'));c.append(nav_button('UP',132,108,80,52,'Up Folder'))
    c.append(nav_button('INSTALLED',220,108,128,52,'Bank Step',3,4))
    c.append(check_button('INSTALL',356,108,150,52,'Install Bank'))
    c.append(label('WAV AS',(520,121,70,26),MUTED,18))
    for n,caption in enumerate(['OSC 1','OSC 2','OSC 3','SAMPLE']):c.append(nav_button(caption,596+n*100,108,94,52,'Import Target',n,4,'env'))
    c.append(nav_button('PREV',1010,108,114,52,'Previous Files'));c.append(nav_button('NEXT',1134,108,114,52,'Next Files'))
    # 0.3.28 (user request): a tap anywhere on a row does what its button does. The row is a
    # component with its own tap action (Mouse Down -> Toggle Switch on the row's command
    # parameter): a plain part of the row is a button of the row's size, and the two texts lie
    # on it as value labels. A label takes the touch away from a button below it, but the
    # touch then reaches the component's action, which other MPC plugins rely on. The
    # OPEN / LOAD buttons at the right stay as they are.
    for row in range(6):
        y=174+row*54;available=handle(f'storage_available_{row}')
        c.append(rect('file separator',(24,y+51,1224,1),LINE))
        c.append(g.entry(f'Row {row+1}','library-row',{'version':1,'handleName':'Data'},f'24 {y} 1084 52','Yes','Show','Always',
            [('Data',handle(f'Open Row {row+1}')),('Kind',handle(f'storage_row_{row*2}')),('Text',handle(f'storage_row_{row*2+1}'))]))
        c.append(conditional(nav_button('OPEN',1116,y,132,52,f'Open Row {row+1}'),['IndexedEnabling/1/3/'+available]))
        c.append(conditional(nav_button('LOAD',1116,y,132,52,f'Open Row {row+1}',color='env'),['IndexedEnabling/2/3/'+available]))
    # 0.3.45: MY SOUNDS opens the folder of the sounds saved with SAVE SOUND, EXPORT copies them
    # to the drive that is open here. They stand at the right; the status keeps the left, three
    # parts in its line and the rest of a long sentence below.
    for part in range(3):c.append(label('Browser status',(24+part*308,506,304,26),COLORS['mod'],18,value=f'storage_status_{part}'))
    c.append(label('In PRESET',(24,534,96,24),MUTED,18))
    for part in range(2):c.append(label('Bank',(124+part*250,534,246,24),COLORS['env'],18,value=f'active_bank_{part}'))
    c.append(label('Browser status',(632,534,304,24),COLORS['mod'],18,value='storage_status_3'))
    c.append(nav_button('MY SOUNDS',964,508,140,52,'Library Step',1,3))
    c.append(nav_button('EXPORT',1112,508,136,52,'Library Step',2,3,'env'))

# ---- OSC overview (0.3.26): three oscillators and the sample side by side --------
# Per source one column: name and on/off button, a picture (the wave of an oscillator, the
# level curve of the sample), then three small dials: Level, Frame (sample: Pan) and Pitch.
# User request after the first version: pictures that can be read, a sample view, and
# dials instead of faders. One Q-Link group per kind of dial: the four levels, the three
# frames and the sample pan, the four pitches; the frame marks each dial of the group.
# The on/off buttons write their own parameter, like every switch.
for mini in COLORS:
  for role in ['knob','bipolar']:
    definitions.append(g.obj(('key','mini-'+role+'-'+mini),('value',g.component_definition(g.control_actions('bipolar overlay' if role=='bipolar' else 'value overlay'),[
        g.entry('focus','Focus',g.focus_data(COLORS[mini]),'0 0 96 128','No','Show','WhenFocussed'),
        ent('name','Label',g.label_data('Name',NAME,20,'horizontallyCentred verticallyCentred'),(0,0,96,24)),
        ent('knob','Knob',g.knob_filmstrip_data(f'small-{mini}-{role}.png',127,'Vertical'),(8,24,80,80)),
        ent('value','Label',g.label_data('Value',COLORS[mini],20,'horizontallyCentred verticallyCentred'),(0,104,96,24)),
    ],'00000000'))))
# ---- HOME (0.3.27): the sound's name, author, style and comment; its four macros under
# their own names; cutoff, resonance, filter switch and volume; the amp envelope as a
# picture that can be dragged and as four dials. User request: name and author, cutoff,
# resonance, volume and the volume ADSR; voices, unison and detune do not belong here.
definitions.append(g.obj(('key','mini-toggle-flt'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['flt']),'0 0 150 128','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,20,'horizontallyCentred verticallyCentred'),(0,0,150,24)),
    ent('switch','Button',g.button_data('large-flt-on.png','large-flt-off.png',1,1),(7,28,136,72)),
    ent('state','Label',g.label_data('Value',COLORS['flt'],20,'horizontallyCentred verticallyCentred'),(0,104,150,24)),
],'00000000'))))
definitions.append(g.obj(('key','macro-knob-mod'),('value',g.component_definition(g.control_actions('value overlay'),[
    g.entry('focus','Focus',g.focus_data(COLORS['mod']),'0 0 296 112','No','Show','WhenFocussed'),
    ent('knob','Knob',g.knob_filmstrip_data('large-mod-knob.png',127,'Vertical'),(0,0,112,112)),
    ent('value','Label',g.label_data('Value',COLORS['mod'],30),(128,32,168,48)),
],'00000000'))))
def home_page(children,view,version):
    items=view['sections'][0]['items'];assert len(items)==12
    section=[f'IndexedEnabling/0/32/'+handle('UI Section')]
    # 0.3.50 (user: a lot of room between the name and the author; BANK, BANK LIST and SAVE SOUND
    # stand oddly; add INIT SOUND). The page has three bands:
    #   head   what is loaded: the name, directly under it author and style, beside them the
    #          comment (three short lines: a text of the MPC holds 31 characters). At the right
    #          what is done with the sound, INIT SOUND and SAVE SOUND, in the place the page
    #          buttons have on the other pages; under them the version.
    #   sound  the four macros, then filter, volume and the amp envelope. Both rows end at the
    #          same right edge (the envelope picture takes the room between the two groups).
    #   foot   the bank as one stepper (< name >) with BANK LIST, then the load message.
    # Up to 0.3.49 the author stood at x 664 beside the name, the load message under the name and
    # the bank name left-aligned between two buttons 460 apart.
    # 0.3.60 (user on the device: bank, INIT SOUND and SAVE SOUND belong at the top, in the middle
    # of the head; the comment goes down to where the bank stood, left-aligned like the name).
    #   head   left the name with author and style under it (416 wide: the widest name of the
    #          281 installed sounds takes 354 at this size). Beside them ONE row, centred on the
    #          height of name and author: the bank as a stepper (< name >), BANK LIST, INIT SOUND,
    #          SAVE SOUND, ending at the right edge of the macros.
    #   foot   three lines: left the comment, from the third macro's column the load message,
    #          at the right end of the last line the version.
    children.append(label('Sound name',(24,66,416,46),TEXT,36,value='library_loaded_0'))
    children.append(label('Author and style',(24,112,416,32),COLORS['env'],26,value='display_sound_info_0'))
    ROW=79      # (66 + 144 - 52) / 2: the row sits in the middle between the top of the name and the bottom of the author
    children.append(nav_button('<',448,ROW,64,52,'Bank Step',1,4,'env'))
    children.append(rect('bank field',(520,ROW,204,52),'ff1c1f29'))
    centred='horizontallyCentred verticallyCentred'
    # One line, centred in the field. A text of the MPC holds 31 characters; a longer bank name
    # shows its first part here and in full in LIBRARY. The widest name of the banks that ship
    # takes 176 at this size.
    children.append(label('Bank',(524,ROW,196,52),COLORS['env'],22,value='active_bank_0',justify=centred))
    children.append(nav_button('>',732,ROW,64,52,'Bank Step',2,4,'env'))
    children.append(nav_button('BANK LIST',804,ROW,124,52,'Bank Step',3,4,'env'))
    # INIT SOUND is a switch: the first tap turns it on and it reads CONFIRM INIT, the second
    # tap resets the sound. Left alone it goes back by itself (see initService).
    children.append(nav_button(('INIT SOUND','CONFIRM INIT'),936,ROW,156,52,'UI Init',color='mod'))
    children.append(nav_button('SAVE SOUND',1100,ROW,156,52,'UI Save',1,3,'osc'))
    cells=[]
    def add(cell,slot):
        dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))));cells.append(cell)
    for n in range(4):
        x=24+312*n
        children.append(label('Macro name',(x,164,296,28),TEXT,24,value=f'display_macro_name_{n}'))   # clear of the frame of the Q-Link group, which starts at y 194
        add(g.entry(f'Macro {n+1}','macro-knob-mod',{'version':1,'handleName':'Data'},f'{x} 200 296 112','Yes','Hide','Always',slot_data(f'Macro {n+1}',items[n],n),invalidating=section),n)
    row=[(4,'Cutoff','mini-knob-flt',24,96),(5,'Reso','mini-knob-flt',120,96),(6,'Filter 1','mini-toggle-flt',216,150),(7,'Volume','mini-knob-osc',366,96),
         (8,'Attack','mini-knob-env',872,96),(9,'Decay','mini-knob-env',968,96),(10,'Sustain','mini-knob-env',1064,96),(11,'Release','mini-knob-env',1160,96)]
    for slot,caption,key,x,w in row:
        data=[('Data','WithName['+caption+']/'+handle(items[slot]))] if 'toggle' in key else slot_data(caption,items[slot],slot)
        add(g.entry(caption,key,{'version':1,'handleName':'Data'},f'{x} 336 {w} 128','Yes','Hide','Always',data,invalidating=section),slot)
    # The amp envelope between the two groups, bound to envelope 1 itself.
    data={'version':1,'envelopeType':'DAHDSR','envelopeStyle':'WithoutHeader','envelopeName':'AMP ENVELOPE','backgroundColour':'ff1c1f29','scaleColour':LINE,'lineColour':COLORS['env'],'fillColour':'2064d9ba','highlightColour':TEXT,'lineWidth':3,'handleWidth':28,'handleHeight':28,'handleShape':'Circle','handleLineWidth':2,'handleLineColour':COLORS['env'],'handleFillColour':BG,'handleTextColour':TEXT,'handleLineWidthOnPressed':2,'handleLineColourOnPressed':TEXT,'handleFillColourOnPressed':COLORS['env'],'handleTextColourOnPressed':BG,'handleArrowLineColourOnPressed':TEXT,'handleArrowFillColourOnPressed':COLORS['env']}
    for stage in ['delay','attack','hold','decay','sustain','release']:data[stage+'Handle']=handle('env_1_'+stage)
    for stage in ['attack','decay','release']:data[stage+'ShapeHandle']=handle('env_1_'+stage+'_power')
    data.update(forceHideShapeHandles=True,decayAndReleaseCurvesLinked=False,curveType='Exponential')
    children.append(ent('amp envelope','Envelope',data,(486,336,362,128),True))
    children.extend(qlink_frames(cells,0,'mod'));children.extend(cells)
    # The foot (0.3.60): the comment of the sound in three lines, left-aligned with the name; the
    # load message on the first two lines from the column of the third macro; the version at the
    # right end of the third line. (0.3.39 to 0.3.59 the bank stood here.)
    children.append(rect('foot line',(24,484,1232,1),LINE))
    FOOT=494
    for line in range(3):children.append(label('Sound note',(24,FOOT+line*22,600,22),MUTED,18,value=f'display_sound_note_{line}'))
    for part in range(4):children.append(label('Import status',(648+(part%2)*288,FOOT+(part//2)*22,276,22),COLORS['mod'],17,value=f'display_load_text_{part}'))
    children.append(label('VITALEON '+version,(904,FOOT+44,352,22),MUTED,16,justify='right'))
# ---- SAVE SOUND (0.3.44): the name entry ---------------------------------------------------
# The MPC gives a plugin no text entry, so the page has its own keys. One momentary
# parameter ('UI Key') carries the key: 1-26 A to Z, 27-36 the digits 1 to 9 and 0, 37 space,
# 38 hyphen, 39 delete, 40 clear. The plugin keeps the name and shows it in 'save_name'.
SAVE_KEYS={**{chr(65+n):n+1 for n in range(26)},**{str((n+1)%10):27+n for n in range(10)},'SPACE':37,'-':38,'DEL':39,'CLEAR':40}
def save_page(children):
    children.append(nav_button('CANCEL',1092,76,156,52,'UI Page',0,len(VIEWS),'osc'))
    children.append(rect('name field',(24,136,1232,56),'ff1c1f29'))
    children.append(label('Name',(40,142,1200,44),TEXT,34,value='save_name'))
    def key(text,x,y,w=116):children.append(nav_button(text,x,y,w,52,'UI Key',SAVE_KEYS[text],41,'osc'))
    for row,keys in enumerate(['1234567890','QWERTYUIOP','ASDFGHJKL-','ZXCVBNM']):
        for col,text in enumerate(keys):key(text,24+col*124,208+row*60)
    key('SPACE',892,388,240);key('DEL',1140,388)
    children.append(nav_button(('CAPS','CAPS'),24,452,116,56,'Save Caps',color='osc'))
    key('CLEAR',148,454)
    children.append(nav_button('SAVE',892,452,364,56,'UI Save',2,3,'env'))
    for part in range(3):children.append(label('Save status',(24+part*412,520,404,26),COLORS['mod'],19,value=f'save_status_{part}'))
# ---- Oscillator page (0.3.29): every control on one page, no sections -----------------
# Small dials for the finger only (no Q-Link action) and a small switch.
for role in ['knob','bipolar']:
    definitions.append(g.obj(('key','mini-'+role+'-osc-touch'),('value',g.component_definition(
        [g.action('Double Click','Show Overlay','Data','bipolar overlay' if role=='bipolar' else 'value overlay'),g.action('Enter Pressed','Show Overlay','Data','bipolar overlay' if role=='bipolar' else 'value overlay')],[
        g.entry('focus','Focus',g.focus_data(COLORS['osc']),'0 0 96 128','No','Show','WhenFocussed'),
        ent('name','Label',g.label_data('Name',NAME,20,'horizontallyCentred verticallyCentred'),(0,0,96,24)),
        ent('knob','Knob',g.knob_filmstrip_data(f'small-osc-{role}.png',127,'Vertical'),(8,24,80,80)),
        ent('value','Label',g.label_data('Value',COLORS['osc'],18,'horizontallyCentred verticallyCentred'),(0,104,96,24)),
    ],'00000000'))))
for state in ['off','on']:
    image=Image.new('RGBA',(80,52),(0,0,0,0));d=ImageDraw.Draw(image);rgb=tuple(bytes.fromhex(COLORS['osc'][2:]))
    d.rounded_rectangle((2,8,77,43),radius=18,fill=rgb if state=='on' else '#454c61');x=58 if state=='on' else 22;d.ellipse((x-13,13,x+13,39),fill='#181a23' if state=='on' else '#edf0f7')
    image.save(skin/f'mini-osc-{state}.png')
definitions.append(g.obj(('key','mini-switch-osc'),('value',g.component_definition([],[
    g.entry('focus','Focus',g.focus_data(COLORS['osc']),'0 0 96 128','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,20,'horizontallyCentred verticallyCentred'),(0,0,96,24)),
    ent('switch','Button',g.button_data('mini-osc-on.png','mini-osc-off.png',1,1),(8,38,80,52)),
    ent('state','Label',g.label_data('Value',COLORS['osc'],18,'horizontallyCentred verticallyCentred'),(0,104,96,24)),
],'00000000'))))
OSC_CAPTIONS={'level':'Level','pan':'Pan','transpose':'Pitch','tune':'Tune','wave_frame':'Frame','spectral_morph_amount':'Spectral','distortion_amount':'Warp','distortion_phase':'Warp Phase',
    'unison_voices':'Voices','unison_detune':'Detune','phase':'Phase','random_phase':'Random','spectral_morph_type':'Spec. Type','distortion_type':'Warp Type','destination':'Path','unison_blend':'Blend',
    'stereo_spread':'Stereo','stack_style':'Stack','detune_power':'Detune Power','detune_range':'Detune Range','frame_spread':'Frame Spread','spectral_unison':'Spec. Unison','distortion_spread':'Warp Spread',
    'spectral_morph_spread':'Spec. Spread','transpose_quantize':'Quantize','midi_track':'Key Track','smooth_interpolation':'Smooth'}
# 0.3.41 (user request): the page shows its controls in two pages, PAGE 1 and PAGE 2, in
# place of two Q-Link layers over one crowded page. A page of controls is a section, so the
# sixteen knobs reach exactly what is on screen and no dial exists twice.
def dial_component(key,w,h,strip,size,knob_xy,name_size,value_size,qlink,color='osc'):
    for role in ['knob','bipolar']:
        overlay='bipolar overlay' if role=='bipolar' else 'value overlay'
        actions=g.control_actions(overlay) if qlink else [g.action('Double Click','Show Overlay','Data',overlay),g.action('Enter Pressed','Show Overlay','Data',overlay)]
        name_h=knob_xy[1];value_y=knob_xy[1]+size
        definitions.append(g.obj(('key',key.replace('ROLE',role)),('value',g.component_definition(actions,[
            g.entry('focus','Focus',g.focus_data(COLORS[color]),f'0 0 {w} {h}','No','Show','WhenFocussed'),
            ent('name','Label',g.label_data('Name',NAME,name_size,'horizontallyCentred verticallyCentred'),(0,0,w,name_h)),
            ent('knob','Knob',g.knob_filmstrip_data(strip.replace('ROLE',role),127,'Vertical'),(knob_xy[0],knob_xy[1],size,size)),
            ent('value','Label',g.label_data('Value',COLORS[color],value_size,'horizontallyCentred verticallyCentred'),(0,value_y,w,h-value_y)),
        ],'00000000'))))
OSC_SIZES={'big':(172,164),'mid':(86,124),'tiny':(75,96),'wide':(172,124),'low':(128,130),'under':(172,130)}
dial_component('osc-big-ROLE',172,164,'large-osc-ROLE.png',112,(30,24),22,24,True)
dial_component('osc-wide-ROLE',172,124,'small-osc-ROLE.png',80,(46,22),20,20,True)     # a medium dial in the cell width of a large one
dial_component('osc-low-ROLE',128,130,'small-osc-ROLE.png',80,(24,24),20,20,True)      # under the picture, four across
dial_component('osc-under-ROLE',172,130,'small-osc-ROLE.png',80,(46,24),20,20,True)    # under the picture, three across
for key,(w,h),top in [('osc-big-switch',(172,164),44),('osc-under-switch',(172,130),28)]:
    definitions.append(g.obj(('key',key),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
        g.entry('focus','Focus',g.focus_data(COLORS['osc']),f'0 0 {w} {h}','No','Show','WhenFocussed'),
        ent('name','Label',g.label_data('Name',NAME,22 if h>130 else 20,'horizontallyCentred verticallyCentred'),(0,0,w,24)),
        ent('switch','Button',g.button_data('large-osc-on.png','large-osc-off.png',1,1),(18,top,136,72)),
        ent('state','Label',g.label_data('Value',COLORS['osc'],24 if h>130 else 20,'horizontallyCentred verticallyCentred'),(0,h-28,w,28)),
    ],'00000000'))))
def osc_page(children,view,chart):
    prefix=f'osc_{chart+1}_';pages=[section['items'] for section in view['sections']];assert [len(items) for items in pages]==[16,11]
    # Head row, left: on/off, the name of the oscillator, the list of tables. No version line.
    # Under it the wave picture (512 x 288). All of this stays while the page of controls changes.
    children.append(nav_button(('OFF','ON'),24,68,96,52,prefix+'on',color='osc'))
    children.append(label(view['title'].upper(),(132,68,216,52),TEXT,24))
    children.append(nav_button('TABLE LIST',356,68,196,52,'UI Browse',chart+1,5))
    wave_bars(children,chart,32,128,512,288)
    # 0.3.49: PAGE 1 and PAGE 2 are two small buttons at the top right of the page (see
    # page_buttons), beside the head row with the on switch and TABLE LIST. The block of dials
    # starts under them, level with the picture; its third row stands beside the group under
    # the picture.
    def kind(name):
        p=parameter(name)
        if p['kind']=='bool':return 'switch'
        if len(p.get('choices',[]))>1:return 'list'
        return 'bipolar' if abs(p['default']-.5)<.0001 and p['display'].startswith('0') else 'knob'
    # One knob group per row. PAGE 1: a row of large dials, two rows of medium ones beside the
    # picture, the fourth group under the picture. PAGE 2: two rows of large dials, the third
    # group under the picture.
    rows=[[('big',568,126,172),('wide',568,298,172),('wide',568,430,172),('low',32,428,128)],
          [('big',568,126,172),('big',568,298,172),('under',32,428,172)]]
    frames=[];widgets=[]
    for section,items in enumerate(pages):
        shown=[f'IndexedEnabling/{section}/32/'+handle('UI Section')];cells=[]
        for slot,name in enumerate(items):
            group,index=divmod(slot,4);size,x0,y,step=rows[section][group];x=x0+index*step
            caption=OSC_CAPTIONS[name[len(prefix):]];k=kind(name);w,h=OSC_SIZES[size]
            # A switch or a list is bound to its own parameter (its steps and labels belong to
            # it); a dial is bound to its Q-Link slot.
            if k=='switch':assert size in ['big','under'],name;key=f'osc-{size}-switch';data=[('Data','WithName['+caption+']/'+handle(name))]
            elif k=='list':key=f'osc-{size}-knob';data=[('Data','WithName['+caption+']/'+handle(name))]
            else:key=f'osc-{size}-{k}';data=slot_data(caption,name,slot)
            e=g.entry(caption,key,{'version':1,'handleName':'Data'},f'{x} {y} {w} {h}','Yes','Hide','Always',data,invalidating=shown)
            dict(dict(e)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))));cells.append(e)
        frames+=qlink_frames(cells,section,'osc');widgets+=cells
    # Frames first: they lie under the controls and take no touch.
    children.extend(frames);children.extend(widgets)
# ---- FILTERS page (0.3.30): both filters, each with the picture of its response -------
# 0.3.46 (user request): two pages of controls, PAGE 1 and PAGE 2, where the two Q-Link layer
# buttons stood (top row, right); the pictures at the left stay. A control is a cell with the
# dial at the left and its name and value beside it, four to a row, one knob bank per row.
# A list (model, style) has a smaller value line: "Notch Spread" has to fit.
for role in ['knob','bipolar']:
    for key,size in [('flt-row-',26),('flt-rowlist-',20)]:
        definitions.append(g.obj(('key',key+role),('value',g.component_definition(g.control_actions('bipolar overlay' if role=='bipolar' else 'value overlay'),[
            g.entry('focus','Focus',g.focus_data(COLORS['flt']),'0 0 222 100','No','Show','WhenFocussed'),
            ent('name','Label',g.label_data('Name',NAME,20,'left verticallyCentred'),(10,0,206,22)),
            ent('knob','Knob',g.knob_filmstrip_data(f'small-flt-{role}.png',127,'Vertical'),(8,20,80,80)),
            ent('value','Label',g.label_data('Value',COLORS['flt'],size,'left verticallyCentred'),(98,36,120,44)),
        ],'00000000'))))
for state in ['off','on']:
    image=Image.new('RGBA',(64,52),(0,0,0,0));d=ImageDraw.Draw(image);rgb=tuple(bytes.fromhex(COLORS['flt'][2:]))
    d.rounded_rectangle((2,10,61,41),radius=16,fill=rgb if state=='on' else '#454c61');x=46 if state=='on' else 18;d.ellipse((x-11,15,x+11,37),fill='#181a23' if state=='on' else '#edf0f7')
    image.save(skin/f'tiny-flt-{state}.png')
definitions.append(g.obj(('key','flt-row-switch'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['flt']),'0 0 222 100','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,20,'left verticallyCentred'),(10,0,206,22)),
    ent('switch','Button',g.button_data('tiny-flt-on.png','tiny-flt-off.png',1,1),(16,34,64,52)),
    ent('state','Label',g.label_data('Value',COLORS['flt'],26,'left verticallyCentred'),(98,36,120,44)),
],'00000000'))))
# 'From F%d': the switch that feeds the other filter into this one.
FILTER_CAPTIONS={'cutoff':'Cutoff','resonance':'Resonance','drive':'Drive','mix':'Mix','blend':'Blend','keytrack':'Key Track','model':'Model','style':'Style',
    'blend_transpose':'Comb Offset','formant_x':'Formant X','formant_y':'Formant Y','formant_transpose':'Formant Pitch','formant_resonance':'Formant Reso','formant_spread':'Formant Spread','filter_input':'From Filter %d'}
def filter_bars(children,f,x,y,w,h):
    # One column per measured frequency (20 Hz to 20 kHz, equal steps in pitch), one frame per
    # level: the line of the response with the area below it lightly filled.
    columns,levels=FILTER_GRAPH['kFilterColumns'],FILTER_GRAPH['kFilterLevels'];assert w%columns==0 and levels<=h;cw=w//columns
    top,floor=FILTER_DB['kFilterTopDb'],FILTER_DB['kFilterFloorDb']
    name=f'filterbar-{cw}x{h}-{levels}.png'
    children.append(rect('graph background',(x-8,y-4,w+16,h+8),'ff1c1f29'))
    for db in (0,-24,-48):children.append(rect('level line',(x,y+round((top-db)/(top-floor)*(h-3))+1,w,1),'ff454c61' if db==0 else 'ff292d39'))
    for hz in (100,1000,10000):children.append(rect('frequency line',(x+round(math.log10(hz/20)/3*w),y,1,h),'ff292d39'))
    for i in range(columns):
        data={'version':2,'imageType':'FilmStrip','colour':'0','image':name,'numFrames':levels,'handleName':handle(f'display_filter_{f}_{i}'),'animated':False}
        children.append(g.entry('filter column '+str(i),'Image',data,f'{x+i*cw} {y} {cw} {h}','No','Show','Always'))
    strip=Image.new('RGBA',(cw,h*levels));d=ImageDraw.Draw(strip);rgb=tuple(bytes.fromhex(COLORS['flt'][2:]))
    for k in range(levels):
        level=round((1-k/(levels-1))*(h-3))
        d.rectangle((0,k*h+level,cw-1,k*h+h-1),fill=rgb+(60,))
        d.rectangle((0,k*h+level,cw-1,k*h+level+2),fill=rgb+(255,))
    strip.save(skin/name)
def filters_page(children,view):
    pages=[section['items'] for section in view['sections']];assert [len(items) for items in pages]==[16,16]
    def frame(x,y,w,h,section,bank,m=4):
        return g.entry('Q-Link frame','Image',{'version':1,'colour':'0','image':frame_image(w+2*m,h+2*m,'flt')},f'{x-m} {y-m} {w+2*m} {h+2*m}','No','Hide','Always',
            invalidating=[f'IndexedEnabling/{section}/32/'+handle('UI Section'),f'IndexedEnabling/{bank}/4/'+handle('UI Bank')])
    def kind(name):
        p=parameter(name)
        if p['kind']=='bool':return 'switch'
        if len(p.get('choices',[]))>1:return 'list'
        if name.endswith(('_formant_x','_formant_y')):return 'knob'   # a position from 0 to 1, not an offset around the middle
        return 'bipolar' if abs(p['default']-.5)<.0001 and p['display'].startswith('0') else 'knob'
    frames=[];fixed=[];widgets=[]
    for f in range(2):
        y0=68+252*f;p=f'filter_{f+1}_'
        # Left, on both pages: on/off, the name, the picture of the response.
        fixed.append(nav_button(('OFF','ON'),24,y0,96,52,p+'on',color='flt'))
        fixed.append(label(f'FILTER {f+1}',(132,y0,196,52),TEXT,24))
        filter_bars(fixed,f,32,y0+60,288,176)
        # Right: two rows of four cells, one knob bank each. PAGE 1: cutoff, resonance, model,
        # style / drive, mix, blend, key tracking. PAGE 2: comb offset and the formant position /
        # formant resonance and spread, the input switch; its fourth knob is the on switch at the left.
        for section,items in enumerate(pages):
            shown=[f'IndexedEnabling/{section}/32/'+handle('UI Section')]
            for n in range(8):
                name=items[f*8+n];assert name.startswith(p),name
                if name==p+'on':continue
                row,col=divmod(n,4);x=344+col*230;y=[128,352][f]+row*108;slot=f*8+n   # under the two page buttons; the picture of filter 1 starts at 128 as well
                caption=FILTER_CAPTIONS[name[len(p):]]
                if '%d' in caption:caption=caption%(2-f)
                k=kind(name);own=[('Data','WithName['+caption+']/'+handle(name))]
                # A switch or a list is bound to its own parameter, a dial to its Q-Link slot.
                if k=='switch':key='flt-row-switch';data=own
                elif k=='list':key='flt-row-knob' if name.endswith('_model') else 'flt-rowlist-knob';data=own   # the names of the models are short and stand as large as a value
                else:key='flt-row-'+k;data=slot_data(caption,name,slot)
                e=g.entry(caption,key,{'version':1,'handleName':'Data'},f'{x} {y} 222 100','Yes','Hide','Always',data,invalidating=shown)
                dict(dict(e)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))));widgets.append(e)
            for bank in range(2):
                names=items[f*8+bank*4:f*8+bank*4+4];cells=len([name for name in names if name!=p+'on'])
                frames.append(frame(344,[128,352][f]+bank*108,cells*230-8,100,section,f*2+bank))
                if p+'on' in names:frames.append(frame(24,y0,96,52,section,f*2+bank))
    # Frames first: they lie under the controls and take no touch.
    children.extend(frames);children.extend(fixed);children.extend(widgets)
def sample_bars(children,x,y,w,h,fine=False):
    # A column is a bar around the middle line whose half height is the level of that slice
    # of the sample. 128 slices on the OSC overview, 256 (their own readouts) on the SAMPLE page.
    columns,levels=(256 if fine else 128),GRAPH['kWaveLevels'];assert w%columns==0;cw=w//columns
    name=f'samplebar-{cw}x{h}-{levels}.png'
    children.append(rect('graph background',(x-8,y-4,w+16,h+8),'ff1c1f29'))
    for i in range(columns):
        data={'version':2,'imageType':'FilmStrip','colour':'0','image':name,'numFrames':levels,'handleName':handle(f'display_sample_fine_{i}' if fine else f'display_sample_{i}'),'animated':False}
        children.append(g.entry('sample column '+str(i),'Image',data,f'{x+i*cw} {y} {cw} {h}','No','Show','Always'))
    strip=Image.new('RGBA',(cw,h*levels));d=ImageDraw.Draw(strip);rgb=tuple(bytes.fromhex(COLORS['osc'][2:]));mid=h//2
    # A lightly filled bar with a bright upper and lower edge (user, 0.3.31: the built-in noise
    # sample, which is equally loud everywhere, was one glaring block when the bars were solid).
    for k in range(levels):
        half=round(k/(levels-1)*(mid-2))
        if half>2:d.rectangle((0,k*h+mid-half,cw-1,k*h+mid+half),fill=rgb+(64,))
        d.rectangle((0,k*h+mid-half,cw-1,k*h+mid-half+1),fill=rgb+(255,))
        d.rectangle((0,k*h+mid+half-1,cw-1,k*h+mid+half),fill=rgb+(255,))
    strip.save(skin/name)
# ---- ENVELOPE page (0.3.33): no name line, no sections, a larger picture, nine dials -------
for size,w,h,strip_name,knob,xy,name_size,value_size in [('big',172,164,'large-env-ROLE.png',112,(30,24),22,24),('mid',86,124,'small-env-ROLE.png',80,(3,22),18,18)]:
    dial_component(f'env-{size}-ROLE',w,h,strip_name,knob,xy,name_size,value_size,True,'env')
ENV_CAPTIONS={'attack':'Attack','decay':'Decay','sustain':'Sustain','release':'Release','delay':'Delay','hold':'Hold','attack_power':'A.Curve','decay_power':'D.Curve','release_power':'R.Curve'}
def env_page(children,view):
    items=view['sections'][0]['items'];assert len(items)==9 and len(view['sections'])==1
    section=[f'IndexedEnabling/0/32/'+handle('UI Section')]
    # The picture takes the height that the name line and the section buttons had. The tab
    # row above already says which envelope this is. 0.3.34 (user on the device: the envelope
    # should be more stretched): the plugin draws it, in columns like the other pictures, and
    # it always fills the width. The MPC's own envelope picture is no longer used here.
    x,y,w,h=32,76,936,288;levels=GRAPH['kWaveLevels'];assert w%ENV_COLUMNS==0;cw=w//ENV_COLUMNS
    name=f'envbar-{cw}x{h}-{levels}.png'
    children.append(rect('graph background',(x-8,y-4,w+16,h+8),'ff1c1f29'))
    for part in (1,2,3):children.append(rect('level line',(x,y+part*h//4,w,1),'ff292d39'))
    # Two pictures in the same place, chosen with SHAPE / DRAG beside the dials: the drawn one,
    # and the MPC's own envelope with its points for the finger (it moves while you drag, but
    # gives each stage a fixed slot, so an ordinary envelope fills only part of it).
    drawn=['IndexedEnabling/0/2/'+handle('UI Env View')];dragged=['IndexedEnabling/1/2/'+handle('UI Env View')]
    for i in range(ENV_COLUMNS):
        data={'version':2,'imageType':'FilmStrip','colour':'0','image':name,'numFrames':levels,'handleName':handle(f'display_env_shape_{i}'),'animated':False}
        children.append(g.entry('envelope column '+str(i),'Image',data,f'{x+i*cw} {y} {cw} {h}','No','Hide','Always',invalidating=drawn))
    native=[];envelope(native)
    for e in native:
        if dict(dict(e)['componentData'])['type']!='Envelope':continue
        b=dict(e)['bounds'];b[:]=[(k,f'{x} {y} {w} {h}' if k=='bounds' else v) for k,v in b]
        children.append(conditional(e,dragged))
    children.append(nav_button('SHAPE',1164,412,88,52,'UI Env View',0,2,'env'))
    children.append(nav_button('DRAG',1164,472,88,52,'UI Env View',1,2,'env'))
    strip=Image.new('RGBA',(cw,h*levels));d=ImageDraw.Draw(strip);rgb=tuple(bytes.fromhex(COLORS['env'][2:]))
    for k in range(levels):
        level=round((1-k/(levels-1))*(h-4))
        d.rectangle((0,k*h+level,cw-1,k*h+h-1),fill=rgb+(55,))
        d.rectangle((0,k*h+level,cw-1,k*h+level+3),fill=rgb+(255,))
    strip.save(skin/name)
    children.append(trace('Envelope output',(1000,72,248,296),'display_env_value','env'))
    # One row: attack, decay, sustain and release large, the five others medium.
    cells=[]
    for slot,name in enumerate(items):
        key=name[len('Env '):];caption=ENV_CAPTIONS[key];role='bipolar' if key.endswith('_power') else 'knob'
        if slot<4:x,y,size,w,h=24+slot*172,388,'big',172,164
        else:x,y,size,w,h=724+(slot-4)*86,408,'mid',86,124
        cell=g.entry(caption,f'env-{size}-{role}',{'version':1,'handleName':'Data'},f'{x} {y} {w} {h}','Yes','Hide','Always',slot_data(caption,name,slot),invalidating=section)
        dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))))
        cells.append(cell)
    # Frames first: they lie under the controls and take no touch.
    children.extend(qlink_frames(cells,0,'env'));children.extend(cells)
# ---- FX (0.3.36): the overview in chain order, every effect on one page -------------------
from surface_spec import FX_PAGES
# 0.3.46: the dials of the effect pages in the effects' colour. Up to 0.3.42 they were the
# filter's dials (both blue); with the filter's own colour (0.3.43) the effects showed pink dials.
for touch in [False,True]:
    suffix='-touch' if touch else ''
    dial_component('fx-big-ROLE'+suffix,172,164,'large-fx-ROLE.png',112,(30,24),22,24,not touch,'fx')
    dial_component('fx-mid-ROLE'+suffix,86,124,'small-fx-ROLE.png',80,(3,22),18,18,not touch,'fx')
dial_component('fx-list-ROLE',86,124,'small-fx-ROLE.png',80,(3,22),18,15,True,'fx')
dial_component('fx-biglist-ROLE',172,164,'large-fx-ROLE.png',112,(30,24),22,22,True,'fx')
dial_component('fx-wide-ROLE',172,124,'small-fx-ROLE.png',80,(46,22),18,16,True,'fx')
for state in ['off','on']:
    image=Image.new('RGBA',(64,52),(0,0,0,0));d=ImageDraw.Draw(image);rgb=tuple(bytes.fromhex(COLORS['fx'][2:]))
    d.rounded_rectangle((2,10,61,41),radius=16,fill=rgb if state=='on' else '#454c61');x=46 if state=='on' else 18;d.ellipse((x-11,15,x+11,37),fill='#181a23' if state=='on' else '#edf0f7')
    image.save(skin/f'tiny-fx-{state}.png')
definitions.append(g.obj(('key','fx-switch'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['fx']),'0 0 86 124','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,18,'horizontallyCentred verticallyCentred'),(0,0,86,22)),
    ent('switch','Button',g.button_data('tiny-fx-on.png','tiny-fx-off.png',1,1),(11,36,64,52)),
    ent('state','Label',g.label_data('Value',COLORS['fx'],16,'horizontallyCentred verticallyCentred'),(0,98,86,24)),
],'00000000'))))
# One place of the chain on the overview: the switch of the effect that stands there.
definitions.append(g.obj(('key','fx-slot'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['fx']),'0 0 132 100','No','Show','WhenFocussed'),
    ent('Effect switch','Button',g.button_data('fx-slot-on.png','fx-slot-off.png',1,1),(0,0,132,72)),
    ent('state','Label',g.label_data('Value',COLORS['fx'],18,'horizontallyCentred verticallyCentred'),(0,74,132,24)),
],'00000000'))))
for state in ['off','on']:
    image=Image.new('RGBA',(132,72),(0,0,0,0));d=ImageDraw.Draw(image);rgb=tuple(bytes.fromhex(COLORS['fx'][2:]))
    d.rounded_rectangle((6,6,125,65),radius=30,fill=rgb if state=='on' else '#454c61');x=94 if state=='on' else 38;d.ellipse((x-22,14,x+22,58),fill='#181a23' if state=='on' else '#edf0f7')
    image.save(skin/f'fx-slot-{state}.png')
FX_CAPTIONS={'drive':'Drive','mix':'Mix','dry_wet':'Mix','type':'Type','filter_order':'Filter Pos','filter_cutoff':'Cutoff','filter_resonance':'Reso','filter_blend':'Blend',
    'frequency':'Rate','mod_depth':'Depth','feedback':'Feedback','voices':'Voices','delay_1':'Delay 1','delay_2':'Delay 2','spread':'Spread','cutoff':'Cutoff','sync':'Sync','tempo':'Tempo',
    'center':'Center','phase_offset':'Phase','blend':'Blend','style':'Style','aux_frequency':'Rate 2','aux_sync':'Sync 2','aux_tempo':'Tempo 2','filter_spread':'Spread',
    'decay_time':'Decay','size':'Size','delay':'Pre Delay','chorus_amount':'Chorus','chorus_frequency':'Ch.Rate','pre_low_cutoff':'Low Cut','pre_high_cutoff':'High Cut',
    'low_shelf_cutoff':'Low Freq','low_shelf_gain':'Low Gain','high_shelf_cutoff':'High Freq','high_shelf_gain':'High Gain','attack':'Attack','release':'Release','enabled_bands':'Bands',
    'low_gain':'Low','band_gain':'Mid','high_gain':'High','resonance':'Reso','keytrack':'Key Trk','model':'Model','blend_transpose':'Comb Ofs','formant_x':'Form X','formant_y':'Form Y',
    'formant_transpose':'F.Pitch','formant_resonance':'F.Reso','formant_spread':'F.Spread'}
for band in ['low','band','high']:
    FX_CAPTIONS.update({f'{band}_upper_threshold':'Up Thr',f'{band}_lower_threshold':'Lo Thr',f'{band}_upper_ratio':'Up Ratio',f'{band}_lower_ratio':'Lo Ratio',f'{band}_cutoff':'Freq',f'{band}_resonance':'Reso',f'{band}_mode':'Mode'})
FX_CAPTIONS_BY_EFFECT={'eq':{'low_gain':'Gain','band_gain':'Gain','high_gain':'Gain'},'distortion':{'filter_order':'Position'},'delay':{'frequency':'Time','aux_frequency':'Time 2'}}
FX_SIZES={'big':(172,164),'mid':(86,124),'wide':(172,124),'switch':(86,124)}
def fx_page(children,view):
    effect=view['effect'];layers=FX_PAGES[effect];prefix=effect+'_';two=len(layers)>1
    def layer(n):return [f'IndexedEnabling/{n}/32/'+handle('UI Section')]
    def frame(x,y,w,h,section,bank,m=4):
        return g.entry('Q-Link frame','Image',{'version':1,'colour':'0','image':frame_image(w+2*m,h+2*m,'fx')},f'{x-m} {y-m} {w+2*m} {h+2*m}','No','Hide','Always',
            invalidating=[f'IndexedEnabling/{section}/32/'+handle('UI Section'),f'IndexedEnabling/{bank}/4/'+handle('UI Bank')])
    def place(key,x,y,size,slot,own_layer):
        name=prefix+key;caption=FX_CAPTIONS_BY_EFFECT.get(effect,{}).get(key,FX_CAPTIONS[key]);p=parameter(name);own=[('Data','WithName['+caption+']/'+handle(name))]
        w,h=FX_SIZES[size];bounds=f'{x} {y} {w} {h}';target=g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}')))
        def entry(component,data,visible):
            if visible:return g.entry(caption,component,{'version':1,'handleName':'Data'},bounds,'Yes','Hide','Always',data,invalidating=visible)
            return g.entry(caption,component,{'version':1,'handleName':'Data'},bounds,'Yes','Hide','Always',data)
        fixed=None
        if p['kind']=='bool' or size=='switch':assert size=='switch' and len(p.get('choices',[]))<=2,name;fixed='fx-switch'   # also a list of two, such as an EQ band's mode
        elif len(p.get('choices',[]))>1:fixed={'big':'fx-biglist-knob','mid':'fx-list-knob'}[size]
        if fixed:
            # A switch or a list keeps its own parameter (its steps and names belong to it).
            e=entry(fixed,own,None);dict(dict(e)['handle remapping'])['map'].append(target);return [e]
        role='bipolar' if abs(p['default']-.5)<.0001 and p['display'].startswith('0') and not key.startswith('formant_') else 'knob'
        q=entry(f'fx-{size}-{role}',slot_data(caption,name,slot),layer(own_layer))
        dict(dict(q)['handle remapping'])['map'].append(target)
        # With two layers a dial also exists for the finger while the other layer is active.
        return [q]+([entry(f'fx-{size}-{role}-touch',own,layer(1-own_layer))] if two else [])
    fixed=[nav_button(('OFF','ON'),24,68,96,52,prefix+'on',color='fx'),label(view['title'].upper(),(132,68,420,52),TEXT,24)]
    if two:fixed+=[nav_button('Q-LINKS MAIN',904,68,172,52,'UI Section',0,32,'fx'),nav_button('Q-LINKS MORE',1084,68,172,52,'UI Section',1,32,'fx')]
    # Rows of groups as the page model lists them. A group is one knob bank and one frame.
    frames=[];widgets=[];y=160
    for layer_index,rows in enumerate(layers):
        bank=0
        for row in rows:
            x=24;row_height=0
            for title,items in row:
                width=sum(FX_SIZES[size][0] for _,size in items)
                assert x+width<=1256,(effect,title,x+width)
                if title:fixed.append(label(title,(x+4,y-28,max(width-8,160),24),NAME,17))
                run=None
                for position,(key,size) in enumerate(items):
                    w,h=FX_SIZES[size];widgets+=place(key,x,y,size,bank*4+position,layer_index)
                    if run and run[3]==h:run[2]+=w
                    else:
                        if run:frames.append(frame(run[0],run[1],run[2],run[3],layer_index,bank))
                        run=[x,y,w,h]
                    x+=w;row_height=max(row_height,h)
                frames.append(frame(run[0],run[1],run[2],run[3],layer_index,bank));x+=16;bank+=1
            y+=row_height+44
    assert y-44<=560,(effect,y-44)
    # Frames first: they lie under the controls and take no touch.
    children.extend(frames);children.extend(fixed);children.extend(widgets)
def fx_overview(children,view):
    items=view['sections'][0]['items'];assert len(items)==9
    section=[f'IndexedEnabling/0/32/'+handle('UI Section')]
    # The chain from left to right: per place its number (tap = choose this place), the name of
    # the effect that stands there, and that effect's switch. Below: move the chosen effect.
    children.append(label('SIGNAL FLOW, LEFT TO RIGHT',(28,76,600,28),NAME,18))
    cells=[]
    for i,name in enumerate(items):
        x=28+136*i
        children.append(nav_button(str(i+1),x,116,132,56,'FX Position',i,9,'fx'))
        children.append(ent('Effect name','Label',g.label_data('Value',TEXT,21,'horizontallyCentred verticallyCentred'),(x,180,132,40),remap=[('Data',handle(f'display_fx_slot_{i}'))]))
        cell=g.entry(f'Slot {i+1}','fx-slot',{'version':1,'handleName':'Data'},f'{x} 228 132 100','Yes','Hide','Always',[('Data','WithName[Slot '+str(i+1)+']/'+handle(name))],invalidating=section)
        dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {i+1}'))))
        cells.append(cell)
    children.append(label('MOVE THE CHOSEN EFFECT',(28,364,600,28),NAME,18))
    children.append(nav_button('< EARLIER',28,400,268,64,'UI FX Move',1,3,'fx'))
    children.append(nav_button('LATER >',304,400,268,64,'UI FX Move',2,3,'fx'))
    children.extend(qlink_frames(cells,0,'fx'));children.extend(cells)
# ---- LFO page (0.3.35): one page, a narrower picture, rate and shape large ----------------
# These sizes are also the dials of HOME > OTHER and HOME > ADVANCED.
dial_component('lfo-big-ROLE',172,164,'large-mod-ROLE.png',112,(30,24),22,24,True,'mod')
dial_component('lfo-mid-ROLE',86,124,'small-mod-ROLE.png',80,(3,22),18,18,True,'mod')
# Lists keep their own parameter: a large one for the shape, medium ones with a smaller value
# line for the sync settings, and a wide cell for the name of a shape file.
dial_component('lfo-biglist-ROLE',172,164,'large-mod-ROLE.png',112,(30,24),22,22,True,'mod')
dial_component('lfo-list-ROLE',86,124,'small-mod-ROLE.png',80,(3,22),18,15,True,'mod')
# 0.3.42: the LFO page itself uses one size, a large dial in a cell a quarter of the right
# half wide (156), so four fit beside the picture and three under it. A list has a smaller
# value line (its longest entry is "Sustain Envelope"); the shape file takes two cells.
dial_component('lfo-cell-ROLE',156,164,'large-mod-ROLE.png',112,(22,24),22,22,True,'mod')
dial_component('lfo-celllist-ROLE',156,164,'large-mod-ROLE.png',112,(22,24),22,18,True,'mod')
dial_component('lfo-file-ROLE',312,164,'large-mod-ROLE.png',112,(100,24),22,20,True,'mod')
definitions.append(g.obj(('key','lfo-cell-switch'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['mod']),'0 0 156 164','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,22,'horizontallyCentred verticallyCentred'),(0,0,156,24)),
    ent('switch','Button',g.button_data('large-mod-on.png','large-mod-off.png',1,1),(10,44,136,72)),
    ent('state','Label',g.label_data('Value',COLORS['mod'],22,'horizontallyCentred verticallyCentred'),(0,136,156,28)),
],'00000000'))))
for state in ['off','on']:
    image=Image.new('RGBA',(64,52),(0,0,0,0));d=ImageDraw.Draw(image);rgb=tuple(bytes.fromhex(COLORS['mod'][2:]))
    d.rounded_rectangle((2,10,61,41),radius=16,fill=rgb if state=='on' else '#454c61');x=46 if state=='on' else 18;d.ellipse((x-11,15,x+11,37),fill='#181a23' if state=='on' else '#edf0f7')
    image.save(skin/f'tiny-mod-{state}.png')
definitions.append(g.obj(('key','lfo-switch'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['mod']),'0 0 86 124','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,18,'horizontallyCentred verticallyCentred'),(0,0,86,22)),
    ent('switch','Button',g.button_data('tiny-mod-on.png','tiny-mod-off.png',1,1),(11,36,64,52)),
    ent('state','Label',g.label_data('Value',COLORS['mod'],16,'horizontallyCentred verticallyCentred'),(0,98,86,24)),
],'00000000'))))
LFO_CAPTIONS={'LFO frequency':'Frequency','LFO Shape':'Shape','LFO tempo':'Tempo','LFO sync':'Sync','LFO sync_type':'Mode','LFO phase':'Phase','LFO stereo':'Stereo','LFO delay_time':'Delay',
    'LFO fade_time':'Fade','LFO smooth_mode':'Smooth','LFO smooth_time':'Smooth Time','Smooth Curve':'Smooth Curve','Point':'Point','Point Time':'Point Time','Point Level':'Point Level','Point Curve':'Point Curve',
    'Point Count':'Points','LFO File':'Shape File','LFO keytrack_transpose':'Key Trans','LFO keytrack_tune':'Key Tune'}
def lfo_page(children,view):
    pages=[section['items'] for section in view['sections']];assert [len(items) for items in pages]==[11,10]
    def frame(x,y,w,h,section,bank,m=4):
        return g.entry('Q-Link frame','Image',{'version':1,'colour':'0','image':frame_image(w+2*m,h+2*m,'mod')},f'{x-m} {y-m} {w+2*m} {h+2*m}','No','Hide','Always',
            invalidating=[f'IndexedEnabling/{section}/32/'+handle('UI Section'),f'IndexedEnabling/{bank}/4/'+handle('UI Bank')])
    # Left: the picture, 576 x 288; it stays while the page of controls changes. One knob bank
    # per row: two rows of four large dials beside the picture, the third bank under it.
    # PAGE 1: frequency, shape, tempo, sync / mode, phase, stereo, delay / fade, smoothing.
    # PAGE 2: the point editor / point count, curve smoothing, shape file / key tracking.
    curve_bars(children,32,76,576,288,step=2)
    # The two pages of controls, as on an oscillator page (where the layer buttons stood up to 0.3.41).
    fixed=[]   # 0.3.49: PAGE 1 / PAGE 2 are two small buttons at the top right of the page, see page_buttons
    rows=[(632,126),(632,298),(32,380)];frames=[];widgets=[]
    for section,items in enumerate(pages):
        shown=[f'IndexedEnabling/{section}/32/'+handle('UI Section')]
        for bank,(x0,y) in enumerate(rows):
            x=x0;names=items[bank*4:bank*4+4]
            assert all(names[:len([n for n in names if n])]),(section,bank,'an unused knob stands at the end of its bank')
            for index,name in enumerate(names):
                if not name:continue
                slot=bank*4+index;caption=LFO_CAPTIONS[name];p=parameter(name);own=[('Data','WithName['+caption+']/'+handle(name))];w=156
                # A switch, a list and the file selector are bound to their own parameter
                # (their steps and labels belong to it); a dial is bound to its Q-Link slot.
                if p['kind']=='bool':key='lfo-cell-switch';data=own
                elif name=='LFO File':key='lfo-file-knob';data=own;w=312
                elif len(p.get('choices',[]))>1:key='lfo-celllist-knob';data=own
                else:key='lfo-cell-'+('bipolar' if abs(p['default']-.5)<.0001 and p['display'].startswith('0') else 'knob');data=slot_data(caption,name,slot)
                e=g.entry(caption,key,{'version':1,'handleName':'Data'},f'{x} {y} {w} 164','Yes','Hide','Always',data,invalidating=shown)
                dict(dict(e)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))));widgets.append(e);x+=w
            if x>x0:frames.append(frame(x0,y,x-x0,164,section,bank))
    # Frames first: they lie under the controls and take no touch.
    children.extend(frames);children.extend(fixed);children.extend(widgets)
# ---- HOME > OTHER and HOME > ADVANCED (0.3.40) ----------------------------------------------
# 0.3.38 put random, expression and the route curve on OTHER, in two Q-Link layers, while
# ADVANCED held three controls. User, 0.3.39: split them so it makes sense, no second layer.
# OTHER: the random sources and the expression controls, large dials, four knob banks.
# ADVANCED: access to any parameter, Live Graphics, and the curve of a route with its point
# editor under the picture, three knob banks.
definitions.append(g.obj(('key','mod-big-switch'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['mod']),'0 0 172 164','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,22,'horizontallyCentred verticallyCentred'),(0,0,172,24)),
    ent('switch','Button',g.button_data('large-mod-on.png','large-mod-off.png',1,1),(18,44,136,72)),
    ent('state','Label',g.label_data('Value',COLORS['mod'],22,'horizontallyCentred verticallyCentred'),(0,136,172,28)),
],'00000000'))))
OTHER_CAPTIONS={'Random Select':'Random','Random frequency':'Frequency','Random sync':'Sync','Random tempo':'Tempo','Random style':'Style','Random stereo':'Stereo','Random sync_type':'Sync Type',
    'Random keytrack_transpose':'Key Trans','Random keytrack_tune':'Key Tune','mod_wheel':'Mod Wheel','pitch_wheel':'Pitch Wheel','voice_amplitude':'Amplitude',
    'Route Select':'Route','Curve':'Curve','Point Count':'Points','Smooth Curve':'Sm.Curve','Point':'Point','Point Time':'P.Time','Point Level':'P.Level','Point Curve':'P.Curve'}
def mod_cell(name,x,y,size,slot):
    # One control in one Q-Link layer: a dial writes its Q-Link slot, a switch or a list its own parameter.
    caption=OTHER_CAPTIONS[name];p=parameter(name);w,h={'big':(172,164),'mid':(86,124)}[size]
    if p['kind']=='bool':key={'big':'mod-big-switch','mid':'lfo-switch'}[size];data=[('Data','WithName['+caption+']/'+handle(name))]
    elif len(p.get('choices',[]))>1:key={'big':'lfo-biglist-knob','mid':'lfo-list-knob'}[size];data=[('Data','WithName['+caption+']/'+handle(name))]
    else:key=f'lfo-{size}-'+('bipolar' if abs(p['default']-.5)<.0001 and p['display'].startswith('0') else 'knob');data=slot_data(caption,name,slot)
    e=g.entry(caption,key,{'version':1,'handleName':'Data'},f'{x} {y} {w} {h}','Yes','Hide','Always',data)
    dict(dict(e)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))));return e
def mod_frame(x,y,w,h,bank,color='mod',m=4):
    return g.entry('Q-Link frame','Image',{'version':1,'colour':'0','image':frame_image(w+2*m,h+2*m,color)},f'{x-m} {y-m} {w+2*m} {h+2*m}','No','Hide','Always',
        invalidating=[f'IndexedEnabling/0/32/'+handle('UI Section'),f'IndexedEnabling/{bank}/4/'+handle('UI Bank')])
def mod_groups(groups,items,size):
    frames=[];fixed=[];widgets=[]
    for bank,title,x,y,count in groups:
        w,h={'big':(172,164),'mid':(86,124)}[size]
        for position in range(count):widgets.append(mod_cell(items[bank*4+position],x+position*w,y,size,bank*4+position))
        assert all(name=='' for name in items[bank*4+count:bank*4+4]),title
        frames.append(mod_frame(x,y,count*w,h,bank));fixed.append(label(title,(x+4,y-28,max(count*w-8,200),24),NAME,18))
    return frames,fixed,widgets
def other_page(children,view):
    items=view['sections'][0]['items'];assert len(view['sections'])==1 and len(items)==15
    frames,fixed,widgets=mod_groups([(0,'RANDOM',24,108,4),(1,'STYLE',736,108,3),(2,'KEY TRACKING',24,356,2),(3,'EXPRESSION',736,356,3)],items,'big')
    # Frames first: they lie under the controls and take no touch.
    children.extend(frames);children.extend(fixed);children.extend(widgets)
def advanced_page(children,view,color):
    items=view['sections'][0]['items'];assert len(view['sections'])==1 and len(items)==12 and items[3]==''
    # Left: the curve of the chosen route with its controls below. Right: any parameter by
    # name with its value, and the Live Graphics switch.
    curve_bars(children,32,76,576,288,step=2)
    frames,fixed,widgets=mod_groups([(1,'CURVE OF A ROUTE',24,404,4),(2,'ITS POINT',384,404,4)],items,'mid')
    section=[f'IndexedEnabling/0/32/'+handle('UI Section')]
    for slot,(name,caption,x,y) in enumerate([('Parameter','Parameter',648,104),('Value','Value',952,104),('Live Graphics','Live Graphics',952,280)]):
        cell=control(name,caption,x,y,color,visible=section,slot=slot)
        dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))));widgets.append(cell)
    frames+=[mod_frame(648,104,600,156,0,color,6),mod_frame(952,280,296,156,0,color,6)]
    fixed.append(label('ANY PARAMETER',(652,74,300,24),NAME,18))
    children.extend(frames);children.extend(fixed);children.extend(widgets)
# ---- SAMPLE page (0.3.31): picture, list button, every control on one page ---------------
# A switch at the width of a large dial, and a medium dial in a cell of that width (the
# names of the paths need the room).
definitions.append(g.obj(('key','smp-switch'),('value',g.component_definition([g.action('Mouse Down','Q-Link','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['osc']),'0 0 172 124','No','Show','WhenFocussed'),
    ent('name','Label',g.label_data('Name',NAME,20,'horizontallyCentred verticallyCentred'),(0,0,172,24)),
    ent('switch','Button',g.button_data('large-osc-on.png','large-osc-off.png',1,1),(18,26,136,72)),
    ent('state','Label',g.label_data('Value',COLORS['osc'],20,'horizontallyCentred verticallyCentred'),(0,98,172,26)),
],'00000000'))))
dial_component('smp-wide-ROLE',172,124,'small-osc-ROLE.png',80,(46,22),18,18,True)
SAMPLE_CAPTIONS={'level':'Level','pan':'Pan','transpose':'Pitch','tune':'Tune','loop':'Loop','bounce':'Bounce','keytrack':'Key Trk','random_phase':'Rand Phase','destination':'Path','transpose_quantize':'Quantize'}
def sample_page(children,view):
    items=view['sections'][0]['items'];assert len(items)==10 and len(view['sections'])==1
    section=[f'IndexedEnabling/0/32/'+handle('UI Section')]
    # Left: on/off and the name of the page, the picture of the sample, the list of samples.
    children.append(nav_button(('OFF','ON'),24,68,96,52,'sample_on',color='osc'))
    children.append(label('SAMPLE',(132,68,216,52),TEXT,24))
    sample_bars(children,32,128,512,288,fine=True)
    # While the sampler is off its picture is dimmed: a dark, see-through sheet over it.
    sheet=Image.new('RGBA',(512,288),(28,31,41,170));sheet.save(skin/'sample-off-512x288.png')
    children.append(g.entry('sample off','Image',{'version':1,'colour':'0','image':'sample-off-512x288.png'},'32 128 512 288','No','Hide','Always',invalidating=['IndexedEnabling/0/2/'+handle('sample_on')]))
    children.append(nav_button('SAMPLE LIST',32,436,512,60,'UI Browse',4,5))
    # Right: level, pan, pitch and tune large / the four switches / path and pitch quantise,
    # and beside them the name of the loaded sample.
    cells=[]
    for slot,name in enumerate(items):
        row,col=divmod(slot,4);caption=SAMPLE_CAPTIONS[name[len('sample_'):]];p=parameter(name)
        x=568+col*172;y=[70,242,374][row];own=[('Data','WithName['+caption+']/'+handle(name))]
        if p['kind']=='bool':key,data,size='smp-switch',own,(172,124)
        elif len(p.get('choices',[]))>1:key,data,size='smp-wide-knob',own,(172,124)      # a list keeps its own parameter
        else:
            role='bipolar' if abs(p['default']-.5)<.0001 and p['display'].startswith('0') else 'knob'
            key,data,size=('osc-big-'+role,slot_data(caption,name,slot),(172,164)) if row==0 else ('smp-wide-'+role,slot_data(caption,name,slot),(172,124))
        cell=g.entry(caption,key,{'version':1,'handleName':'Data'},f'{x} {y} {size[0]} {size[1]}','Yes','Hide','Always',data,invalidating=section)
        dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))))
        cells.append(cell)
    children.append(rect('name field',(912,374,344,124),'ff1e212e'))
    children.append(label('Loaded sample',(928,386,312,22),NAME,17))
    children.append(label('Sample name',(928,414,320,44),COLORS['osc'],26,value='display_sample_name'))
    # Frames first: they lie under the controls and take no touch.
    children.extend(qlink_frames(cells,0,'osc'));children.extend(cells)
def tap_area(name,x,y,w,h,param,value,count):
    # A button without a picture of its own (0.3.52): what lies under it is the button.
    file=f'tap-{w}x{h}.png';Image.new('RGBA',(w,h),(0,0,0,0)).save(skin/file)
    data={'version':2,'onImage':file,'offImage':file,'buttonId':value,'numButtonsInGroup':count,'handleName':handle(param),'gestureBehaviour':'Instant'}
    return ent(name,'Button',data,(x,y,w,h),True)
CENTRED='horizontallyCentred verticallyCentred'
def osc_overview(children,view):
    items=view['sections'][0]['items'];assert len(items)==12
    section=[f'IndexedEnabling/0/32/'+handle('UI Section')]
    # 0.3.52 (user): the wavetable of an oscillator is changed here with two arrows, and a tap on
    # its picture opens the page of the 32 factory tables. Each column from the top: name and
    # switch, the picture, the name of the table between its two arrows, the three dials. The
    # line 'OSC OVERVIEW' is gone (the tab says it); that is the room the new row takes.
    for col,(name,switch) in enumerate([('OSC 1','osc_1_on'),('OSC 2','osc_2_on'),('OSC 3','osc_3_on'),('SAMPLE','sample_on')]):
        x=24+312*col
        children.append(label(name,(x,68,160,52),TEXT,26))
        children.append(nav_button(('OFF','ON'),x+176,68,120,52,switch,color='osc'))
        if col<3:
            wave_bars(children,col,x+20,136,256,176,step=2)
            children.append(tap_area(f'Tables {col+1}',x+12,132,272,184,'UI Tables',col+1,4))
            children.append(nav_button('<',x,326,64,52,'Table Step',col*2+1,7,'osc'))
            children.append(rect('table field',(x+72,326,152,52),'ff1c1f29'))
            children.append(label('Table name',(x+74,326,148,52),COLORS['osc'],16,value=f'display_table_name_{col}',justify=CENTRED))   # 16: '21 Trans Resonance' has to fit
            children.append(nav_button('>',x+232,326,64,52,'Table Step',col*2+2,7,'osc'))
        else:
            sample_bars(children,x+20,136,256,176)
            children.append(rect('sample field',(x,326,296,52),'ff1c1f29'))
            children.append(label('Sample name',(x+4,326,288,52),COLORS['osc'],18,value='display_sample_name',justify=CENTRED))
    captions={'level':'Level','frame':'Frame','pan':'Pan','transpose':'Pitch'}
    cells=[]
    for slot,name in enumerate(items):
        kind,col=divmod(slot,4);caption=captions[name.split('_')[-1]]
        role='bipolar' if caption in ['Pan','Pitch'] else 'knob'
        cell=g.entry(caption,'mini-'+role+'-osc',{'version':1,'handleName':'Data'},f'{24+312*col+4+96*kind} 394 96 128','Yes','Hide','Always',slot_data(caption,name,slot),invalidating=section)
        dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))))
        cells.append(cell)
    children.extend(qlink_frames(cells,0,'osc'));children.extend(cells)

# ---- The 32 factory wavetables as a table (0.3.52) ----------------------------------------
# User: tapping the picture shows the 32 stock wavetables, in the form of a table with small
# previews, so that more of them fit on the screen. Four columns of eight rows; a row is one
# button: picture, number, name. The picture shows the first frame of the table and, fainter,
# its last, so the way the table travels can be seen. A tap loads the table into the
# oscillator named at the top ('Table Pick'); the page stays, so tables can be tried in turn.
import struct as _struct
from array import array as _array
from surface_spec import FACTORY_TABLES
OVERVIEW_VIEW=next(i for i,v in enumerate(VIEWS) if v['chart']=='oscmix')
def table_frames(number,name):
    # First and last frame of a factory table, or None when the file is not at hand (a source
    # tree without the factory content: the pictures already in skins/ are kept then).
    source=project/'Content/Wavetables/Factory'/f'{number:02d} {name}.wav'
    if not source.exists():return None
    raw=source.read_bytes();at=12;form=None;data=None
    while at+8<=len(raw):
        tag,size=raw[at:at+4],_struct.unpack('<I',raw[at+4:at+8])[0]
        if tag==b'fmt ':form=_struct.unpack('<HHIIHH',raw[at+8:at+24])
        if tag==b'data':data=raw[at+8:at+8+size]
        at+=8+size+(size&1)
    assert form and data and form[1]==1,(source.name,form)
    if form[0]==3 and form[5]==32:values=_array('f',data)
    elif form[0]==1 and form[5]==16:values=[v/32768 for v in _array('h',data)]
    else:raise AssertionError((source.name,form))
    frames=len(values)//2048;assert frames>=1,source.name
    return [list(values[k*2048:(k+1)*2048]) for k in (0,frames-1)]
def table_button(number,name,x,y,w,h):
    files=[f'table-{number:02d}-{state}.png' for state in ['off','on']];frames=table_frames(number,name)
    if frames is not None or not all((skin/f).exists() for f in files):
        picture=None
        if frames:
            scale=3;pw,ph=88*scale,40*scale;picture=Image.new('RGBA',(pw,ph),'#1c1f29');d=ImageDraw.Draw(picture)
            peak=max(1e-6,max(abs(v) for frame in frames for v in frame))
            d.line((0,ph/2,pw,ph/2),fill='#303441',width=scale)
            for frame,color,width in [(frames[1],'#5d5383',2*scale),(frames[0],'#'+COLORS['osc'][2:],2*scale)]:
                d.line([(px,ph/2-frame[min(2047,px*2048//pw)]/peak*(ph/2-3*scale)) for px in range(pw)],fill=color,width=width,joint='curve')
            picture=picture.resize((88,40),Image.Resampling.LANCZOS)
        for file,state in zip(files,['off','on']):
            im=Image.new('RGBA',(w,h),'#373045' if state=='on' else '#222531');d=ImageDraw.Draw(im)
            d.rectangle((0,0,w-1,h-1),outline='#'+(COLORS['osc'] if state=='on' else LINE)[2:],width=2 if state=='on' else 1)
            if picture:im.paste(picture,(6,6))
            else:d.rectangle((6,6,93,45),fill='#1c1f29')
            # Number and name stand on one base line in every row (centred by the height of the
            # letters they would jump by a pixel or two with every g and p).
            small=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),16);d.text((104,h/2+7),f'{number:02d}',font=small,fill='#'+MUTED[2:],anchor='ls')
            size=21;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
            while d.textlength(name,font=font)>w-140 and size>16:size-=1;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
            assert d.textlength(name,font=font)<=w-140,(name,w)
            d.text((132,h/2+7),name,font=font,fill='#edf0f7',anchor='ls');im.save(skin/file)
    data={'version':2,'onImage':files[1],'offImage':files[0],'buttonId':number,'numButtonsInGroup':len(FACTORY_TABLES)+1,'handleName':handle('Table Pick'),'gestureBehaviour':'Instant'}
    return ent(f'{number:02d} {name}','Button',data,(x,y,w,h),True)
# 0.3.53 (user): the page turns to further pages of 32 tables, and for tables nobody knows in
# advance (the user's own) the plugin draws the picture itself. Page 0 keeps the 32 factory
# rows made above. On every other page a row is a component: a button of the size of the
# row with the name as a value label on it, as the rows of LIBRARY are built (a tap anywhere
# on the row reaches the component's action). Its picture is made of parts of a film strip
# side by side (see table_strip), which the plugin fills from the file (showTablePage in
# src/FullTables.inc); frame 0 of the strip is empty.
# 0.3.54 (user, on the pictures of 0.3.53: why is their resolution so much worse than that of
# the factory rows?). A row's picture was 12 parts of two bars each; it is one line now, in 24
# parts of 4 px. A part is the segment from one point of the wave to the next, both in 27
# levels, so neighbouring parts meet and the line is unbroken and smooth.
# 0.3.55, after measuring on the device (see src/FullTables.inc): a film strip costs about two
# microseconds for every frame of every part that uses it, on every page change, and 768
# parameters more cost 1.8 s. So the strip has 11 levels (1 + 11 x 11 frames instead of
# 1 + 27 x 27), and the parts take their values from the 768 values of the three large wave
# pictures, which are not on screen while this page is.
TABLE_ROWS=32;TABLE_LEVELS=11;TABLE_PARTS=24;TABLE_FRAMES=1+TABLE_LEVELS*TABLE_LEVELS
def table_strip():
    name='tableline-4x40.png';scale=4;image=Image.new('RGBA',(4,40*TABLE_FRAMES),(0,0,0,0))
    line='#'+COLORS['osc'][2:];height=lambda level:(3+(TABLE_LEVELS-1-level)/(TABLE_LEVELS-1)*34)*scale
    for a in range(TABLE_LEVELS):
        for b in range(TABLE_LEVELS):
            # drawn four times as large and wider than the part, then cut, so that the ends of the
            # segment are not rounded off where the next part goes on
            big=Image.new('RGBA',(12*scale,40*scale),'#1c1f29');d=ImageDraw.Draw(big)
            d.line((0,20*scale,12*scale,20*scale),fill='#303441',width=scale)
            ya,yb=height(a),height(b);d.line((4*scale-(4*scale),ya-(yb-ya),4*scale,ya,8*scale,yb,12*scale,yb+(yb-ya)),fill=line,width=round(1.9*scale),joint='curve')
            frame=big.crop((4*scale,0,8*scale,40*scale)).resize((4,40),Image.Resampling.LANCZOS)
            image.paste(frame,(0,40*(1+a*TABLE_LEVELS+b)))
    assert image.height<=31104,image.height   # the tallest strip the MPC is known to take
    image.save(skin/name);return name
def table_row_images():
    plain=Image.new('RGBA',(300,52),'#222531');d=ImageDraw.Draw(plain)
    d.rectangle((0,0,299,51),outline='#'+LINE[2:],width=1);d.rectangle((6,6,101,45),fill='#1c1f29');plain.save(skin/'tablerow.png')
    mark=Image.new('RGBA',(300,52),(0,0,0,0));ImageDraw.Draw(mark).rectangle((0,0,299,51),outline='#'+COLORS['osc'][2:],width=2);mark.save(skin/'tablerow-mark.png')
table_row_images()
definitions.append(g.obj(('key','table-row'),('value',g.component_definition([g.action('Mouse Down','Toggle Switch','Data',''),g.action('Enter Pressed','Toggle Switch','Data','')],[
    g.entry('focus','Focus',g.focus_data(COLORS['osc']),'0 0 300 52','No','Show','WhenFocussed'),
    ent('row','Button',g.button_data('tablerow.png','tablerow.png',1,1),(0,0,300,52)),
    ent('Table row name','Label',label_for('Text',TEXT,19),(112,0,182,52)),
],'00000000'))))
def tables_page(children):
    children.append(label('WAVETABLES',(24,68,190,52),TEXT,28))
    for o in range(3):
        children.append(nav_button(f'OSC {o+1}',222+104*o,68,96,52,'Table Target',o+1,4,'osc'))
        # what that oscillator plays now (also a table that is on no page)
        children.append(conditional(label('Table name',(540,68,216,52),COLORS['osc'],18,value=f'display_table_name_{o}'),[f'IndexedEnabling/{o+1}/4/'+handle('Table Target')]))
    # The pages: FACTORY, then the folders that ship with the plugin (FIZMATIC 1 OF 2 ...), then USER 1 OF n; the arrows go round.
    children.append(nav_button('<',764,68,64,52,'Table Page Step',1,3,'osc'))
    children.append(rect('page field',(836,68,160,52),'ff1c1f29'))
    children.append(label('Table page',(840,68,152,52),COLORS['osc'],17,value='display_table_page',justify=CENTRED))
    children.append(nav_button('>',1004,68,64,52,'Table Page Step',2,3,'osc'))
    children.append(nav_button('BACK',1084,68,172,52,'UI Page',OVERVIEW_VIEW,len(VIEWS),'osc'))
    factory=[f'IndexedEnabling/0/32/'+handle('Table Page')]
    for n,name in enumerate(FACTORY_TABLES):
        col,row=divmod(n,8);children.append(conditional(table_button(n+1,name,24+308*col,128+54*row,300,52),factory))
    strip=table_strip()
    for r in range(TABLE_ROWS):
        col,row=divmod(r,8);x,y=24+308*col,128+54*row
        children.append(conditional(g.entry(f'Table row {r+1}','table-row',{'version':1,'handleName':'Data'},f'{x} {y} 300 52','Yes','Show','Always',
            [('Data',handle(f'Table Row {r+1}')),('Text',handle(f'display_table_row_{r}'))]),['IndexedEnabling/1/2/'+handle(f'Table Row On {r+1}')]))
        # Shown with its row only: on the factory page and on an empty row these values hold
        # what the wave pictures left in them.
        shown=['IndexedEnabling/1/2/'+handle(f'Table Row On {r+1}')]
        for part in range(TABLE_PARTS):
            osc,column=divmod(r*TABLE_PARTS+part,GRAPH['kWaveColumns'])   # as waveColumn() in src/FullSynth.h
            source=f'display_wave_{osc}_{column}' if column<96 else f'display_wave_bank_{osc}_{column-96}' if column<192 else f'display_wave_fine_{osc}_{column-192}'
            data={'version':2,'imageType':'FilmStrip','colour':'0','image':strip,'numFrames':TABLE_FRAMES,'handleName':handle(source),'animated':False}
            children.append(g.entry(f'table picture {r+1}','Image',data,f'{x+6+4*part} {y+6} 4 40','No','Hide','Always',invalidating=shown))
        children.append(g.entry('row mark','Image',{'version':1,'colour':'0','image':'tablerow-mark.png'},f'{x} {y} 300 52','No','Hide','Always',invalidating=[f'IndexedEnabling/{r+1}/{TABLE_ROWS+1}/'+handle('Table Row Mark')]))
    # A page of user tables with nothing on it says how it is filled. The text is a picture:
    # it lies over the places of the rows, and a label there would take their taps away
    # (see bake_button_captions), while a picture lets them through.
    hint=Image.new('RGBA',(1232,140),(0,0,0,0));d=ImageDraw.Draw(hint)
    for y,size,color,text in [(0,30,TEXT,'No user wavetables yet.'),(58,21,MUTED,'Load a wavetable in HOME > LIBRARY from a USB stick, an SD card or an SSD.'),
                              (90,21,MUTED,'It is copied to the folder Vitaleon/Wavetables beside the plugin and listed here with its picture.')]:
        font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size);d.text((616,y),text,font=font,fill='#'+color[2:],anchor='ma')
    hint.save(skin/'table-empty.png')
    children.append(g.entry('empty page hint','Image',{'version':1,'colour':'0','image':'table-empty.png'},'24 250 1232 140','No','Hide','Always',invalidating=['IndexedEnabling/1/2/'+handle('Table Empty')]))

# ---- Serum-style modulation matrix and its two-step target picker ------------
MATRIX_VIEW=next(i for i,v in enumerate(VIEWS) if v['chart']=='mxtable')
GROUP_VIEW=next(i for i,v in enumerate(VIEWS) if v['chart']=='pickgroup')
TARGET_VIEW=next(i for i,v in enumerate(VIEWS) if v['chart']=='picktarget')
def strip(name,size,frames,draw):
    if (skin/name).exists():return
    w,h=size;image=Image.new('RGBA',(w,h*frames))
    for k in range(frames):
        frame=Image.new('RGBA',(w,h));draw(ImageDraw.Draw(frame),k/(frames-1));image.paste(frame,(0,k*h))
    image.save(skin/name)
MOD=tuple(bytes.fromhex(COLORS['mod'][2:]))
def draw_fader(d,v):
    d.rounded_rectangle((4,22,116,30),radius=4,fill='#454c61');d.rounded_rectangle((4,22,4+round(v*112),30),radius=4,fill=MOD)
    x=4+round(v*112);d.rounded_rectangle((x-5,8,x+5,44),radius=3,fill='#edf0f7')
def draw_bipolar(d,v):
    d.rounded_rectangle((4,22,146,30),radius=4,fill='#454c61');x=4+round(v*142);mid=75
    d.rectangle((min(x,mid),22,max(x,mid),30),fill=MOD);d.rectangle((mid,16,mid,36),fill='#a0a7b7');d.rounded_rectangle((x-5,8,x+5,44),radius=3,fill='#edf0f7')
strip('mx-bipolar.png',(150,52),127,draw_bipolar)
# 0.3.43 (user): every section has its colour, and the matrix shows what is modulated by what
# in those colours. A label has one fixed colour, so the name of a source and of a target
# exists once per colour; the plugin says which one to show ('Matrix Source Tint n' and
# 'Matrix Target Tint n' for the eight rows on screen: 0 none, 1 OSC, 2 FILTER, 3 ENV, 4 LFO, 5 FX).
from surface_spec import tint_of
SOURCES=catalog_data['sources']
TINTS=[TEXT,COLORS['osc'],COLORS['flt'],COLORS['env'],COLORS['mod'],COLORS['fx']]
for tint,ink in enumerate(TINTS):
    for key,w in [('mx-src',250),('mx-tgt',330)]:
        definitions.append(g.obj(('key',f'{key}-{tint}'),('value',g.component_definition(g.control_actions('value overlay'),[
            g.entry('focus','Focus',g.focus_data(COLORS['mod']),f'0 0 {w} 52','No','Show','WhenFocussed'),
            ent('value','Label',g.label_data('Value',ink,21),(8,4,w-12,44))],'00000000'))))
definitions.append(g.obj(('key','mx-amount'),('value',g.component_definition(g.control_actions('bipolar overlay'),[
    g.entry('focus','Focus',g.focus_data(COLORS['mod']),'0 0 230 52','No','Show','WhenFocussed'),
    ent('fader','Knob',g.knob_filmstrip_data('mx-bipolar.png',127,'Horizontal'),(0,0,150,52)),
    ent('value','Label',g.label_data('Value',COLORS['mod'],21),(156,4,72,44))],'00000000'))))
def matrix_table(children):
    for x,text in [(68,'SOURCE'),(326,'AMOUNT'),(564,'TARGET')]:children.append(label(text,(x,128,200,20),MUTED,15))
    # What the colours of the names stand for.
    for x,w,text,tint in [(902,44,'OSC',1),(950,64,'FILTER',2),(1018,44,'ENV',3),(1066,44,'LFO',4),(1114,36,'FX',5)]:children.append(label(text,(x,128,w,20),TINTS[tint],15))
    for page in range(8):
        show=[f'IndexedEnabling/{page}/32/'+handle('UI Section')]
        for row in range(8):
            route=page*8+row+1;y=148+row*52;knob=handle(f'UI Control {row+1}')
            if row%4==0:children.append(g.entry('Q-Link frame','Image',{'version':1,'colour':'0','image':frame_image(238,216,'mod')},f'322 {y-4} 238 216','No','Hide','Always',invalidating=show+[f'IndexedEnabling/{row//4}/4/'+handle('UI Bank')]))
            children.append(conditional(label(f'{route:02d}',(24,y+4,40,44),MUTED,20),show))
            for tint in range(6):
                children.append(g.entry(f'route {route}',f'mx-src-{tint}',{'version':1,'handleName':'Data'},f'68 {y} 250 52','Yes','Hide','Always',[('Data','WithName[Route '+str(route)+']/'+handle(f'Route {route} Source')),('QTarget',knob)],
                    invalidating=show+[f'IndexedEnabling/{tint}/6/'+handle(f'Matrix Source Tint {row+1}')]))
            amount=g.entry(f'route {route}','mx-amount',{'version':1,'handleName':'Data'},f'326 {y} 230 52','Yes','Hide','Always',[('Data','WithName[Route '+str(route)+']/'+knob),('Target',handle(f'modulation_{route}_amount')),('QTarget',knob)],invalidating=show)
            children.append(amount)
            for tint in range(6):
                children.append(g.entry(f'route {route}',f'mx-tgt-{tint}',{'version':1,'handleName':'Data'},f'564 {y} 330 52','Yes','Hide','Always',[('Data','WithName[Route '+str(route)+']/'+handle(f'Route {route} Target')),('QTarget',knob)],
                    invalidating=show+[f'IndexedEnabling/{tint}/6/'+handle(f'Matrix Target Tint {row+1}')]))
            # One PICK per row: it opens the page with the sources and the groups of targets.
            children.append(conditional(nav_button('PICK',902,y,76,52,'UI Pick',route,65,'mod'),show))
            children.append(conditional(nav_button('BIPOLAR',990,y,88,52,f'modulation_{route}_bipolar',color='mod'),show))
            children.append(conditional(nav_button('STEREO',1082,y,82,52,f'modulation_{route}_stereo',color='mod'),show))
            children.append(conditional(nav_button('MUTE',1168,y,80,52,f'modulation_{route}_bypass',color='fx'),show))
PICK_SOURCES=[['env_1','env_2','env_3','env_4','env_5','env_6','velocity','note'],
              ['lfo_1','lfo_2','lfo_3','lfo_4','lfo_5','lfo_6','lfo_7','lfo_8'],
              ['random_1','random_2','random_3','random_4','random','stereo','mod_wheel','pitch_wheel'],
              ['macro_control_1','macro_control_2','macro_control_3','macro_control_4','aftertouch','slide','lift','note_in_octave']]
assert sorted(name for row in PICK_SOURCES for name in row)==sorted(SOURCES),set(SOURCES)^{name for row in PICK_SOURCES for name in row}
def pick_groups(children):
    # 0.3.43 (user: no way to pick the source; one page, the same PICK button): the page of a
    # route. Above, its source: a tap sets it and the page stays, so the target can follow.
    # Below, the groups of targets: a group opens its entries, and an entry returns to the matrix.
    children.append(label('for route',(470,72,110,38),MUTED,24));children.append(label('Route',(580,72,70,38),COLORS['mod'],30,value='Route Select'))
    children.append(nav_button('NO SOURCE',756,76,156,52,'Source',0,len(SOURCES)+1,'fx'))
    children.append(nav_button('NO TARGET',924,76,156,52,'Target',0,len(DESTINATIONS)+1,'fx'))
    children.append(nav_button('BACK',1092,76,156,52,'UI Page',MATRIX_VIEW,len(VIEWS),'mod'))
    children.append(label('SOURCE',(24,131,400,20),MUTED,15))
    for row,names in enumerate(PICK_SOURCES):
        for col,name in enumerate(names):
            tint=tint_of(name);children.append(nav_button(source_name(name),24+col*155,152+row*56,147,52,'Source',SOURCES.index(name)+1,len(SOURCES)+1,'mod',ink=TINTS[tint] if tint else None))
    children.append(label('TARGET',(24,375,400,20),MUTED,15))
    for k,(name,items) in enumerate(TARGET_GROUPS):
        row,col=divmod(k,9);tints={tint_of(d) for d in items};tint=next(iter(tints)) if len(tints)==1 else 0
        children.append(nav_button(name,24+col*137,396+row*56,129,52,'UI Section',k+1,32,'mod',ink=TINTS[tint] if tint else None))
def pick_targets(children):
    children.append(nav_button('BACK',1092,76,156,52,'UI Page',MATRIX_VIEW,len(VIEWS),'mod'))
    children.append(nav_button('GROUPS',924,76,156,52,'UI Page',GROUP_VIEW,len(VIEWS),'mod'))
    for k,(name,items) in enumerate(TARGET_GROUPS):
        show=[f'IndexedEnabling/{k+1}/32/'+handle('UI Section')]
        children.append(conditional(label(name,(470,72,440,38),COLORS['mod'],30),show))
        for n,destination in enumerate(items):
            row,col=divmod(n,4);caption=target_name(destination)
            tint=tint_of(destination);children.append(conditional(nav_button(caption,24+col*308,140+row*52,300,52,'Target',DESTINATIONS.index(destination)+1,len(DESTINATIONS)+1,'mod',ink=TINTS[tint] if tint else None),show))
from surface_spec import DESTINATIONS
all_children=[]
# PAGE 1 / PAGE 2 of a page with two pages of controls (0.3.49). The user, on 0.3.48, which had
# them in the tab row: horrible; small buttons UNDER the tab row, the size they had on the
# page before, and the tab row for the tabs alone. So: two small buttons at the top right of
# the page itself, in the same place on every such page (where the compressor has its two
# Q-Link layer buttons).
def page_buttons(children,color):
    for number,caption in enumerate(['PAGE 1','PAGE 2']):children.append(nav_button(caption,904+180*number,68,172,52,'UI Section',number,32,color))
version=json.loads((project/'plugin.json').read_text())['version']
for group,group_name in enumerate(GROUPS):
    members=sorted([(i,p) for i,p in enumerate(VIEWS) if p['group']==group and not p.get('hidden')],key=lambda m:(m[1].get('order',0),m[0]))
    width=min(413,(1256-8*(len(members)-1))//len(members))   # a group with one page keeps a tab of ordinary width
    for slot,(i,p) in enumerate(members):
        e=nav_button(p['tab'],12+slot*(width+8),4,width,56,'UI Page',i,len(VIEWS),p['color'])
        all_children.append(conditional(e,[f'IndexedEnabling/{group}/7/'+handle('UI Group')]))
for i,view in enumerate(VIEWS):
    title=view['title'];chart=view['chart'];color=view['color'];sections=view['sections'];children=[]
    pages.append(dict(family=view['group'],sub=view['member'],title=title,items=[x for sec in sections for x in sec['items']],chart=chart,color=color,subtitle='',sections=sections))
    # ENV/LFO instances share their actual proxy-backed graph/control view.
    # Do not instantiate eight copies of 384 graph segments on a small MPC.
    if view['group'] in [3,4] and view['member']>1:continue
    if chart!='library':
        if view['group']==3:pass   # no name line on an envelope page (user, 0.3.33): the tab row names it
        elif view['group']==4:pass   # nor on an LFO page (0.3.35)
        elif chart not in ['home','filters','sample','effects','fx','other','advanced','oscmix','tables'] and not isinstance(chart,int):children.append(label({'pickgroup':'SOURCE AND TARGET','picktarget':'CHOOSE TARGET'}.get(chart,title.upper()),(24,72,330 if chart=='mxtable' else 440,38),TEXT,30))
        # The version stands once, at the bottom of HOME (user, 0.3.29: leave it out of the pages).
        if len(sections)>1 and chart not in ['picktarget','filters','lfo','fx','other'] and not isinstance(chart,int):   # an oscillator page draws its own two layer buttons
            # Up to four sections keep the wide buttons; more are packed right-aligned.
            for section,part in enumerate(sections):
                if len(sections)<=4:children.append(nav_button(part['title'],720+section*132,76,124,52,'UI Section',section,32,color))
                else:children.append(nav_button(part['title'],1248-(len(sections)-section)*110+6,76,104,52,'UI Section',section,32,color))
        # Large graphs (user request): oscillator 560x315 (16:9), envelope and
        # LFO/curve 240 px high with a single row of four controls below.
        # 0.3.7 compromise after "too small" (0.3.5) and "way too big" (0.3.6).
        if isinstance(chart,int):osc_page(children,view,chart);page_buttons(children,'osc')
        elif chart=='lfo':lfo_page(children,view);page_buttons(children,'mod')
        elif chart=='effects':fx_overview(children,view)
        elif chart=='fx':fx_page(children,view)
        elif chart=='other':other_page(children,view)
        elif chart=='advanced':advanced_page(children,view,color)
        elif chart=='mxtable':matrix_table(children)
        elif chart=='oscmix':osc_overview(children,view)
        elif chart=='home':home_page(children,view,version)
        elif chart=='filters':
            filters_page(children,view)
            page_buttons(children,'flt')
        elif chart=='sample':sample_page(children,view)
        if chart=='savesound':save_page(children)
        if chart=='tables':tables_page(children)
        if chart=='pickgroup':pick_groups(children)
        elif chart=='picktarget':pick_targets(children)
        elif chart=='env':env_page(children,view)
        for section,part in enumerate(sections):
            if chart in ['mxtable','pickgroup','picktarget','savesound','tables','oscmix','home','filters','retired','sample','env','lfo','effects','fx','other','advanced'] or isinstance(chart,int):break
            cells=[]
            for slot,name in enumerate(part['items']):
                row,col=divmod(slot,4);x=24+312*col;y=(228+168*row if chart not in [None,'home'] else 166+190*row)
                if isinstance(chart,int) and slot:x,y=[(648,140),(960,140),(24,408),(336,408),(648,408),(960,408)][slot-1]
                if chart in ['env','lfo','curve']:assert row==0,(title,part['title']);y=364
                caption=pretty(name)
                if view['group']==5:
                    for prefix in ['Compressor ','Chorus ','Delay ','Warp ','Eq ','Filter Fx ','Flanger ','Phaser ','Reverb ']:
                        if caption.startswith(prefix):caption=caption[len(prefix):]
                for prefix in ['Osc '+str(view['member'])+' ','Filter 1 ','Filter 2 ','Env ','Lfo ','Random ']:
                    if caption.startswith(prefix):caption=caption[len(prefix):]
                caption=caption.replace('Spectral Amount','Spectral Amt').replace('Warp Amount','Warp Amount').replace('Unison Detune','Detune').replace('Smooth Interpolation','Smooth Frames').replace('Keytrack','Key Track').replace('Transpose Quantize','Pitch Quantize').replace('Lower Threshold','Lower Thresh.').replace('Upper Threshold','Upper Thresh.').replace('Dry Wet','Mix').replace('Home ','').replace('Macro Control','Macro')
                if isinstance(chart,int) and slot==0:
                    cell=g.entry('Frame','wave-frame',{'version':1,'handleName':'Data'},'648 304 296 88','Yes','Hide','Always',slot_data('Frame',name,slot),invalidating=[f'IndexedEnabling/{section}/32/'+handle('UI Section')])
                elif chart=='effects' and section==0:
                    row,col=divmod(slot,3);caption=name[:-3].replace('_',' ').title()
                    cell=g.entry(caption,'effect-toggle',{'version':1,'handleName':'Data'},f'{24+col*416} {152+row*132} 392 112','Yes','Hide','Always',[('Data','WithName['+caption+']/'+handle(name))],invalidating=[f'IndexedEnabling/{section}/32/'+handle('UI Section')])
                else:
                    control_color=color
                    if chart=='home':
                        if name in ['Voices','Home Attack','Home Release']:control_color='env'
                        elif name.startswith('filter_') or name in ['Home Cutoff','Home Resonance','Home Filter']:control_color='flt'
                        elif name.startswith('macro_'):control_color='mod'
                    cell=control(name,caption,x,y,control_color,visible=[f'IndexedEnabling/{section}/32/'+handle('UI Section')],slot=slot)
                dict(dict(cell)['handle remapping'])['map'].append(g.obj(('key','QTarget'),('value',handle(f'UI Control {slot+1}'))))
                cells.append(cell)
            # Frames first: they lie under the controls and take no touch.
            children.extend(qlink_frames(cells,section,color));children.extend(cells)
        # No second LIBRARY button on HOME (user request, 0.3.27): the LIBRARY tab is in the top row of this page.
    else:add_library(children)
    if title=='Advanced' and __import__('os').environ.get('VITALEON_DIAG'):  # device diagnostic only
        children.append(label('DIAGNOSTIC: a test chord plays on every page except HOME',(24,352,1000,30),COLORS['mod'],20))
        children.append(label('blk mean/max us   gap max us between callbacks   late callbacks   wait max us for fx core   n host calls/s   call mean/max us   t seconds',(24,386,1230,26),MUTED,17))
        for part in range(4):children.append(label('Diag status',(24+part*308,424,304,40),TEXT,22,value=f'display_load_text_{part}'))
    for e in children:
        b=dict(e)['bounds'];x,y,w,h=map(int,dict(b)['bounds'].split())
        b[:]=[(k,f'{x} {y-64} {w} {h}' if k=='bounds' else v) for k,v in b]
    key=f'view-{i}'
    definitions.append(g.obj(('key',key),('value',g.component_definition([],children,BG))))
    condition=f'IndexedEnabling/{view["group"]}/7/'+handle('UI Group') if view['group'] in [3,4] else f'IndexedEnabling/{i}/{len(VIEWS)}/'+handle('UI Page')
    # A hidden-on-invalid container needs a valid model of its own. Child
    # bindings alone do not make it valid (0.3.0 showed only navigation on MPC).
    all_children.append(g.entry(title,key,{'version':1,'handleName':'Data'},'0 64 1280 500','No','Hide','Always',[('Data',handle('UI Page'))],invalidating=[condition]))
for group,name in enumerate(GROUPS):all_children.append(nav_button(name,12+180*group,568,176,56,'UI Group',group,7,['env','osc','flt','env','mod','fx','mod'][group]))

# Touching a visible control selects the bank containing its live Q-Link proxy.
for definition in definitions:
    value=dict(dict(definition)['value'])
    for action in value['actions']:
        a=dict(action)
        if a.get('handler')=='Q-Link':action[:]=[(k,'QTarget' if k=='handleName' else v) for k,v in action]
tabs=[{'version':1,'tabName':'VITALEON','fnKeyIndex':0,'fnKeySubIndex':0,'componentDefinition':g.component_definition([],all_children,BG),'qlinkBoundsData':[]}]
q={}
for slot in range(16):row,col=divmod(slot,4);q[f'Q-Link {13+col-row*4}']=index(f'UI Control {slot+1}')
maps=[{'Tab':1,'SubTab':1,'Bank Direction':'RowTopDown','Q-Links':q}]
replacements=[{'key':'Parameter '+str(p['index']),'value':p['choices']} for p in params if p['name']!='Full Engine' and len(p.get('choices',[]))>1]
# ---- 0.3.28 design pass --------------------------------------------------------
# Cards: every control sits on a quiet panel of its own size, drawn as the background of
# its component (one small image per size, shared by all controls of that size).
card_sizes={'wave-frame':(296,88),'macro-knob-mod':(296,112),'mini-toggle-flt':(150,128),'effect-toggle':(392,112)}
for color in COLORS:
    for role in ['knob','bipolar','fader','toggle','menu','file']:card_sizes[role+'-'+color]=(296,156)
    for role in ['knob','bipolar']:card_sizes['mini-'+role+'-'+color]=(96,128)
for role in ['knob','bipolar']:
    card_sizes['osc-big-'+role]=(172,164);card_sizes['osc-wide-'+role]=(172,124);card_sizes['osc-low-'+role]=(128,130);card_sizes['osc-under-'+role]=(172,130)
card_sizes['osc-big-switch']=(172,164);card_sizes['osc-under-switch']=(172,130)
for role in ['knob','bipolar']:
    for suffix in ['','-touch']:card_sizes['fx-big-'+role+suffix]=(172,164);card_sizes['fx-mid-'+role+suffix]=(86,124)
    card_sizes['fx-list-'+role]=(86,124);card_sizes['flt-row-'+role]=(222,100);card_sizes['flt-rowlist-'+role]=(222,100)
card_sizes['flt-row-switch']=(222,100)
card_sizes['smp-switch']=(172,124)
for role in ['knob','bipolar']:
    card_sizes['lfo-big-'+role]=(172,164);card_sizes['lfo-mid-'+role]=(86,124)
    card_sizes['lfo-biglist-'+role]=(172,164);card_sizes['lfo-list-'+role]=(86,124)
    card_sizes['lfo-cell-'+role]=(156,164);card_sizes['lfo-celllist-'+role]=(156,164);card_sizes['lfo-file-'+role]=(312,164)
card_sizes['lfo-switch']=(86,124);card_sizes['lfo-cell-switch']=(156,164);card_sizes['mod-big-switch']=(172,164)
card_sizes['fx-switch']=(86,124)
for role in ['knob','bipolar']:card_sizes['fx-biglist-'+role]=(172,164);card_sizes['fx-wide-'+role]=(172,124)
for role in ['knob','bipolar']:card_sizes['env-big-'+role]=(172,164);card_sizes['env-mid-'+role]=(86,124)
for role in ['knob','bipolar']:card_sizes['smp-wide-'+role]=(172,124)
def card_image(w,h):
    name=f'card-{w}x{h}.png'
    if not (skin/name).exists() or True:
        image=Image.new('RGBA',(w,h),(0,0,0,0));ImageDraw.Draw(image).rounded_rectangle((2,2,w-3,h-3),radius=10,fill=(30,33,46,255),outline=(44,48,64,255),width=1)
        image.save(skin/name)
    return name
carded=0
for definition in definitions:
    pairs=dict(definition);key=pairs['key']
    size=card_sizes.get(key) or ((296,156) if key.startswith('segments-') else None)
    if not size:continue
    for k,v in pairs['value']:
        if k=='backgroundData':
            for side,data in v:
                if side in ('focussed','unfocussed'):data[:]=[(kk,card_image(*size) if kk=='image' else vv) for kk,vv in data]
            carded+=1
assert carded>=20,carded
# The tile the MPC shows for the plugin under Sounds > INSTRUMENTS (270 x 110).
tile_dir=skin/'browser_images';tile_dir.mkdir(exist_ok=True)
tile=Image.new('RGB',(270,110),'#181a23');td=ImageDraw.Draw(tile)
points=[(x,64-math.sin(x/270*math.pi*2*1.5)*(4+11*x/270)*math.cos(x/270*math.pi*5)) for x in range(0,271,2)]   # between the name and the maker's line
td.line(points,fill='#3a3350',width=6);td.line(points,fill='#bca3ff',width=2)
td.rectangle((0,0,269,109),outline='#353949',width=1)
big=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),30);small=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),14)
td.text((14,12),'VITALEON',font=big,fill='#edf0f7');td.text((15,84),'Andersson Audio',font=small,fill='#a0a7b7')
tile.save(tile_dir/'soundsmode.png')
tui={'pageData':{'version':1,'componentDefinitions':{'version':2,'importFiles':[],'localComponentDefinitions':definitions},'replacementStrings':replacements,'info':{'version':1,'type':'CompleteDescription'},'tabs':tabs}}
qlinks={'version':4,'info':{'version':1,'type':'CompleteDescription'},'Screen Mode Q-Links':{'version':4,'map':maps},'Program Mode Q-Links':{f'Q-Link {i+1}':index(n) for i,n in enumerate(['macro_control_1','macro_control_2','macro_control_3','macro_control_4','filter_1_cutoff','filter_1_resonance','filter_2_cutoff','filter_2_resonance','Env attack','Env decay','Env sustain','Env release','volume','Voices','osc_1_wave_frame','osc_1_unison_voices'])}}
for state in ['off','on']:
    img=Image.new('RGBA',(176,56),'#373045' if state=='on' else '#222531');d=ImageDraw.Draw(img)
    d.rectangle((0,0,175,55),outline='#bca3ff' if state=='on' else '#454c61',width=2 if state=='on' else 1)
    img.save(skin/f'panel-{state}.png')
# Original native-size touch backgrounds; labels remain dynamic native text.
for kind,size in [('bank',(116,52)),('row',(440,52)),('select',(96,52)),('page',(104,60)),('load',(216,60)),('mode',(216,56)),('filter',(200,52))]:
    for state in ['off','on']:
        selected=kind=='load' or (kind in ['row','select','mode','filter'] and state=='on')
        img=Image.new('RGBA',size,'#373045' if selected else '#222531');d=ImageDraw.Draw(img)
        d.rectangle((0,0,size[0]-1,size[1]-1),outline='#bca3ff' if selected else '#454c61',width=1)
        img.save(skin/f'home-{kind}-{state}.png')
for color,accent in COLORS.items():
    strip=Image.new('RGBA',(56,112*127))
    for i in range(127):
        image=Image.new('RGBA',(112,224));d=ImageDraw.Draw(image);y=200-round(i*176/126)
        d.rounded_rectangle((46,16,66,208),radius=10,fill='#454c61')
        d.rounded_rectangle((46,min(y,188),66,208),radius=10,fill='#'+accent[2:])
        d.rounded_rectangle((16,y-12,96,y+12),radius=5,fill='#edf0f7')
        strip.paste(image.resize((56,112),Image.Resampling.LANCZOS),(0,i*112))
    strip.save(skin/f'home-fader-{color}.png')
for kind,size in [('arrow',(64,60)),('page',(160,60)),('row',(744,60)),('select',(112,60)),('load',(424,64))]:
    for state in ['off','on']:
        fill='#373045' if kind=='load' or (kind in ['row','select'] and state=='on') else '#222531'
        border='#bca3ff' if kind=='load' or (kind in ['row','select'] and state=='on') else '#353949'
        img=Image.new('RGBA',size,fill);d=ImageDraw.Draw(img);d.rectangle((0,0,size[0]-1,size[1]-1),outline=border,width=1)
        img.save(skin/f'library-{kind}-{state}.png')
# Film strips: actual UI assets, not a simulated shader or web-only rendering.
for color,argb in COLORS.items():
    rgb=tuple(bytes.fromhex(argb[2:]))
    for role in ['knob','bipolar']:
        strip=Image.new('RGBA',(112,112*127));small=Image.new('RGBA',(80,80*127));tiny=Image.new('RGBA',(56,56*127))
        for i in range(127):
            image=Image.new('RGBA',(448,448));d=ImageDraw.Draw(image)
            d.ellipse((54,58,394,398),fill='#262a38',outline='#454c61',width=4)
            d.arc((25,25,423,423),135,405,fill='#454c61',width=14)
            angle=135+270*i/126
            if role=='bipolar':
                if i!=63:d.arc((25,25,423,423),min(angle,270),max(angle,270),fill=rgb,width=14)
            else:d.arc((25,25,423,423),135,angle,fill=rgb,width=14)
            theta=math.radians(angle);d.line((224+math.cos(theta)*104,224+math.sin(theta)*104,224+math.cos(theta)*151,224+math.sin(theta)*151),fill='#edf0f7',width=12)
            strip.paste(image.resize((112,112),Image.Resampling.LANCZOS),(0,i*112))
            small.paste(image.resize((80,80),Image.Resampling.LANCZOS),(0,i*80))
            if color in ['osc','fx']:tiny.paste(image.resize((56,56),Image.Resampling.LANCZOS),(0,i*56))
        strip.save(skin/f'large-{color}-{role}.png')
        small.save(skin/f'small-{color}-{role}.png')   # the small dials of HOME and of the OSC overview
        if color in ['osc','fx']:tiny.save(skin/f'tiny-{color}-{role}.png')   # the smallest dials of the oscillator pages and of the filter page
    strip=Image.new('RGBA',(280,56*127))
    for i in range(127):
        image=Image.new('RGBA',(560,112));d=ImageDraw.Draw(image);x=24+round(i*512/126)
        d.rounded_rectangle((16,46,544,66),radius=10,fill='#454c61')
        d.rounded_rectangle((16,46,max(36,x),66),radius=10,fill=rgb)
        d.rounded_rectangle((x-18,16,x+18,96),radius=6,fill='#edf0f7')
        strip.paste(image.resize((280,56),Image.Resampling.LANCZOS),(0,i*56))
    strip.save(skin/f'large-{color}-fader.png')
    for state in ['off','on']:
        img=Image.new('RGBA',(136,72));d=ImageDraw.Draw(img)
        d.rounded_rectangle((2,2,133,69),radius=34,fill=rgb if state=='on' else '#454c61')
        x=100 if state=='on' else 36;d.ellipse((x-25,11,x+25,61),fill='#181a23' if state=='on' else '#edf0f7')
        img.save(skin/f'large-{color}-{state}.png')
        img=Image.new('RGBA',(144,52),('#373045' if state=='on' else '#222531'));d=ImageDraw.Draw(img)
        d.rectangle((0,0,143,51),outline=rgb if state=='on' else '#454c61',width=2 if state=='on' else 1)
        img.save(skin/f'segment-choice-{color}-{state}.png')
    for bank in range(4):
        strip=Image.new('RGBA',(8,64*256))
        for a in range(bank*8,bank*8+8):
            for b in range(32):
                image=Image.new('RGBA',(64,512));d=ImageDraw.Draw(image);d.line((0,(1-a/31)*511,63,(1-b/31)*511),fill=rgb,width=8)
                strip.paste(image.resize((8,64),Image.Resampling.LANCZOS),(0,((a%8)*32+b)*64))
        strip.save(skin/f'segment-{color}-{bank}.png')
(root/'surface-pages.json').write_text(json.dumps([{k:p[k] for k in ['family','sub','title','items','chart','color','subtitle','sections']} for p in pages],indent=2))
strip=Image.new('RGBA',(1216,4*512))
for frame in range(512):
    d=ImageDraw.Draw(strip);x=round(frame*1213/511);d.rectangle((x,frame*4,x+2,frame*4+3),fill='#edf0f7')
strip.save(skin/'lfo-playhead.png')

# Hardware evidence (upstream PR #5): even a bound, non-focusable Label consumes
# taps over a Button. Render fixed captions INTO each button image, then remove
# the overlay widgets. The custom preset browser is retired; use native VST programs.
# No touch routing assumption or transparent-label workaround is required.
caption_assets={}
def bake_button_captions(entries):
    remove=set()
    for entry in entries:
        model=dict(dict(entry)['componentData'])
        if model['type']!='Button':continue
        data=model['data'];bx,by,bw,bh=map(int,dict(dict(entry)['bounds'])['bounds'].split())
        captions=[]
        for candidate in entries:
            m=dict(dict(candidate)['componentData'])
            if m['type']!='Label':continue
            x,y,w,h=map(int,dict(dict(candidate)['bounds'])['bounds'].split())
            if min(x+w,bx+bw)<=max(x,bx) or min(y+h,by+bh)<=max(y,by):continue
            assert x>=bx and y>=by and x+w<=bx+bw and y+h<=by+bh,('Partial label overlap',model['name'],m['name'])
            d=dict(m['data']);style=json.loads(g.dump_json(d['textStyle']))
            bound=d['handleName'];remap={dict(v)['key']:dict(v)['value'] for v in dict(dict(candidate)['handle remapping'])['map']};bound=remap.get(bound,bound)
            constant=re.search(r'WithName\[([^]]*)\]',bound)
            if d['type']=='Name' and constant:texts=[constant[1],constant[1]]
            elif d['type']=='Value' and bound==dict(data)['handleName']:texts=['Off','On']
            else:raise ValueError('Dynamic label must sit beside button: '+m['name'])
            captions.append({'text':texts,'bounds':[x-bx,y-by,w,h],'style':style});remove.add(id(candidate))
        if not captions:continue
        current=dict(data);signature=json.dumps({'on':current['onImage'],'off':current['offImage'],'captions':captions},sort_keys=True)
        key=hashlib.sha256(signature.encode()).hexdigest()[:16]
        for state,field in enumerate(['offImage','onImage']):
            image=Image.open(skin/current[field]).convert('RGBA');draw=ImageDraw.Draw(image)
            for caption in captions:
                x,y,w,h=caption['bounds'];style=caption['style'];text=caption['text'][state]
                size=int(style['font']['height']);font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
                while draw.textlength(text,font=font)>w-4 and size>14:
                    size-=1;font=ImageFont.truetype(str(root/'fonts/Roboto-Regular.ttf'),size)
                assert draw.textlength(text,font=font)<=w,(model['name'],text,w)
                tx=x+(w-draw.textlength(text,font=font))/2 if 'horizontallyCentred' in style['justification'] else x
                bounds=draw.textbbox((0,0),text,font=font);ty=y+(h-(bounds[3]-bounds[1]))/2-bounds[1]
                draw.text((tx,ty),text,font=font,fill='#'+style['colour'][2:])
            filename=f'caption-{key}-{state}.png';image.save(skin/filename);current[field]=filename
            caption_assets[filename]={'size':list(image.size),'texts':[c['text'][state] for c in captions]}
        if isinstance(data,dict):data.update(current)
        else:data[:]=list(current.items())
    entries[:]=[entry for entry in entries if id(entry) not in remove]

# The library row keeps its two texts as live labels on its own button: the row component takes the tap (0.3.28).
for definition in definitions:
    # The rows of LIBRARY and of the wavetable pages keep their value label on the row button: the
    # tap reaches the component's own action (seen on the device for LIBRARY, 0.3.28).
    if dict(definition)['key'] not in ('library-row','table-row'):bake_button_captions(dict(dict(definition)['value'])['componentsData'])
for tab in tabs:bake_button_captions(dict(tab['componentDefinition'])['componentsData'])
# Resolve every direct native widget through an explicit scoped alias. This is
# also used for Local panel buttons and fixed captions; modifiers live in the
# remap value, never in an unregistered widget-local name.
def bind_native_handles(entries):
    for entry in entries:
        model=dict(dict(entry)['componentData']);data=model['data'];fields=dict(data)
        mapping=dict(dict(entry)['handle remapping'])['map'];known={dict(x)['key'] for x in mapping}
        for field,value in list(fields.items()):
            if not isinstance(value,str) or not (field=='handleName' or field.endswith('Handle') or field=='dataHandle'):continue
            if 'Parameter ' not in value and not value.startswith('Local/'):continue
            alias='Bound'+field
            assert alias not in known
            mapping.append(g.obj(('key',alias),('value',value)));known.add(alias);fields[field]=alias
        # Never rebuild the widget from a dict: KnobOverlay/MenuOverlay carry two
        # "version" keys on purpose (ui-reference 6.10/6.11). Collapsing them made
        # the menu overlay open invisibly on the MPC (0.3.2). Replace values in place.
        if isinstance(data,dict):data.update(fields)
        else:data[:]=[(key,fields[key] if key!='version' else value) for key,value in data]
for definition in definitions:bind_native_handles(dict(dict(definition)['value'])['componentsData'])
for tab in tabs:bind_native_handles(dict(tab['componentDefinition'])['componentsData'])
(root/'button-captions.json').write_text(json.dumps(caption_assets,indent=2),encoding='utf-8')
# 0.3.47: no indentation. The same JSON, line for line, without the leading spaces that made
# two thirds of the file (11.9 MB -> 4.1 MB for the MPC to read when the screen opens). (No JSON string holds a raw line break, so
# stripping the start of each line cannot touch a text.)
(skin/'TUI.json').write_text('\n'.join(line.lstrip(' ') for line in g.dump_json(tui).split('\n'))+'\n',encoding='utf-8');(skin/'Q-Links.json').write_text(g.dump_json(qlinks)+'\n',encoding='utf-8')
print('Generated',len(tabs),'native pages;',len(params),'parameters;',len(replacements),'choice labels')

