from pathlib import Path
import subprocess,shutil,sys
p=Path(__file__).resolve().parents[1];out=p/'build/sample-checks';out.mkdir(parents=True,exist_ok=True)
exe=out/('sample_prepare_checks.exe' if sys.platform=='win32' else 'sample_prepare_checks')
with (out/'build.log').open('w') as log:
 subprocess.run([shutil.which('clang++') or shutil.which('g++'),'-std=c++17','-O2',str(p/'tests/sample_prepare_checks.cpp'),*[str(p/'src'/f) for f in ['Pffft.cpp','PffftCommon.cpp','WavetableImport.cpp','BankInflate.cpp']],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
raise SystemExit(subprocess.run([str(exe)]).returncode)
