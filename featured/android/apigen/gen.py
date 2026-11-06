#!/usr/bin/env python3
# Fills ATL's api-impl with what apps reference and it lacks, each member
# declared exactly as android.jar declares it, bodies Android's "nothing
# here" answer. Writes into the overlay from a snapshot (base), so a run
# can be repeated; members named in skip.txt are left out.
import os, re, sys, subprocess, struct, zipfile, json, shutil

ROOT = '/src/silver/'
# working files: the base snapshot, its jar, android.jar (platform-35,
# from dl.google.com's platform tools), what the last run wrote
S = ROOT + 'install/tmp/apigen/'
# the inputs kept in the repo: the apps' scans and the skip list
G = ROOT + 'featured/android/apigen/'
OV = ROOT + 'featured/android/android_translation_layer/src/api-impl/'
CK = ROOT + 'checkout/android_translation_layer/android_translation_layer/'
BASE = S + 'gen/base/'
JAR = S + 'sdk/android.jar'
JAVAP = ROOT + 'platform/native/lib/jdk17/bin/javap'
DEX = ROOT + 'platform/native/lib/java/dex/'
# ATL's jar as it is without this run's work (a build's jar has it)
LIBJARS = [S + 'gen/base-api-impl.jar'] + [DEX + 'art/' + j for j in (
    'core-libart-hostdex.jar', 'core-oj-hostdex.jar', 'apache-xml-hostdex.jar',
    'okhttp-hostdex.jar', 'bouncycastle-hostdex.jar', 'apachehttp-hostdex.jar')]

# ---- dex: which classes and members ATL (and the core libraries) have ----
def uleb(d, o):
    r = sh = 0
    while True:
        b = d[o]; o += 1; r |= (b & 127) << sh; sh += 7
        if b < 128: return r, o

def dex_load(d, out):
    u16 = lambda o: struct.unpack_from('<H', d, o)[0]
    u32 = lambda o: struct.unpack_from('<I', d, o)[0]
    strs = []
    for i in range(u32(0x38)):
        l, p = uleb(d, u32(u32(0x3c) + 4 * i)); e = d.index(b'\0', p)
        strs.append(d[p:e].decode('utf8', 'replace'))
    types = [strs[u32(u32(0x44) + 4 * i)] for i in range(u32(0x40))]
    po = u32(0x4c); protos = []
    for i in range(u32(0x48)):
        sh, rt, pa = struct.unpack_from('<III', d, po + 12 * i)
        ps = [types[u16(pa + 4 + 2 * k)] for k in range(u32(pa))] if pa else []
        protos.append(('(' + ''.join(ps) + ')', types[rt]))
    mo = u32(0x5c)
    meths = [struct.unpack_from('<HHI', d, mo + 8 * i) for i in range(u32(0x58))]
    fo = u32(0x54)
    fields = [struct.unpack_from('<HHI', d, fo + 8 * i) for i in range(u32(0x50))]
    for c in range(u32(0x60)):
        co = u32(0x64) + 32 * c; cls = types[u32(co)]; acc = u32(co + 4)
        cdo = u32(co + 24); ms = set(); fs = set()
        if cdo:
            o = cdo; sf, o = uleb(d, o); inf, o = uleb(d, o); dm, o = uleb(d, o); vm, o = uleb(d, o)
            for cnt in (sf, inf):
                fi = 0
                for _ in range(cnt):
                    x, o = uleb(d, o); fi += x; _a, o = uleb(d, o); fs.add(strs[fields[fi][2]])
            for cnt in (dm, vm):
                mi = 0
                for _ in range(cnt):
                    x, o = uleb(d, o); mi += x; _a, o = uleb(d, o); _b, o = uleb(d, o)
                    m = meths[mi]; pr = protos[m[1]]
                    ms.add((strs[m[2]], pr[0], pr[1]))
        out[cls] = (acc, ms, fs)

LIB = {}
for j in LIBJARS:
    z = zipfile.ZipFile(j)
    for n in z.namelist():
        if n.endswith('.dex'): dex_load(z.read(n), LIB)

