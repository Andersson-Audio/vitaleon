"""Structural and binding checks; the native MPC renderer is not emulated."""
from surface_states import *
from PIL import Image
skin=PROJECT/'skins'
assert len(UI['tabs'])==1
assert len(VIEWS)==43 and [sum(v['group']==g for v in VIEWS) for g in range(7)]==[6,6,2,6,8,10,5]
assert VIEWS[41]['chart']=='savesound' and VIEWS[42]['chart']=='tables' and VIEWS[42]['hidden'] and VIEWS[42]['group']==1,'the page of factory tables is appended as a hidden page of OSC'
assert VIEWS[40]['chart']=='oscmix' and VIEWS[4]['tab']=='OSC 1','the overview is appended; earlier pages keep their numbers'
assert all([len(s['items']) for s in VIEWS[n]['sections']]==[16,11] for n in (4,5,6)),'an oscillator page shows its controls in two pages: sixteen and eleven'
assert VIEWS[37]['chart']=='other' and VIEWS[2]['chart']=='advanced' and len(VIEWS[37]['sections'])==1 and len(VIEWS[2]['sections'])==1 and VIEWS[37]['group']==0 and [VIEWS[n].get('redirect') for n in (9,35,36)]==[8,37,37] and VIEWS[34]['chart']=='mxtable','MOD holds the matrix; HOME > OTHER random and expression, HOME > ADVANCED the route curve, one layer each; retired pages open their successor'
assert VIEWS[7]['chart']=='sample' and len(VIEWS[7]['sections'])==1,'the sample has one page without sections'
assert VIEWS[8]['chart']=='filters' and VIEWS[9]['chart']=='retired' and VIEWS[9]['hidden'] and VIEWS[10]['tab']=='ENV 1','both filters share page 8; page 9 keeps its place so later pages keep their numbers'
assert all(len(v['sections'])<=32 and all(len(s['items'])<=16 for s in v['sections']) for v in VIEWS)
replacement={x['key']:x['value'] for x in UI['replacementStrings']}
assert len(replacement['Parameter '+str(NAMES['Target'])])==429
assert len(replacement['Parameter '+str(NAMES['Parameter'])])==770
assert 'Parameter '+str(NAMES['Value']) not in replacement
assert replacement['Parameter '+str(NAMES['voice override'])]==['Kill','Steal']
# A Q-Link slot carries a different control on every page: it must never have a label list
# (0.3.27 to 0.3.29: slot 7 was an Off/On switch everywhere because HOME has a switch there).
for slot in range(1,17):assert 'Parameter '+str(NAMES[f'UI Control {slot}']) not in replacement,f'UI Control {slot} has a label list'
mapping=json.loads((skin/'Q-Links.json').read_text())['Screen Mode Q-Links']['map']
assert len(mapping)==1 and mapping[0]['Bank Direction']=='RowTopDown'
for slot in range(16):
    row,col=divmod(slot,4);assert mapping[0]['Q-Links'][f'Q-Link {13+col-row*4}']==NAMES[f'UI Control {slot+1}']
image_cache={}
def image_size(name):
    if name not in image_cache:image_cache[name]=Image.open(skin/name).size
    return image_cache[name]
def parameter(h,mapping):
    h=resolve(h,mapping);m=re.search(r'Parameter (\d+)',h)
    assert m,('Unresolved native handle',h)
    n=int(m[1]);assert 0<=n<len(PARAMS) and n!=NAMES['Full Engine'];return n
