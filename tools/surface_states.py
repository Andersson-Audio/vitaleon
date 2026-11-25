"""Expand the actual native skin in every module/section state, including aliases."""
import json,re
from pathlib import Path
from surface_spec import VIEWS
PROJECT=Path(__file__).resolve().parents[1]
UI=json.loads((PROJECT/'skins/TUI.json').read_text(encoding='utf-8'))['pageData']
DEFS={d['key']:d['value'] for d in UI['componentDefinitions']['localComponentDefinitions']}
PARAMS=json.loads((PROJECT/'build/params.json').read_text())['params']
NAMES={p['name']:p['index'] for p in PARAMS}
def resolve(h,mapping):
    for _ in range(8):
        if h in mapping and mapping[h]!=h:h=mapping[h]
        else:break
    return h
def states():
    for i,v in enumerate(VIEWS):
        for section in range(len(v['sections'])):
            values={NAMES['UI Page']:i/(len(VIEWS)-1),NAMES['UI Group']:v['group']/6,NAMES['UI Section']:section/31}
            values.update({NAMES[f'storage_available_{row}']:1 for row in range(6)})
            yield i,section,values
def bound(handle,mapping):
    handle=resolve(handle,mapping)
    parameter=re.search(r'Parameter (\d+)$',handle)
    return bool(parameter and 0<=int(parameter[1])<len(PARAMS)) or handle.startswith('Local/')
def expand(entries,values,ox=0,oy=0,mapping=None,tap=None):
    # tap: the enclosing component when it takes a tap itself (Mouse Down -> Toggle Switch).
    # Inside such a component a label on its button is intended: the touch reaches the
    # component's action (library rows since 0.3.28).
    mapping=mapping or {};result=[]
    for e in entries:
        b=e['bounds'];d=e['componentData']
        if b['whenVisible']=='WhenFocussed':continue
        local=dict(mapping)
        for pair in e['handle remapping']['map']:local[pair['key']]=resolve(pair['value'],mapping)
        # A custom container has its OWN data model. Valid child parameters do
        # not rescue an unbound parent hidden by showWhenDataModelInvalid.
        primary=d['data'].get('handleName')
        if primary and b['showWhenDataModelInvalid']=='Hide' and not bound(primary,local):continue
        visible=True
        for cond in b.get('additionalInvalidatingHandles',[]):
            m=re.match(r'IndexedEnabling/(\d+)/(\d+)/(.*)',cond)
            if m:
                h=resolve(m[3],local);p=re.search(r'Parameter (\d+)',h)
                if p:visible &= round(values.get(int(p[1]),PARAMS[int(p[1])]['default'])*(int(m[2])-1))==int(m[1])
        if not visible:continue
        x,y,w,h=map(int,b['bounds'].split());x+=ox;y+=oy
        if d['type'] in DEFS:
            takes=any(a.get('onAction')=='Mouse Down' and a.get('handler')=='Toggle Switch' for a in DEFS[d['type']].get('actions',[]))
            result+=expand(DEFS[d['type']]['componentsData'],values,x,y,local,(d['type'],x,y) if takes else tap)
        else:result.append(dict(kind=d['type'],name=d['name'],box=(x,y,x+w,y+h),data=d['data'],mapping=local,tap=tap))
    return result
def audit():
    errors=[];count=0;buttons=0
    for page,section,values in states():
        leaves=expand(UI['tabs'][0]['componentDefinition']['componentsData'],values);count+=1
        if not any(e['kind']=='Label' and 64<=e['box'][1]<568 for e in leaves):errors.append((page,section,'module content is entirely hidden'))
        for e in leaves:
            x,y,r,b=e['box']
            if x<0 or y<0 or r>1280 or b>628:errors.append((page,section,'out of bounds',e['name'],e['box']))
        for button in [e for e in leaves if e['kind']=='Button']:
            buttons+=1
            x,y,r,b=button['box']
            for label in [e for e in leaves if e['kind']=='Label']:
                a,c,d,f=label['box']
                if button.get('tap') and button.get('tap')==label.get('tap'):continue   # the component takes the tap
                if min(r,d)>max(x,a) and min(b,f)>max(y,c):errors.append((page,section,'label intercepts button',button['name'],label['name']))
        foot=[e for e in leaves if e['kind']=='Button' and e['box'][1]==568]
        if len(foot)!=7:errors.append((page,section,'footer groups',len(foot)))
    return dict(states=count,buttons=buttons,errors=errors)
if __name__=='__main__':
    result=audit();print(json.dumps(result,indent=2));raise SystemExit(bool(result['errors']))