def jname(desc):  # Landroid/view/View$X; -> android.view.View$X
    return desc[1:-1].replace('/', '.')

def have_class(n):  # binary name android.view.View$X
    return ('L' + n.replace('.', '/') + ';') in LIB

# ---- android.jar through javap ----
JP = {}
def javap(names):
    names = [n for n in names if n not in JP]
    for i in range(0, len(names), 200):
        chunk = names[i:i + 200]
        r = subprocess.run([JAVAP, '-s', '-constants', '-protected', '-cp', JAR] + chunk,
                           capture_output=True, text=True)
        cur = None
        lines = r.stdout.split('\n')
        k = 0
        while k < len(lines):
            ln = lines[k]
            if cur is None:
                m = re.match(r'^(.*?)\b(class|interface|enum|@interface) ([\w.$]+)(.*)\{$', ln)
                if m:
                    cur = {'mods': m.group(1).split(), 'kind': m.group(2), 'name': m.group(3),
                           'rest': m.group(4).strip(), 'members': []}
                    # javap shows an enum as a class extending Enum
                    if cur['kind'] == 'class' and cur['rest'].startswith('extends java.lang.Enum<'):
                        cur['kind'] = 'enum'
                k += 1; continue
            if ln.startswith('}'):
                JP[cur['name']] = cur; cur = None; k += 1; continue
            s = ln.strip()
            if s and k + 1 < len(lines) and lines[k + 1].strip().startswith('descriptor:'):
                cur['members'].append((s, lines[k + 1].strip()[len('descriptor:'):].strip()))
                k += 2; continue
            k += 1
        for n in chunk:
            JP.setdefault(n, None)
    return

# ---- types ----
PRIM = {'void', 'boolean', 'byte', 'char', 'short', 'int', 'long', 'float', 'double'}
QN = re.compile(r'\b(?:[a-z_][\w]*\.)+[A-Z][\w$]*')

def types_in(text):
    return set(m.group(0) for m in QN.finditer(text))

GEN = set()  # binary names generated this run

def known(t):
    if have_class(t) or t in GEN: return True
    return False

def src_type(t):  # source spelling
    return t.replace('$', '.')

# ---- what to make ----
want_classes = set()
for ln in open(G + 'allclasses.txt'):
    want_classes.add(jname(ln.strip()))
want_members = {}
for ln in open(G + 'allmembers.txt'):
    ln = ln.strip()
    if '->' in ln:
        c, rest = ln.split('->', 1); nm = rest[:rest.index('(')]; desc = rest[len(nm):]
        want_members.setdefault(jname(c), []).append(('m', nm, desc))
    else:
        c, nm = ln.rsplit('.', 1) if False else (ln[:ln.index(';') + 1], ln[ln.index(';') + 2:])
        want_members.setdefault(jname(c), []).append(('f', nm, None))
skip = set(l.strip() for l in open(G + 'skip.txt')) if os.path.exists(G + 'skip.txt') else set()

def android(n): return n.startswith('android.')

javap(sorted(set(n for n in want_classes if android(n)) | set(want_members)))

# classes to make: wanted android ones, their outers, their supertypes, and
# the types the wanted members of present classes name
def header_types(h):
    return types_in(h['rest'])

def need_class(n, acc):
    if n in acc or have_class(n) or not android(n) or ('class:' + n) in skip: return
    javap([n])
    if JP.get(n) is None: return
    acc.add(n)
    if '$' in n: need_class(n.rsplit('$', 1)[0], acc)
    for t in header_types(JP[n]):
        need_class(t.split('<')[0], acc)

make = set()
for n in want_classes: need_class(n, make)
for c, ms in want_members.items():
    h = JP.get(c)
    if not h: continue
    for kind, nm, desc in ms:
        for decl, d in h['members']:
            if kind == 'm' and d == desc and re.search(r'\b' + re.escape(nm) + r'\(', decl):
                for t in types_in(decl): need_class(t, make)