checked=0
for page,section,values in states():
    leaves=expand(UI['tabs'][0]['componentDefinition']['componentsData'],values);checked+=1
    for e in leaves:
        d=e['data'];kind=e['kind'];x,y,r,b=e['box'];w,h=r-x,b-y
        if kind=='Label':assert d['type']!='Text'
        for field,value in d.items():
            if isinstance(value,str) and (field=='handleName' or field.endswith('Handle') or field=='dataHandle'):
                p=parameter(value,e['mapping'])
                if kind=='Envelope':assert 'Parameter '+str(p) not in replacement
        if kind=='Button':
            assert w>=64 and h>=52,(e['name'],w,h)
            for name in ['onImage','offImage']:assert image_size(d[name])==(w,h)
            p=parameter(d['handleName'],e['mapping'])
            if p==NAMES['UI Page']:assert d['numButtonsInGroup']==43 and 0<=d['buttonId']<43
            if p==NAMES['UI Group']:assert d['numButtonsInGroup']==7
        if kind=='Knob':
            assert image_size(d['filmStrip'])==(w,h*d['numFrames'])
            assert (w,h) in [(112,112),(280,56),(56,112),(120,52),(150,52),(80,80),(56,56)],(e['name'],w,h)
    if VIEWS[page]['group']==1 and isinstance(VIEWS[page]['chart'],int):
        segments=[e for e in leaves if e['name'].startswith('wave column')]
        left=min(e['box'][0] for e in segments);right=max(e['box'][2] for e in segments)
        top=min(e['box'][1] for e in segments);bottom=max(e['box'][3] for e in segments)
        assert (right-left)*9==(bottom-top)*16 and right-left==512 and len(segments)==256 and (left,top)==(32,128),'Oscillator graph must be exactly 16:9, in 256 columns, in the same place on both pages'
        # 0.3.41: PAGE 1 shows the sixteen main controls, PAGE 2 the eleven others, and nothing else.
        controls=len([e for e in leaves if e['kind']=='Knob'])+len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])
        assert controls==[16,11][section],(page,section,controls,'controls of a page of an oscillator')
        assert sorted(e['name'] for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568 and e['name']!='switch')==['OFF','PAGE 1','PAGE 2','TABLE LIST'],(page,section,'on/off, the list of tables and the two page buttons')
        tabs=[e['name'] for e in sorted([e for e in leaves if e['kind']=='Button' and e['box'][1]<64],key=lambda e:e['box'][0])]
        assert tabs==['OVERVIEW','OSC 1','OSC 2','OSC 3','SAMPLE'],(page,tabs,'the tab row of OSC holds its five tabs and nothing else')
        dials=[e for e in leaves if e['kind']=='Knob' and e['box'][0]>=568 and 64<=e['box'][1]<568]
        assert min(e['box'][1] for e in dials)>=124,(page,section,'the dials beside the picture start no higher than the picture')
    if VIEWS[page]['chart']=='savesound':
        # 0.3.44: the name entry: 40 keys with their own codes, CAPS, SAVE, CANCEL, the name and its status.
        body=[e for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568]
        keys=[e for e in body if parameter(e['data']['handleName'],e['mapping'])==NAMES['UI Key']]
        assert len(keys)==40 and sorted(e['data']['buttonId'] for e in keys)==list(range(1,41)) and all(e['data']['numButtonsInGroup']==41 for e in keys),(page,len(keys),'every key once')
        assert sorted(e['name'] for e in body if e not in keys)==['CANCEL','CAPS','SAVE'],(page,'CAPS, SAVE and CANCEL')
        assert len([e for e in leaves if e['kind']=='Label' and e['name']=='Name'])==1 and len([e for e in leaves if e['kind']=='Label' and e['name']=='Save status'])==3,(page,'the name and its status')
    if VIEWS[page]['chart']=='library':
        # 0.3.45: the folder of the saved sounds and their export, at the right of the status.
        steps=sorted((e['name'],e['data']['buttonId']) for e in leaves if e['kind']=='Button' and parameter(e['data']['handleName'],e['mapping'])==NAMES['Library Step'])
        assert steps==[('EXPORT',2),('MY SOUNDS',1)],(page,steps,'MY SOUNDS and EXPORT in the library')
        assert len([e for e in leaves if e['kind']=='Label' and e['name']=='Browser status'])==4,(page,'the status has four parts')
    if VIEWS[page]['chart']=='oscmix':
        # 0.3.52: two arrows per oscillator step through the tables, the name stands between
        # them, and the whole picture is a button that opens the page of factory tables.
        bound=lambda e,name:parameter(e['data']['handleName'],e['mapping'])==NAMES[name]
        buttons=[e for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568]
        arrows=sorted((e['data']['buttonId'],e['name'],e['box'][0]) for e in buttons if bound(e,'Table Step'))
        assert [(a[0],a[1]) for a in arrows]==[(1,'<'),(2,'>'),(3,'<'),(4,'>'),(5,'<'),(6,'>')] and all(e['data']['numButtonsInGroup']==7 for e in buttons if bound(e,'Table Step')),(page,arrows)
        taps=sorted([e for e in buttons if bound(e,'UI Tables')],key=lambda e:e['data']['buttonId'])
        assert [e['data']['buttonId'] for e in taps]==[1,2,3],(page,'one tap area per oscillator')
        names=sorted([e for e in leaves if e['kind']=='Label' and e['name']=='Table name'],key=lambda e:e['box'][0])
        assert len(names)==3,(page,'the name of each table')
        for col,(tap,name) in enumerate(zip(taps,names)):
            px,py=44+312*col,136;box=tap['box'];left,right=[a[2] for a in arrows if (a[0]-1)//2==col]
            assert box[0]<=px and box[1]<=py and box[2]>=px+256 and box[3]>=py+176,(page,col,box,'the tap area covers the whole picture')
            assert left+64<=name['box'][0] and name['box'][2]<=right and name['box'][1]>=box[3],(page,col,'the name stands between its arrows, under the picture')
    if VIEWS[page]['chart']=='tables':
        # 0.3.52: the 32 factory tables as a table, four columns of eight rows, a row is a button.
        from surface_spec import FACTORY_TABLES
        bound=lambda e,name:parameter(e['data']['handleName'],e['mapping'])==NAMES[name]
        buttons=[e for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568]
        rows=sorted([e for e in buttons if bound(e,'Table Pick')],key=lambda e:e['data']['buttonId'])
        assert len(FACTORY_TABLES)==32 and [e['data']['buttonId'] for e in rows]==list(range(1,33)) and all(e['data']['numButtonsInGroup']==33 for e in rows),(page,len(rows))
        assert [e['name'] for e in rows]==[f'{n+1:02d} {name}' for n,name in enumerate(FACTORY_TABLES)],(page,'rows in the order of the numbers')
        xs=sorted({e['box'][0] for e in rows});ys=sorted({e['box'][1] for e in rows})
        assert len(xs)==4 and len(ys)==8 and max(e['box'][3] for e in rows)<=564,(page,xs,ys,'four columns of eight rows on one screen')
        assert all(rows[n]['box'][0]==xs[n//8] and rows[n]['box'][1]==ys[n%8] for n in range(32)),(page,'the numbers run down each column')
        for e in rows:
            for image in (e['data']['onImage'],e['data']['offImage']):assert Image.open(skin/image).size==(e['box'][2]-e['box'][0],e['box'][3]-e['box'][1]),(page,image)
        assert sorted(e['data']['buttonId'] for e in buttons if bound(e,'Table Target'))==[1,2,3],(page,'the oscillator the page works on')
        back=[e for e in buttons if e['name']=='BACK']
        assert len(back)==1 and bound(back[0],'UI Page') and VIEWS[back[0]['data']['buttonId']]['chart']=='oscmix',(page,'BACK goes to the overview')
        assert sorted((e['name'],e['data']['buttonId']) for e in buttons if bound(e,'Table Page Step'))==[('<',1),('>',2)],(page,'the two arrows of the pages')
        assert len(buttons)==38,(page,len(buttons),'32 tables, three oscillators, two page arrows, BACK')
        # 0.3.53: a page of other tables. 32 rows in the same places as the factory rows, each a
        # row button with its name and a picture of 12 parts; the factory rows are gone then.
        other=dict(values);other[NAMES['Table Page']]=1/31;other.update({NAMES[f'Table Row On {r+1}']:1.0 for r in range(32)})
        shown=[e for e in expand(UI['tabs'][0]['componentDefinition']['componentsData'],other) if 64<=e['box'][1]<568]
        user=[e for e in shown if e['kind']=='Button' and e['name']=='row']
        assert sorted((e['box'][0],e['box'][1]) for e in user)==sorted((e['box'][0],e['box'][1]) for e in rows) and not [e for e in shown if e['kind']=='Button' and bound(e,'Table Pick')],(page,len(user),'the user rows take the places of the factory rows')
        assert len({parameter(e['data']['handleName'],e['mapping']) for e in user})==32 and all(NAMES[f'Table Row {r+1}'] in {parameter(e['data']['handleName'],e['mapping']) for e in user} for r in range(32)),(page,'each row has its own tap')
        assert len([e for e in shown if e['kind']=='Label' and e['name']=='Table row name'])==32,(page,'each row has its name')
        pictures=[e for e in shown if e['kind']=='Image' and e['name'].startswith('table picture')]
        assert len(pictures)==32*24 and all(e['data']['numFrames']==122 for e in pictures) and len({parameter(e['data']['handleName'],e['mapping']) for e in pictures})==768,(page,len(pictures),'24 picture parts per row, each on its own value')
        # 0.3.55: the values are those of the three large wave pictures, no parameters of their own;
        # a part is shown with its row only.
        assert all(PARAMS[parameter(e['data']['handleName'],e['mapping'])]['name'].startswith('display_wave_') for e in pictures) and len(PARAMS)==3392,(page,len(PARAMS),'the pictures borrow the values of the wave pictures')
        assert not [e for e in leaves if e['kind']=='Image' and e['name'].startswith('table picture')],(page,'no picture part without its row')
        one=dict(values);one[NAMES['Table Page']]=1/31;one[NAMES['Table Row On 3']]=1.0
        assert len([e for e in expand(UI['tabs'][0]['componentDefinition']['componentsData'],one) if e['kind']=='Image' and e['name'].startswith('table picture')])==24,(page,'a row brings its 24 picture parts')
        assert all(e['box'][2]-e['box'][0]==4 and e['box'][3]-e['box'][1]==40 for e in pictures),(page,'a part is 4 x 40')
        strip=Image.open(skin/pictures[0]['data']['image']);assert strip.size==(4,40*122) and strip.height<=31104,(page,strip.size)
        # a part ends where the next begins: the frame 'level 5 to 7' ends at the height at which 'level 7 to 6' starts
        bright=lambda frame,x:max(range(40),key=lambda y:sum(strip.getpixel((x,40*frame+y))[:3]))
        assert abs(bright(1+5*11+7,3)-bright(1+7*11+6,0))<=3,(page,'neighbouring parts of a picture meet')
        assert strip.getpixel((0,0))[3]==0 and strip.getpixel((0,40))[3]==255,(page,'frame 0 of the picture strip is empty')
        for row in range(1,33):
            marked=dict(other);marked[NAMES['Table Row Mark']]=row/32
            assert len([e for e in expand(UI['tabs'][0]['componentDefinition']['componentsData'],marked) if e['kind']=='Image' and e['name']=='row mark'])==1,(page,row,'one row is marked')
        hint=dict(values);hint[NAMES['Table Page']]=1/31;hint[NAMES['Table Empty']]=1.0
        said=[e for e in expand(UI['tabs'][0]['componentDefinition']['componentsData'],hint) if e['kind']=='Image' and e['name']=='empty page hint']
        assert len(said)==1 and not [e for e in leaves if e['kind']=='Image' and e['name']=='empty page hint'],(page,len(said),'an empty page says how it is filled, a page with tables does not')
    if VIEWS[page]['chart']=='home':
        # 0.3.60: the bank stepper, BANK LIST, INIT SOUND and SAVE SOUND are one row at the top,
        # centred on the height of name and author; the comment stands at the bottom left under the
        # name's left edge; the author stands directly under the name.
        places={e['name']:(e['box'][0],e['box'][1],e['box'][2]-e['box'][0],e['box'][3]-e['box'][1]) for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568 and e['name']!='switch'}
        assert places=={'<':(448,79,64,52),'>':(732,79,64,52),'BANK LIST':(804,79,124,52),'INIT SOUND':(936,79,156,52),'SAVE SOUND':(1100,79,156,52)},(page,places)
        init=[e for e in leaves if e['kind']=='Button' and e['name']=='INIT SOUND'][0]
        assert parameter(init['data']['handleName'],init['mapping'])==NAMES['UI Init'] and init['data']['numButtonsInGroup']==1,(page,'INIT SOUND is a switch on its own parameter')
        name=[e for e in leaves if e['kind']=='Label' and e['name']=='Sound name'][0]['box'];author=[e for e in leaves if e['kind']=='Label' and e['name']=='Author and style'][0]['box']
        assert author[0]==name[0] and 0<=author[1]-name[3]<=4,(page,name,author,'the author stands directly under the name')
        assert all(2*y+h==name[1]+author[3] for _,y,_,h in places.values()),(page,name,author,'the row of buttons is centred on name and author')
        assert max(name[2],author[2])<=min(x for x,_,_,_ in places.values())-8,(page,'name and author end before the row begins')
        notes=sorted([e['box'] for e in leaves if e['kind']=='Label' and e['name']=='Sound note'],key=lambda b:b[1])
        assert len(notes)==3 and all(b[0]==name[0] for b in notes) and notes[0][1]>=488 and notes[2][3]<=564,(page,notes,'the comment stands at the bottom, left-aligned with the name')
        status=[e['box'] for e in leaves if e['kind']=='Label' and e['name']=='Import status']
        assert len(status)==4 and min(b[0] for b in status)>=max(b[2] for b in notes)+8,(page,'the load message stands to the right of the comment')
        # The row under the macros ends where the macros end (cells to x 1256; a small dial sits 8 inside its cell).
        picture=[e['box'] for e in leaves if e['kind']=='Envelope'];dials=[e['box'] for e in leaves if e['kind']=='Knob' and 330<=e['box'][1]<470]
        assert picture==[(486,336,848,464)] or [tuple(b) for b in picture]==[(486,336,848,464)],(page,picture)
        assert max(b[2] for b in dials)==1248,(page,max(b[2] for b in dials),'the amp dials end at the right edge of the macros')
    if VIEWS[page]['chart']=='mxtable':
        # 0.3.43: one PICK per row; source and target each once on screen, in one of six colours.
        body=[e for e in leaves if 64<=e['box'][1]<568]
        assert len([e for e in body if e['kind']=='Button' and e['name']=='PICK'])==8,(page,section,'one PICK per row')
        names=[e for e in body if e['kind']=='Label' and e['box'][0] in (76,572) and e['box'][1]>=148]
        assert len(names)==16 and len({(e['box'][0],e['box'][1]) for e in names})==16,(page,section,len(names),'one source and one target name per row')
        assert [e['name'] for e in sorted([e for e in body if e['kind']=='Label' and e['box'][1]==128 and e['box'][0]>=902],key=lambda e:e['box'][0])]==['OSC','FILTER','ENV','LFO','FX'],(page,section,'the legend of the colours')
        for tint in range(1,6):
            other=dict(values);other.update({NAMES[f'Matrix Source Tint {row}']:tint/5 for row in range(1,9)});other.update({NAMES[f'Matrix Target Tint {row}']:tint/5 for row in range(1,9)})
            shown=[e for e in expand(UI['tabs'][0]['componentDefinition']['componentsData'],other) if e['kind']=='Label' and e['box'][0] in (76,572) and 148<=e['box'][1]<568]
            assert len(shown)==16 and len({e['data']['textStyle']['colour'] if isinstance(e['data'].get('textStyle'),dict) else str(e['data']) for e in shown})==1,(page,section,tint,'every colour class shows exactly one name per cell')
    if VIEWS[page]['chart']=='pickgroup':
        # 0.3.43: the page of a route: its 32 sources above, the 27 groups of targets below.
        body=[e for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568]
        sources=[e for e in body if parameter(e['data']['handleName'],e['mapping'])==NAMES['Source']]
        groups=[e for e in body if parameter(e['data']['handleName'],e['mapping'])==NAMES['UI Section']]
        assert len(sources)==33 and sorted(e['data']['buttonId'] for e in sources)==list(range(33)) and all(e['data']['numButtonsInGroup']==33 for e in sources),(page,len(sources),'every source and NO SOURCE')
        assert len(groups)==27 and sorted(e['data']['buttonId'] for e in groups)==list(range(1,28)),(page,len(groups),'every group of targets')
        assert sorted(e['name'] for e in body if e not in sources+groups)==['BACK','NO TARGET'] and 'NO SOURCE' in [e['name'] for e in sources],(page,'BACK, NO SOURCE and NO TARGET')
    if VIEWS[page]['chart']=='other':
        # HOME > OTHER (0.3.40): the twelve random and expression controls, no picture, one layer.
        assert not [e for e in leaves if e['name'].startswith('curve column')],(page,section,'no picture on HOME > OTHER')
        assert len([e for e in leaves if e['kind']=='Knob'])+len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])==12,(page,section,'controls of HOME > OTHER')
        assert not [e for e in leaves if e['kind']=='Button' and e['name'].startswith('Q-LINKS')],(page,section,'HOME > OTHER has one Q-Link layer')
    if VIEWS[page]['chart']=='advanced':
        # HOME > ADVANCED (0.3.40): the route curve with its eight controls, and three more.
        columns=[e for e in leaves if e['name'].startswith('curve column')]
        assert len(columns)==192 and max(e['box'][2] for e in columns)-min(e['box'][0] for e in columns)==576,(page,section,'route curve picture')
        assert len([e for e in leaves if e['kind']=='Knob'])+len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])>=9,(page,section,'controls of HOME > ADVANCED')
    if VIEWS[page]['chart']=='fx':
        # An effect page shows every control of the effect in every Q-Link layer.
        wanted=sum(len([i for i in s['items'] if i]) for s in VIEWS[page]['sections'])
        shown=len([e for e in leaves if e['kind']=='Knob'])+len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])
        assert shown==wanted,(page,section,VIEWS[page]['title'],shown,wanted)
        assert len(VIEWS[page]['sections'])<=2 and (len(VIEWS[page]['sections'])==1)==(not [e for e in leaves if e['kind']=='Button' and e['name'].startswith('Q-LINKS')]),(page,'layer buttons only where there are two layers')
    if VIEWS[page]['chart']=='lfo':
        # 0.3.42: the picture stays; PAGE 1 shows its eleven controls, PAGE 2 its nine, and nothing else.
        columns=[e for e in leaves if e['name'].startswith('curve column')]
        assert len(columns)==192 and max(e['box'][2] for e in columns)-min(e['box'][0] for e in columns)==576 and max(e['box'][3] for e in columns)-min(e['box'][1] for e in columns)==288,(page,section,'LFO picture')
        knobs=len([e for e in leaves if e['kind']=='Knob']);switches=len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])
        assert (knobs,switches)==[(10,1),(8,1)][section],(page,section,knobs,switches,'controls of a page of an LFO')
        assert sorted(e['name'] for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568 and e['name']!='switch')==['PAGE 1','PAGE 2'],(page,section,'only the two page buttons')
    if VIEWS[page]['chart']=='env':
        # One page: the picture, nine dials, no name line and no section buttons.
        graph=[e for e in leaves if e['name'].startswith('envelope column')];assert len(graph)==312 and max(e['box'][2] for e in graph)-min(e['box'][0] for e in graph)==936 and not [e for e in leaves if e['kind']=='Envelope'],(page,'envelope picture')
        # DRAG shows the MPC's own envelope in the same place and hides the drawn one.
        other=dict(values);other[NAMES['UI Env View']]=1.0;drag=expand(UI['tabs'][0]['componentDefinition']['componentsData'],other)
        native=[e for e in drag if e['kind']=='Envelope'];assert len(native)==1 and native[0]['box'][2]-native[0]['box'][0]==936 and not [e for e in drag if e['name'].startswith('envelope column')],(page,'drag view')
        assert len([e for e in leaves if e['kind']=='Knob' and 64<=e['box'][1]<568])==9,(page,'nine dials')
        assert sorted(e['name'] for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568)==['DRAG','SHAPE'],(page,'only the two picture buttons on an envelope page')
    if VIEWS[page]['chart']=='sample':
        columns=[e for e in leaves if e['name'].startswith('sample column')]
        assert len(columns)==256 and max(e['box'][2] for e in columns)-min(e['box'][0] for e in columns)==512,(page,len(columns))
        assert len([e for e in leaves if e['kind']=='Knob'])==6 and len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])==4,(page,'sample controls')
        assert len([e for e in leaves if e['name']=='sample off'])==1,'with the sampler off (Init) its picture is dimmed'
        assert any(e['kind']=='Button' and e['name']=='SAMPLE LIST' for e in leaves) and any(e['kind']=='Label' and e['name']=='Sample name' for e in leaves),(page,'list button and name')
    if VIEWS[page]['chart']=='filters':
        # 0.3.46: both pictures on both pages of controls; PAGE 1 shows eight controls per filter,
        # PAGE 2 seven and the on switch at the left; the two page buttons stand in the top row.
        for f in range(2):
            columns=[e for e in leaves if e['name'].startswith('filter column') and parameter(e['data']['handleName'],e['mapping']) in range(NAMES[f'display_filter_{f}_0'],NAMES[f'display_filter_{f}_0']+144)]
            assert len(columns)==144 and max(e['box'][2] for e in columns)-min(e['box'][0] for e in columns)==288,(page,section,f,len(columns))
        knobs=len([e for e in leaves if e['kind']=='Knob']);switches=len([e for e in leaves if e['kind']=='Button' and e['name']=='switch'])
        assert (knobs,switches)==[(16,0),(12,2)][section],(page,section,knobs,switches)
        assert not [e for e in leaves if e['kind']=='Button' and e['box'][1]<64 and e['name'].startswith('PAGE')],(page,section,'no page buttons in the tab row')
        assert not [e for e in leaves if e['kind']=='Button' and e['name'].startswith('Q-LINKS')],(page,section,'no layer buttons')
    # A dial carries the colour of its section: the filters their own, the effects theirs (0.3.46:
    # the effect pages showed the filter's dials after the filter got its own colour).
    for chart,tag in [('filters','-flt-'),('fx','-fx-')]:
        if VIEWS[page]['chart']==chart:
            wrong=sorted({e['data']['filmStrip'] for e in leaves if e['kind']=='Knob' and 64<=e['box'][1]<568 and tag not in e['data']['filmStrip']})
            assert not wrong,(page,section,chart,wrong)
    if page==24 and section==0:
        assert len([e for e in leaves if e['name']=='Effect switch'])==9 and len([e for e in leaves if e['name']=='Effect name'])==9,'FX overview must expose all nine places of the chain'
        assert len(VIEWS[page]['sections'])==1 and sorted(e['name'] for e in leaves if e['kind']=='Button' and 64<=e['box'][1]<568 and e['name']!='Effect switch')==sorted([str(n) for n in range(1,10)]+['< EARLIER','LATER >']),'the overview is one page: nine places and two move buttons'
    # 0.3.49, one coherent design: wherever a page has two pages of controls, PAGE 1 and PAGE 2
    # are two small buttons in the same place, at the top right of the page under the tab row.
    # The tab row holds tabs only (0.3.48 had the page buttons in it; the user rejected that).
    assert not [e for e in leaves if e['kind']=='Button' and e['box'][1]<64 and e['name'].startswith(('PAGE','Q-LINKS'))],(page,section,'the tab row holds tabs only')
    if isinstance(VIEWS[page]['chart'],int) or VIEWS[page]['chart'] in ('lfo','filters'):
        places=sorted((e['name'],e['box'][0],e['box'][1],e['box'][2]-e['box'][0],e['box'][3]-e['box'][1]) for e in leaves if e['kind']=='Button' and e['name'] in ('PAGE 1','PAGE 2'))
        assert places==[('PAGE 1',904,68,172,52),('PAGE 2',1084,68,172,52)],(page,section,places)
result=audit();assert not result['errors'],result['errors']
print(f'PASS: 43 module views / {checked} states, seven footer groups, all nine FX, exact 16:9 oscillator graphs, native-size controls, complete bindings, Q-Link map and no label/button collisions')
