#!/usr/bin/env python3
# branch totals without the failure side of check macros
import json, re, sys

FAIL = {'validate', 'verify', 'error', 'fault', 'assert'}
src_cache, def_cache = {}, {}

def lines(f):
    if f not in src_cache:
        try:
            src_cache[f] = open(f).read().split('\n')
        except OSError:
            src_cache[f] = []
    return src_cache[f]

def text(f, l, c1, c2):
    ls = lines(f)
    return ls[l - 1][c1 - 1:c2 - 1] if 0 < l <= len(ls) else ''

def cond_spot(f, name, near):
    # the first use of the first param in the nearest #define
    key = (f, name, near)
    if key in def_cache:
        return def_cache[key]
    ls, best = lines(f), None
    pat = re.compile(r'#define\s+' + name + r'\(\s*(\w+)')
    for i, t in enumerate(ls[:near]):
        m = pat.search(t)
        if m:
            best = (i, m)
    spot = None
    if best:
        i, m = best
        param, l, start = m.group(1), i, m.end()
        while l < len(ls) and spot is None:
            for u in re.finditer(r'\b' + param + r'\b', ls[l][start:]):
                c = start + u.start() + 1
                spot = (l + 1, c, c + len(param))
                break
            if not ls[l].rstrip().endswith('\\'):
                break
            l, start = l + 1, 0
    def_cache[key] = spot
    return spot

def classify(fn):
    # per branch: 'keep', 'drop', or 'fail_true'/'fail_false'
    names = fn['filenames']
    parent = {}
    for r in fn['regions']:
        if r[7] == 1:
            parent[r[6]] = (r[5], r)
    fail = {}
    for e, (p, r) in parent.items():
        name = text(names[p], r[0], r[1], r[3]) if r[0] == r[2] else ''
        if name in FAIL:
            near = min([x[0] for x in fn['regions'] if x[5] == e] or [r[0]])
            fail[e] = cond_spot(names[e], name, near)

    def place(b):
        # innermost first: (fail expansion, where in it)
        out, fid, pos = [], b[6], (b[0], b[1], b[3])
        while True:
            if fid in fail:
                s = fail[fid]
                if s and pos == (s[0], s[1] - 1, s[2] + 1):
                    out.append((fid, 'paren'))
                elif s and pos == (s[0], s[1] - 2, s[2] + 1):
                    out.append((fid, 'not'))
                elif s and pos[0] == s[0] and pos[1] >= s[1] and pos[2] <= s[2]:
                    out.append((fid, 'cond'))
                else:
                    out.append((fid, 'args'))
            if fid not in parent:
                return out
            fid, r = parent[fid]
            pos = (r[0], r[1], r[3])

    res = []
    for b in fn['branches']:
        p = place(b)
        if not p:
            res.append('keep')
        elif any(w == 'args' for _, w in p):
            res.append('drop')
        elif p[0][1] == 'paren':
            res.append('fail_false')
        elif p[0][1] == 'not':
            res.append('fail_true')
        else:
            res.append('keep')
    return res

def main(path, want):
    d = json.load(open(path))['data'][0]
    tot = {}
    for fn in d['functions']:
        f = fn['filenames'][0]
        key = next((w for w in want if f.endswith(w)), None)
        if not key:
            continue
        t = tot.setdefault(key, [0, 0, 0, 0, 0])
        for b, how in zip(fn['branches'], classify(fn)):
            t[0] += 2
            t[1] += (b[4] > 0) + (b[5] > 0)
            if how == 'keep':
                t[2] += 2
                t[3] += (b[4] > 0) + (b[5] > 0)
            elif how in ('fail_true', 'fail_false'):
                t[2] += 1
                t[3] += (b[5] > 0) if how == 'fail_true' else (b[4] > 0)
                t[4] += 1
    print('branches without check-macro failure sides:')
    for k, (a, ac, b, bc, s) in tot.items():
        print(f'  {k:10} all {ac}/{a} {100.0 * ac / max(a, 1):.2f}%'
              f'  real {bc}/{b} {100.0 * bc / max(b, 1):.2f}%'
              f'  ({s} simple checks)')

main(sys.argv[1], sys.argv[2:])