GEN |= make

# ---- declarations ----
def split_params(p):
    out = []; depth = 0; cur = ''
    for ch in p:
        if ch == '<': depth += 1
        if ch == '>': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip(): out.append(cur.strip())
    return out

def default_for(rt, owner):
    rt = rt.strip()
    if rt == 'void': return None
    if rt == 'boolean': return 'false'
    if rt in ('byte', 'char', 'short', 'int', 'long', 'float', 'double'): return '0'
    base = rt.split('<')[0]
    if base == owner and 'Builder' in owner.rsplit('.', 1)[-1]: return 'this'
    # Android hands back an empty list or array, not null
    if rt.endswith('[]') and '<' not in rt:
        return 'new %s[0]%s' % (rt[:-2].split('[')[0], '[]' * (rt.count('[]') - 1))
    empty = {'java.util.List': 'ArrayList', 'java.util.Collection': 'ArrayList',
             'java.util.Set': 'HashSet', 'java.util.Map': 'HashMap'}.get(base)
    if empty: return 'new java.util.%s<>()' % empty
    return 'null'

def member_src(decl, owner_bin, kind_iface, existing):
    # decl: javap line without the descriptor
    d = decl.rstrip(';').strip()
    d = d.replace('$', '.')
    owner_src = src_type(owner_bin)
    simple = owner_bin.rsplit('.', 1)[-1].rsplit('$', 1)[-1]
    if '(' not in d:  # field
        val = None
        if ' = ' in d:
            d, val = d.split(' = ', 1)
        words = d.split()
        fmods = {'public', 'protected', 'private', 'static', 'final', 'transient', 'volatile'}
        k = 0
        while k < len(words) and words[k] in fmods: k += 1
        mods = words[:k]; nm = words[-1]; ty = ' '.join(words[k:-1])
        if not ty: return None
        mods = [x for x in mods if x not in ('transient', 'volatile')]
        if val is None:
            if 'static' in mods and 'final' in mods:
                val = default_for(ty, None) or 'null'
                if val == 'this': val = 'null'
            else:
                if 'final' in mods: mods.remove('final')
        else:
            if ty == 'float' and not val.endswith('f'): val += 'f'
            if ty == 'long' and val.endswith('l'): val = val[:-1] + 'L'
            if ty == 'char' and re.match(r'^\d+$', val): val = '(char)' + val
        if kind_iface:
            mods = [x for x in mods if x not in ('public', 'static', 'final')]
        return ' '.join(mods + [ty, nm]) + (' = ' + val if val is not None else '') + ';'
    # method or constructor
    head, args = d.split('(', 1)
    args, after = args.rsplit(')', 1)
    params = split_params(args)
    names = ['arg%d' % i for i in range(len(params))]
    plist = ', '.join(p + ' ' + n for p, n in zip(params, names))
    throws = after.strip()
    words = head.split()
    is_ctor = words[-1] == owner_src or words[-1].split('.')[-1] == simple and '.' in words[-1]
    mods = [w for w in words[:-1]]
    if is_ctor:
        mods = [w for w in mods if w in ('public', 'protected', 'private')]
        call = existing.get('super_call', '')
        return '%s %s(%s) %s{%s}' % (' '.join(mods), simple, plist, (throws + ' ') if throws else '', call)
    nm = words[-1]
    # return type: everything after the modifiers (and a generic <T> part)
    modset = {'public', 'protected', 'private', 'static', 'final', 'abstract', 'native',
              'synchronized', 'default', 'strictfp'}
    i = 0
    gen = ''
    while i < len(words) - 1 and (words[i] in modset or words[i].startswith('<')):
        if words[i].startswith('<'):
            # a generic list may hold spaces: join to its end
            g = words[i]
            while g.count('<') > g.count('>'):
                i += 1; g += ' ' + words[i]
            gen = g
        i += 1
    rt = ' '.join(words[i:-1])
    mods = [w for w in words[:i] if w in modset]
    abstract = 'abstract' in mods
    mods = [w for w in mods if w not in ('native', 'synchronized', 'abstract', 'default')]
    if kind_iface:
        mods = [w for w in mods if w != 'public']
        if 'static' not in mods and not existing.get('iface_abstract'):
            mods.insert(0, 'default')
        elif 'static' not in mods:
            return '%s %s %s(%s)%s;' % (' '.join(mods), (gen + ' ' + rt).strip(), nm, plist,
                                        (' ' + throws) if throws else '')
    elif abstract and existing.get('keep_abstract'):
        return '%s abstract %s %s(%s)%s;' % (' '.join(mods), (gen + ' ' + rt).strip(), nm, plist,
                                             (' ' + throws) if throws else '')
    dv = default_for(rt, owner_src)
    body = '' if dv is None else ' return %s; ' % dv
    return '%s %s %s(%s)%s {%s}' % (' '.join(mods), (gen + ' ' + rt).strip(), nm, plist,
                                    (' ' + throws) if throws else '', body)

