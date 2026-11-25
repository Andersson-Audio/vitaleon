"""Build measurement copies of Vitaleon with different compiler switches.
Each copy is its own plugin (name "Vitaleon T<n>", its own id) with the device measurement
compiled in, so all of them can sit on the MPC beside the release.
usage: make_variants.py <sdk folder> [variant numbers...]
"""
import json, os, shutil, subprocess, sys
from pathlib import Path

VARIANTS = {
    # zig c++ compiles -O2 and -O3 to the same code (checked: the libraries differ only in
    # their name and id), so there is no separate -O3 variant.
    0: ('-O2 (as released)', ''),
    1: ('-Os, optimised for code size', '-Os'),
    2: ('link-time optimisation', '-flto'),
    3: ('link-time optimisation, fast floating point', '-flto -ffast-math -fno-finite-math-only'),
    4: ('link-time optimisation, relaxed floating point', '-flto -fno-math-errno -fno-trapping-math -ffp-contract=fast'),
}
sdk = Path(sys.argv[1]).resolve()
source = sdk / 'plugins' / 'Vitaleon'
chosen = [int(a) for a in sys.argv[2:]] or sorted(VARIANTS)
for n in chosen:
    label, flags = VARIANTS[n]
    project = sdk / 'plugins' / ('VitaleonT%d' % n)
    if project.exists():
        shutil.rmtree(project)
    project.mkdir()
    shutil.copytree(source / 'src', project / 'src')
    shutil.copytree(source / 'skins', project / 'skins')
    (project / 'Content' / 'Banks').mkdir(parents=True)
    shutil.copy2(source / 'Content' / 'Banks' / 'Vitaleon Factory.vitalbank', project / 'Content' / 'Banks')
    for name in ['ui.json', 'LICENSE.txt']:
        if (source / name).exists():
            shutil.copy2(source / name, project / name)
    manifest = json.loads((source / 'plugin.json').read_text(encoding='utf-8'))
    manifest['name'] = 'Vitaleon T%d' % n
    manifest['uid'] = 'Al7' + str(n)
    manifest['description'] = 'Measurement copy, not a release: ' + label
    (project / 'plugin.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    # Step 1: the usual build without extra switches (desktop library, parameter list, folder).
    # Step 2: the 32-bit ARM library again with the switches. Link-time optimisation does not
    # link for the Windows desktop library, and only the 32-bit build runs on the Live II.
    defines = ['--define', 'VITALEON_CPUDIAG=1', '--define', 'VITALEON_VARIANT=%d' % n]
    log = project / 'build.log'
    with log.open('w', encoding='utf-8') as out:
        result = subprocess.run([sys.executable, str(sdk / 'tools' / 'build.py'), str(project)] + defines,
                                env=dict(os.environ, MPC_EXTRA_CXXFLAGS='', PYTHONUTF8='1'), stdout=out, stderr=subprocess.STDOUT)
        if result.returncode == 0 and flags:
            # The same call tools/build.py makes for arm32, plus the switches.
            output = project / 'build' / ('Andersson Audio - VST - Vitaleon T%d' % n) / 'VST' / 'vitaleon-arm32.so'
            sources = sorted(str(f) for f in (project / 'src').iterdir() if f.suffix.lower() == '.cpp')
            argv = [str(sdk / '.sdk-tools' / 'zigenv' / 'Scripts' / 'python.exe'), '-m', 'ziglang', 'c++',
                    '-std=c++17', '-fPIC', '-shared', '-fvisibility=hidden', '-fvisibility-inlines-hidden', '-ffunction-sections', '-fdata-sections',
                    '-O2', '-I' + str(sdk / 'framework'), '-I' + str(project / 'src'), '-DVITALEON_CPUDIAG=1', '-DVITALEON_VARIANT=%d' % n] + flags.split() + [
                    '-Wl,--gc-sections', '-Wl,-s', '-Wl,--version-script=' + str(sdk / 'tools' / 'exports.map'),
                    '-target', 'arm-linux-gnueabihf.2.31', '-mcpu=cortex_a17', '-o', str(output)] + sources
            output.unlink()
            result = subprocess.run(argv, stdout=out, stderr=subprocess.STDOUT)
    built = project / 'build' / ('Andersson Audio - VST - Vitaleon T%d' % n) / 'VST' / 'vitaleon-arm32.so'
    size = built.stat().st_size if built.exists() else 0
    print('T%d  %-55s exit %d  arm32 %.2f MB' % (n, label, result.returncode, size / 1e6), flush=True)
    if result.returncode:
        print(''.join(log.read_text(encoding='utf-8', errors='replace').splitlines(True)[-12:]))
