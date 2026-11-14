"""Windows: compare two native release DLLs across deterministic MIDI/state scenarios.
Usage: python tests/compare_releases.py <old.dll> <new.dll>
Builds the test host with clang++/g++; captures and results go under build/comparison.
"""
from pathlib import Path
import array, json, math, shutil, subprocess, sys
project=Path(__file__).resolve().parents[1]
sdk=project.parents[1]
if sys.platform!='win32' or len(sys.argv)!=3:
    raise SystemExit(__doc__)
libraries=[Path(p).resolve(strict=True) for p in sys.argv[1:]]
compiler=shutil.which('clang++') or shutil.which('g++')
if not compiler: raise SystemExit('Install clang++ or g++.')
out=project/'build/comparison';out.mkdir(parents=True,exist_ok=True)
exe=out/'scenarios.exe'
subprocess.run([compiler,'-std=c++17','-O2','-I'+str(sdk/'framework'),
                str(project/'tests/scenarios_windows.cpp'),'-o',str(exe)],check=True)
results=[]
for rate in [22050,44100,48000,88200,96000]:
    captures=[]
    for index,dll in enumerate(libraries):
        file=out/f'scenario-{rate}-{index}.f32'
        subprocess.run([str(exe),str(dll),str(file),str(rate)],check=True)
        data=array.array('f');data.frombytes(file.read_bytes());captures.append(data)
    a,b=captures;assert len(a)==len(b)
    error=sum((x-y)**2 for x,y in zip(a,b));signal=sum(x*x for x in a)
    result=dict(rate=rate,samples=len(a),bit_identical=a.tobytes()==b.tobytes(),
                max_absolute_error=max(abs(x-y) for x,y in zip(a,b)),
                error_relative_db=10*math.log10(error/signal) if error else None)
    results.append(result);print(json.dumps(result))
(out/'scenario-results.json').write_text(json.dumps(results,indent=2))
raise SystemExit(0 if all(r['bit_identical'] for r in results) else 1)