def resolves(text):
    for t in types_in(text.replace('$', '.')):
        b = t
        # source spelling a.b.C.D: find a known binary form
        parts = b.split('.')
        ok = False
        for k in range(len(parts) - 1, 0, -1):
            bn = '.'.join(parts[:k + 1 - (len(parts) - 1 - k) if False else k]) if False else None
        # try every split of the trailing capitalised names into $
        caps = [i for i, p in enumerate(parts) if p[:1].isupper()]
        if not caps:
            continue
        first = caps[0]
        cand = '.'.join(parts[:first + 1]) + ''.join('$' + p for p in parts[first + 1:])
        if known(cand) or cand.startswith('java.') and have_class(cand):
            continue
        if cand.startswith('java.') or cand.startswith('javax.') or cand.startswith('org.'):
            if not have_class(cand): return False
            continue
        if not known(cand): return False
    return True

# ---- sources ----
def rel_of(top_bin):
    return top_bin.replace('.', '/') + '.java'

FILES = {}
def file_text(rel):
    if rel in FILES: return FILES[rel]
    b = BASE + rel
    if os.path.exists(b):
        t = open(b).read()
    else:
        r = subprocess.run(['git', '-C', CK, 'show', 'HEAD:src/api-impl/' + rel],
                           capture_output=True, text=True)
        t = r.stdout if r.returncode == 0 else None
    FILES[rel] = t
    return t

def body_span(text, chain):
    # the { } of the class named by chain (outermost first) in text
    pos = 0; end = len(text)
    for nm in chain:
        m = None
        for mm in re.finditer(r'\b(class|interface|enum|@interface)\s+' + re.escape(nm) + r'\b', text[pos:end]):
            m = mm; break
        if not m: return None
        k = pos + m.end()
        k = text.index('{', k)
        depth = 0; i = k; instr = None
        while i < end:
            ch = text[i]
            if instr:
                if ch == '\\': i += 2; continue
                if ch == instr: instr = None
            elif ch in '"\'': instr = ch
            elif text.startswith('//', i):
                i = text.index('\n', i); continue
            elif text.startswith('/*', i):
                i = text.index('*/', i) + 2; continue
            elif ch == '{': depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0: break
            i += 1
        pos, end = k + 1, i
    return pos, end

INSERTS = {}  # rel -> list of (chain, text)
NOARG = set()  # (rel, chain): a constructor added; the default must stay
ENUMS = {}  # rel -> list of (chain, constant, key): first in the enum's list
NEWFILES = {}  # rel -> text

def kind_of(src):
    # 'interface' or 'class' as ATL (or this run) has it
    base = src.split('<')[0]
    parts = base.split('.')
    caps = [i for i, p in enumerate(parts) if p[:1].isupper()]
    if not caps: return None
    b = '.'.join(parts[:caps[0] + 1]) + ''.join('$' + p for p in parts[caps[0] + 1:])
    if b in GEN and JP.get(b): return 'interface' if JP[b]['kind'] == 'interface' else 'class'
    e = LIB.get('L' + b.replace('.', '/') + ';')
    if e: return 'interface' if e[0] & 0x200 else 'class'
    return None

