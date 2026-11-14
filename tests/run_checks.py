"""Build the direct engine checks. Run from any directory. Requires clang++ or g++."""
from pathlib import Path
import shutil
import subprocess
import sys

project = Path(__file__).resolve().parents[1]
sdk = project.parents[1]
compiler = shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise SystemExit('Install a C++17 clang++/g++ toolchain to run direct engine checks.')
out = project / 'build' / 'checks'
out.mkdir(parents=True, exist_ok=True)
exe = out / ('engine_checks.exe' if sys.platform == 'win32' else 'engine_checks')
subprocess.run([compiler, '-std=c++17', '-O2', '-I' + str(sdk / 'framework'),
                str(project / 'tests/engine_checks.cpp'), str(project / 'src/Engine.cpp'),
                str(project / 'src/Pffft.cpp'), str(project / 'src/PffftCommon.cpp'),
                str(project / 'src/WavetableImport.cpp'), str(project / 'src/BankInflate.cpp'),
                '-o', str(exe)], check=True)
with (out / 'engine_checks.log').open('w') as log:
    result = subprocess.run([str(exe)], stdout=log, stderr=subprocess.STDOUT)
print((out / 'engine_checks.log').read_text())
if result.returncode:
    raise SystemExit(result.returncode)
fft_exe = out / ('fft_compare.exe' if sys.platform == 'win32' else 'fft_compare')
subprocess.run([compiler, '-std=c++17', '-O2', '-I' + str(project / 'src/engine'),
                str(project / 'tests/fft_compare.cpp'), str(project / 'src/Pffft.cpp'),
                str(project / 'src/PffftCommon.cpp'), '-o', str(fft_exe)], check=True)
raise SystemExit(subprocess.run([str(fft_exe)]).returncode)
