"""Detect Label/Button hit-region collisions, including expanded components.

Models the hardware rule reported in upstream PR #5, not the MPC renderer:
https://github.com/inmusic-software/mpc-plugin-builder-sdk/pull/5
Unlike direct parameter-write tests this checks the actual touch geometry.
"""
from pathlib import Path
import argparse,json,re

def audit(path):
    ui=json.loads(Path(path).read_text(encoding='utf-8'))['pageData']
    if len(ui['tabs'])==1 and ui['tabs'][0]['tabName']=='VITALEON':
        import surface_states
        previous=surface_states.UI;surface_states.UI=ui
        previous_defs=surface_states.DEFS;surface_states.DEFS={d['key']:d['value'] for d in ui['componentDefinitions']['localComponentDefinitions']}
        result=surface_states.audit();surface_states.UI=previous;surface_states.DEFS=previous_defs
        return {'pages':40,'states':result['states'],'button_instances':result['buttons'],'label_button_collisions':len(result['errors']),'findings':result['errors']}
    definitions={d['key']:d['value'] for d in ui['componentDefinitions']['localComponentDefinitions']}
    findings=[];button_count=0
    def expand(entries,panel,ox=0,oy=0):
        leaves=[]
        for entry in entries:
            b=entry['bounds'];d=entry['componentData']
            if b['whenVisible']=='WhenFocussed':continue
            local=[re.search(r'IndexedEnabling/(\d+)/2/Local/',h) for h in b.get('additionalInvalidatingHandles',[])]
            if any(m and int(m[1])!=panel for m in local):continue
            x,y,w,h=map(int,b['bounds'].split());x+=ox;y+=oy
            if d['type'] in definitions:leaves+=expand(definitions[d['type']]['componentsData'],panel,x,y)
            else:leaves.append({'kind':d['type'],'name':d['name'],'box':(x,y,x+w,y+h),'data':d['data']})
        return leaves
    for tab in ui['tabs']:
        for panel in (0,1):
            leaves=expand(tab['componentDefinition']['componentsData'],panel)
            labels=[e for e in leaves if e['kind']=='Label']
            for button in (e for e in leaves if e['kind']=='Button'):
                button_count+=1;x1,y1,x2,y2=button['box']
                for label in labels:
                    a,b,c,d=label['box'];w=min(c,x2)-max(a,x1);h=min(d,y2)-max(b,y1)
                    if w>0 and h>0:findings.append({'page':tab['tabName'],'panel':panel,'button':button['name'],'label':label['name'],'covered_pixels':w*h,'button_pixels':(x2-x1)*(y2-y1)})
    return {'pages':len(ui['tabs']),'states':len(ui['tabs'])*2,'button_instances':button_count,'label_button_collisions':len(findings),'findings':findings}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--skin',default=str(Path(__file__).resolve().parents[1]/'skins/TUI.json'));parser.add_argument('--json')
    args=parser.parse_args();result=audit(args.skin)
    if args.json:Path(args.json).write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(('FAIL' if result['findings'] else 'PASS')+f": {result['states']} page states, {result['button_instances']} button instances, {result['label_button_collisions']} label/button collisions")
    for finding in result['findings'][:12]:print(json.dumps(finding))
    raise SystemExit(bool(result['findings']))