def class_decl(n):
    h = JP[n]
    simple = n.rsplit('.', 1)[-1].rsplit('$', 1)[-1]
    mods = [m for m in h['mods'] if m in ('public', 'protected', 'static', 'final', 'abstract')]
    if '$' not in n: mods = [m for m in mods if m != 'static']
    kind = h['kind']
    rest = h['rest'].replace('$', '.')
    gen = ''
    if rest.startswith('<'):
        depth = 0
        for i, ch in enumerate(rest):
            if ch == '<': depth += 1
            if ch == '>':
                depth -= 1
                if depth == 0: gen = rest[:i + 1]; rest = rest[i + 1:].strip(); break
    # drop supertypes we cannot name
    ext = ''; imp = []
    m = re.match(r'^(?:extends (.+?))?(?:\s*implements (.+))?$', rest)
    if m:
        if kind == 'interface':
            sups = split_params(m.group(1) or '')
            imp = [s for s in sups if resolves(s) and kind_of(s) == 'interface']
            ext = ''
        else:
            e = (m.group(1) or '').strip()
            if e and e != 'java.lang.Object' and e != 'java.lang.Enum<' and resolves(e) and kind != 'enum' \
               and kind_of(e) == 'class':
                ext = e
            imp = [s for s in split_params(m.group(2) or '') if resolves(s) and kind_of(s) == 'interface']
    if kind == 'enum':
        mods = [x for x in mods if x not in ('final', 'abstract')]
    line = ' '.join(mods + [kind, simple + gen])
    if ext: line += ' extends ' + ext
    if imp: line += (' extends ' if kind == 'interface' else ' implements ') + ', '.join(imp)
    return line, ext

def super_call(ext, use_this=False):
    # a constructor of the superclass ATL has: no-arg, else the first one
    if not ext: return ''
    base = ext.split('<')[0]
    parts = base.split('.')
    caps = [i for i, p in enumerate(parts) if p[:1].isupper()]
    b = '.'.join(parts[:caps[0] + 1]) + ''.join('$' + p for p in parts[caps[0] + 1:])
    if b in GEN and not use_this: return ''
    e = LIB.get('L' + b.replace('.', '/') + ';')
    if not e: return ''
    ctors = [m for m in e[1] if m[0] == '<init>']
    if not ctors or any(m[1] == '()' for m in ctors): return ''
    args = re.findall(r'\[*(?:L[^;]+;|[ZBCSIJFD])', ctors[0][1][1:-1])
    vals = []
    for a in args:
        vals.append('false' if a == 'Z' else '(%s)0' % {'B': 'byte', 'C': 'char', 'S': 'short', 'I': 'int',
                    'J': 'long', 'F': 'float', 'D': 'double'}[a] if a in 'BCSIJFD' and len(a) == 1
                    else '(%s)null' % jname(a).replace('$', '.') if not a.startswith('[') else 'null')
    return ' %s(%s); ' % ('this' if use_this else 'super', ', '.join(vals))

def gen_class(n):
    h = JP[n]
    line, ext = class_decl(n)
    iface = h['kind'] == 'interface'
    out = []
    if h['kind'] == 'enum':
        consts = [re.match(r'.* (\w+);$', d).group(1) for d, desc in h['members']
                  if 'static final' in d and d.rstrip(';').endswith(' ' + d.rstrip(';').split()[-1])
                  and desc == 'L' + n.replace('.', '/') + ';']
        out.append('\t' + ', '.join(consts) + ';')
    elif h['kind'] == '@interface':
        pass
    else:
        sc = super_call(ext)
        has_noarg = False
        for decl, desc in h['members']:
            key = 'member:%s:%s' % (n, decl)
            if key in skip: continue
            if decl.startswith('static {}') or '<clinit>' in decl: continue
            if not resolves(decl): continue
            # ATL's own classes may implement it already: bodies, not abstract
            ctx = {'super_call': sc, 'iface_abstract': False, 'keep_abstract': 'abstract' in h['mods']}
            s = member_src(decl, n, iface, ctx)
            if s is None: continue
            if desc == '()V' and '(' in decl and ' ' + n.split('.')[-1].split('$')[-1] + '(' in decl.replace('$', '.').replace(n.replace('$', '.'), n.split('.')[-1].split('$')[-1]):
                has_noarg = True
            out.append('\t// GEN %s\n\t%s' % (key, s))
        if not iface and not has_noarg:
            simple = n.rsplit('.', 1)[-1].rsplit('$', 1)[-1]
            out.append('\tprotected %s() {%s}' % (simple, sc))
    return '%s {\n%s\n}\n' % (line, '\n\n'.join(out))

