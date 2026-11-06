#!/usr/bin/env python3
# build; each javac error inside a generated member puts that member (or
# class) in skip.txt; again until it builds or an error is not ours
import re, subprocess, sys, os
S = '/src/silver/install/tmp/apigen/'
G = '/src/silver/featured/android/apigen/'
OV = '/src/silver/featured/android/android_translation_layer/src/api-impl/'
for rnd in range(int(sys.argv[1]) if len(sys.argv) > 1 else 12):
    r = subprocess.run('cd /src/silver && silver --clean --build featured/android > %sb-gen.log 2>&1' % S,
                       shell=True)
    log = open(S + 'b-gen.log', errors='replace').read()
    errs = re.findall(r'src/api-impl/([\w/$]+\.java):(\d+): error: (.*)', log)
    if r.returncode == 0 and not errs:
        print('round', rnd, 'built'); sys.exit(0)
    if not errs:
        print('round', rnd, 'failed, no javac error'); print('\n'.join(log.splitlines()[-15:])); sys.exit(1)
    skip = set(l.strip() for l in open(G + 'skip.txt')) if os.path.exists(G + 'skip.txt') else set()
    new = set(); foreign = []
    for rel, line, msg in errs:
        if not os.path.exists(OV + rel):
            foreign.append((rel, line, msg)); continue
        lines = open(OV + rel).read().split('\n')
        key = None
        i = int(line) - 1
        # nearest GEN marker above, inside the same generated chunk
        for k in range(i, max(-1, i - 400), -1):
            m = re.search(r'// GEN (.*)$', lines[k])
            if m:
                key = m.group(1).strip(); break
        # a member marker sits right above its one line; farther up is a class
        if key and key.startswith('member:') and i - k > 1:
            key = None
            for kk in range(i, max(-1, i - 2000), -1):
                m = re.search(r'// GEN (class:.*)$', lines[kk])
                if m: key = m.group(1).strip(); break
        # a header line of a class this made: that class
        if not key and any('made by gen.py' in l for l in lines[:4]):
            key = 'class:' + rel[:-5].replace('/', '.')
        if key: new.add(key)
        else: foreign.append((rel, line, msg))
    new -= skip
    print('round', rnd, 'errors', len(errs), 'skipped', len(new), 'not ours', len(foreign))
    for f in foreign[:8]: print('  ', f)
    if not new:
        sys.exit(2)
    with open(G + 'skip.txt', 'a') as f:
        for k in sorted(new): f.write(k + '\n')
    subprocess.run(['python3', '-I', G + 'gen.py'], cwd='/src/silver')
sys.exit(3)
