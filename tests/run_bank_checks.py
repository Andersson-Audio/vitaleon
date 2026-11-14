"""ZIP compatibility and malformed-bank regression tests; generates its own fixtures."""
from pathlib import Path
import shutil,subprocess,zipfile,struct,sys
project=Path(__file__).resolve().parents[1];out=project/'build/bank-checks';out.mkdir(parents=True,exist_ok=True)
for name,method in [('stored',zipfile.ZIP_STORED),('deflated',zipfile.ZIP_DEFLATED)]:
    with zipfile.ZipFile(out/(name+'.vitalbank'),'w',compression=method) as z:
        z.writestr('Presets/Test.vital','{"synth_version":"1.0.6"}')
        z.writestr('empty.txt','')
base=(out/'stored.vitalbank').read_bytes();cd=base.index(b'PK\x01\x02')
for name,offset,fmt,value in [('crc',cd+16,'I',1),('encrypted',cd+8,'H',1),('offset',cd+42,'I',0xfffffff0)]:
    data=bytearray(base);struct.pack_into('<'+fmt,data,offset,value);(out/(name+'.vitalbank')).write_bytes(data)
(out/'truncated.vitalbank').write_bytes(base[:-14])
with zipfile.ZipFile(out/'duplicate.vitalbank','w') as z:
    z.writestr('Presets/Test.vital','one');z.writestr('Presets/Test.vital','two')
exe=out/('bank_checks.exe' if sys.platform=='win32' else 'bank_checks')
subprocess.run([shutil.which('clang++') or shutil.which('g++'),'-std=c++17','-O2',str(project/'tests/bank_checks.cpp'),str(project/'src/BankInflate.cpp'),'-o',str(exe)],check=True)
raise SystemExit(subprocess.run([str(exe),str(out)]).returncode)