# order: outers before inners
for n in sorted(make, key=lambda x: (x.count('$'), x)):
    if JP.get(n) is None: continue
    if 'class:' + n in skip: continue
    src = gen_class(n)
    top = n.split('$')[0]
    rel = rel_of(top)
    if '$' not in n:
        pkg = n.rsplit('.', 1)[0]
        NEWFILES[rel] = 'package %s;\n\n// android.jar\'s shape, made by gen.py\n%s' % (pkg, src)
    else:
        chain = n.split('.')[-1].split('$')
        INSERTS.setdefault(rel, []).append((chain[:-1], '\n\t// GEN class:%s\n\t' % n +
                                             src.replace('\n', '\n\t').rstrip('\t') + '\n'))

# members of classes ATL has
added = 0
for c, ms in sorted(want_members.items()):
    if not android(c) or c in make: continue
    h = JP.get(c)
    if not h or not have_class(c): continue
    acc, have_m, have_f = LIB['L' + c.replace('.', '/') + ';']
    iface = h['kind'] == 'interface'
    chain = c.split('.')[-1].split('$')
    rel = rel_of(c.split('$')[0])
    for kind, nm, desc in ms:
        for decl, d in h['members']:
            if kind == 'm' and nm == '<init>':
                simple = c.split('.')[-1].split('$')[-1]
                if d != desc or not re.search(r'[.$]' + re.escape(simple) + r'\(', decl): continue
                if any(x[0] == '<init>' and x[1] == d[:d.index(')') + 1] for x in have_m): continue
            elif kind == 'm':
                if d != desc or not re.search(r'(^|[\s.])' + re.escape(nm) + r'\(', decl): continue
                if any(x[0] == nm and x[1] == d[:d.index(')') + 1] for x in have_m): continue
            else:
                if '(' in decl or not re.search(r'\b' + re.escape(nm) + r'( =|;$)', decl.rstrip(';') + ';'): continue
                if nm in have_f: continue
            key = 'member:%s:%s' % (c, decl)
            if key in skip or not resolves(decl): break
            ctx = {'super_call': '', 'iface_abstract': False, 'keep_abstract': False}
            if kind == 'm' and nm == '<init>':
                # through a constructor the class has: this(...)
                ctors = sorted(x for x in have_m if x[0] == '<init>')
                if not ctors: break
                if not any(x[1] == '()' for x in ctors):
                    sc = super_call(c.replace('$', '.'), use_this=True)
                    if sc is None: break
                    ctx['super_call'] = sc
            if h['kind'] == 'enum' and kind == 'f' and 'static final' in decl \
               and decl.rstrip(';').split()[-2].replace('$', '.') == c.replace('$', '.'):
                ENUMS.setdefault(rel, []).append((chain, nm, key))
                added += 1
                break
            s = member_src(decl, c, iface, ctx)
            if s:
                INSERTS.setdefault(rel, []).append((chain, '\n\t// GEN %s\n\t%s\n' % (key, s)))
                added += 1
                if kind == 'm' and nm == '<init>':
                    NOARG.add((rel, tuple(chain)))
            break

