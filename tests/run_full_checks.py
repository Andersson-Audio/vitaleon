"""Build integration checks. Optional: --presets path/to/your/Vital/library."""
from pathlib import Path
import argparse,subprocess,shutil,sys,os,json,zipfile,struct,math
parser=argparse.ArgumentParser();parser.add_argument('--presets',default='');args=parser.parse_args()
project=Path(__file__).resolve().parents[1];sdk=project.parents[1];src=project/'src'
out=project/'build/full-checks';out.mkdir(parents=True,exist_ok=True)
compiler=shutil.which('clang++') or shutil.which('g++')
if not compiler:raise SystemExit('A C++17 clang++ or g++ compiler is required.')
exe=out/('full_checks.exe' if sys.platform=='win32' else 'full_checks')
with (out/'build.log').open('w') as log:
    result=subprocess.run([compiler,'-std=c++17','-O2','-I'+str(sdk/'framework'),str(project/'tests/full_checks.cpp'),*[str(src/f) for f in ['Engine.cpp','Pffft.cpp','PffftCommon.cpp','WavetableImport.cpp','BankInflate.cpp']],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT)
if result.returncode:print((out/'build.log').read_text());raise SystemExit(result.returncode)
fixture=out/'test-content';presets=fixture/'Content/Presets';banks=fixture/'Content/Banks';presets.mkdir(parents=True,exist_ok=True);banks.mkdir(parents=True,exist_ok=True)
def preset(cutoff,**extra):return json.dumps({'synth_version':'1.0.6','settings':{'filter_1_cutoff':cutoff,'volume':3136,**extra}})
(presets/'01 Import Test.vital').write_text(json.dumps({'synth_version':'1.0.6','author':'Test Author','preset_style':'Lead','comments':'A short note about this sound.','macro1':'BRIGHT','macro2':'MOVE','macro3':'TONE','settings':{'filter_1_cutoff':60,'volume':3136}}))
for n in range(4,11):(presets/f'{n:02} Browser Fixture.vital').write_text(preset(40+n))
(presets/'03 Unsupported.vital').write_text(preset(99,filter_1_model=99))
for stale in banks.glob('*'):stale.unlink()   # a bank installed by an interrupted run
shutil.rmtree(out/'Vitaleon',ignore_errors=True)   # sounds saved by an earlier run (0.3.44: <drive>/Vitaleon/Sounds)
with zipfile.ZipFile(banks/'Imported Bank.vitalbank','w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('Presets/02 Bank Test.vital',preset(77))
with zipfile.ZipFile(banks/'Second Bank.vitalbank','w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('Presets/02 Bank Test.vital',preset(88))
external=fixture/'Content/Storage Tests';external.mkdir(exist_ok=True)
for name,values in [('A small',[31]),('B large',[71,72,73]),('C same size',[81]),('D empty',[])]:
    with zipfile.ZipFile(external/(name+'.vitalbank'),'w',compression=zipfile.ZIP_DEFLATED) as z:
        for i,value in enumerate(values):z.writestr('Presets/'+('Same name' if i==0 else 'Sound '+str(i))+'.vital',preset(value))
(external/'E broken.vitalbank').write_bytes(b'broken bank')
loose=external/'Loose Sounds';loose.mkdir(exist_ok=True)
(loose/'First loose.vital').write_text(preset(52));(loose/'Second loose.vital').write_text(preset(53,osc_1_spectral_morph_type=14,pitch_env_type=0))
for i in range(8):(external/f'Folder {i}').mkdir(exist_ok=True)
# Files for the library import (0.3.25): single cycle, stated cycle length, plain table, sample, junk, LFO shape.
imports=external/'Imports';imports.mkdir(exist_ok=True)
def wav(path,values,channels=1,rate=44100,floats=False,extra=b''):
    data=b''.join(struct.pack('<f',v) if floats else struct.pack('<h',int(round(max(-1,min(1,v))*32767))) for v in values)
    bits=32 if floats else 16;block=channels*bits//8
    body=b'WAVEfmt '+struct.pack('<IHHIIHH',16,3 if floats else 1,channels,rate,rate*block,block,bits)+extra+b'data'+struct.pack('<I',len(data))+data
    path.write_bytes(b'RIFF'+struct.pack('<I',len(body))+body)
def sine(length,cycles=1):return [.8*math.sin(2*math.pi*cycles*i/length) for i in range(length)]
wav(imports/'Cycle 600.wav',sine(600))
clm=b'<!>1024 00000000 wavetable    ';wav(imports/'Clm 1024.wav',sine(1024)+sine(1024,2),floats=True,extra=b'clm '+struct.pack('<I',len(clm))+clm)
wav(imports/'Table 4x2048.wav',[v for n in range(4) for v in sine(2048,n+1)])
wav(imports/'Kick.wav',[v*math.exp(-i/1500) for i,v in enumerate(sine(8820,40)) for _ in range(2)],channels=2)
(imports/'Broken.wav').write_bytes(b'this is not audio')
# The plugin's own folders of tables and samples, for the TABLE LIST / SAMPLE LIST buttons (0.3.28).
(fixture/'Content/Wavetables').mkdir(exist_ok=True);(fixture/'Content/Samples').mkdir(exist_ok=True)
wav(fixture/'Content/Wavetables/Browse Table.wav',[v for n in range(2) for v in sine(2048,n+1)])
wav(fixture/'Content/Samples/Browse Hit.wav',[v*math.exp(-i/900) for i,v in enumerate(sine(4410,30))])
(imports/'Shape.vitallfo').write_text(json.dumps({'name':'Shape','num_points':3,'points':[0,1,.5,0,1,1],'powers':[0,0,0],'smooth':False}))
env=os.environ.copy();env['MPC_PLUGIN_FOLDER']=str(fixture)
with (out/'results.log').open('w') as log:
    result=subprocess.run([str(exe),args.presets,str(out/'preview-values.json'),str(fixture),str(project/'skins/TUI.json')],env=env,stdout=log,stderr=subprocess.STDOUT)
    empty=out/'no-content';(empty/'Content').mkdir(parents=True,exist_ok=True);bare=os.environ.copy();bare['MPC_PLUGIN_FOLDER']=str(empty)
    log.flush();second=subprocess.run([str(exe),'--no-content',str(out/'init-values.json')],env=bare,stdout=log,stderr=subprocess.STDOUT)
    # 0.3.39: a plugin with the factory bank, another bank that sorts before it, and a loose sound.
    factory=out/'factory-start';start=factory/'Content/Banks';start.mkdir(parents=True,exist_ok=True);(factory/'Content/Presets').mkdir(exist_ok=True)
    with zipfile.ZipFile(start/'Vitaleon Factory.vitalbank','w',compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('Presets/BA Alpha.vital',preset(33));z.writestr('Presets/LD Beta.vital',preset(44))
    with zipfile.ZipFile(start/'Aaa Bank.vitalbank','w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('Presets/Other One.vital',preset(55))
    (factory/'Content/Presets/A Loose One.vital').write_text(preset(66))
    # 0.3.52: three factory tables (the numbers need not be complete) and a table of the user's.
    tables=factory/'Content/Wavetables';(tables/'Factory').mkdir(parents=True,exist_ok=True)
    for number,name,cycles in [(1,'Alpha',1),(2,'Beta',2),(5,'Epsilon',3)]:wav(tables/'Factory'/f'{number:02d} {name}.wav',[v for n in range(2) for v in sine(2048,cycles+n)])
    wav(tables/'Own Table.wav',[v for n in range(2) for v in sine(2048,5+n)])
    # 0.3.59: a folder of tables that ships with the plugin gets pages of its own.
    (tables/'Pack').mkdir(exist_ok=True)
    wav(tables/'Pack'/'Gamma.wav',[v for n in range(2) for v in sine(2048,7+n)]);wav(tables/'Pack'/'Delta.wav',[v for n in range(2) for v in sine(2048,9+n)])
    home=os.environ.copy();home['MPC_PLUGIN_FOLDER']=str(factory)
    # 0.3.53: the user's folder of wavetables (beside the plugin's folder) and a folder that
    # stands for a USB stick. Made here, after the first two runs, so that they see neither.
    mine=out/'Vitaleon/Wavetables';shutil.rmtree(mine,ignore_errors=True);mine.mkdir(parents=True)
    stick=out/'table-stick';shutil.rmtree(stick,ignore_errors=True);(stick/'other').mkdir(parents=True)
    wav(mine/'User One.wav',[-.9+1.8*i/2047 for i in range(2048)]*2)
    wav(mine/'User Zed.wav',[(.8 if i<1024 else -.8) for i in range(2048)]*2)
    wav(stick/'Stick Table.wav',[v for n in range(3) for v in sine(2048,n+1)])
    wav(stick/'other'/'Stick Table.wav',[v for n in range(5) for v in sine(2048,n+2)])
    (stick/'Broken.wav').write_bytes(b'this is not audio')
    log.flush();third=subprocess.run([str(exe),'--factory-start',str(stick)],env=home,stdout=log,stderr=subprocess.STDOUT)
print((out/'results.log').read_text());raise SystemExit(result.returncode or second.returncode or third.returncode)