# ---- write ----
# what the last run wrote goes back to its base first
if os.path.exists(S + 'gen/last.json'):
    for rel in json.load(open(S + 'gen/last.json'))['written']:
        if os.path.exists(BASE + rel): shutil.copy(BASE + rel, OV + rel)
        elif os.path.exists(OV + rel): os.remove(OV + rel)
        # the build copied it over the checkout: put the checkout's back
        r = subprocess.run(['git', '-C', CK, 'show', 'HEAD:src/api-impl/' + rel],
                           capture_output=True, text=True)
        ck = CK + 'src/api-impl/' + rel
        if r.returncode == 0: open(ck, 'w').write(r.stdout)
        elif os.path.exists(ck) and not os.path.exists(OV + rel): os.remove(ck)
written = []
for rel in ENUMS:
    INSERTS.setdefault(rel, [])
for rel, items in INSERTS.items():
    if rel in NEWFILES:
        t = NEWFILES[rel]
    else:
        t = file_text(rel)
        if t is None:
            print('no source for', rel); continue
    for chain, nm, key in ENUMS.get(rel, []):
        sp = body_span(t, chain)
        if not sp:
            print('no enum', chain, 'in', rel); continue
        # after the last constant: the existing ones keep their ordinals
        body = t[sp[0]:sp[1]]
        depth = 0; at = len(body); instr = None; i = 0
        while i < len(body):
            ch = body[i]
            if instr:
                if ch == '\\': i += 2; continue
                if ch == instr: instr = None
            elif ch in '"\'': instr = ch
            elif body.startswith('//', i): i = body.index('\n', i); continue
            elif body.startswith('/*', i): i = body.index('*/', i) + 2; continue
            elif ch in '({': depth += 1
            elif ch in ')}': depth -= 1
            elif ch == ';' and depth == 0: at = i; break
            i += 1
        head = body[:at].rstrip()
        add = ('' if head.endswith(',') or head == '' else ',') + '\n\t// GEN %s\n\t%s' % (key, nm)
        t = t[:sp[0]] + head + add + body[len(head):] + t[sp[1]:]
        sp = None
    for chain, txt in items:
        sp = body_span(t, chain)
        if not sp:
            print('no class', chain, 'in', rel); continue
        t = t[:sp[1]] + txt + t[sp[1]:]
    # a class that had only the implicit constructor keeps it, written out
    for (r, ch) in NOARG:
        if r != rel: continue
        sp = body_span(t, list(ch))
        if not sp: continue
        simple = ch[-1]
        if re.search(r'\b' + re.escape(simple) + r'\s*\(\s*\)\s*(throws[^{]*)?\{', t[sp[0]:sp[1]]): continue
        own = [m for m in re.finditer(r'(public|protected|private)\s+' + re.escape(simple) + r'\s*\(', t[sp[0]:sp[1]])]
        if len(own) > sum(1 for x in items if ('(' in x[1] and ' ' + simple + '(' in x[1])):
            continue
        t = t[:sp[0]] + '\n\t// GEN default constructor, kept\n\tpublic %s() {}\n' % simple + t[sp[0]:]
    if rel in NEWFILES: NEWFILES[rel] = t
    else:
        open(OV + rel, 'w').write(t) if os.path.isdir(os.path.dirname(OV + rel)) else (
            os.makedirs(os.path.dirname(OV + rel)), open(OV + rel, 'w').write(t))
        written.append(rel)
for rel, t in NEWFILES.items():
    os.makedirs(os.path.dirname(OV + rel), exist_ok=True)
    open(OV + rel, 'w').write(t)
    written.append(rel)
# meson: new files listed
mb = open(BASE + 'meson.build').read()
newlist = ''.join("\t'%s',\n" % r for r in sorted(NEWFILES))
mb = mb.replace('srcs = [\n', 'srcs = [\n' + newlist, 1)
open(OV + 'meson.build', 'w').write(mb)
json.dump({'written': written, 'new': sorted(NEWFILES)}, open(S + 'gen/last.json', 'w'))
print('classes made', len(NEWFILES), 'nested', sum(1 for n in make if '$' in n),
      'members added', added, 'files', len(written))
