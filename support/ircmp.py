#!/usr/bin/env python3
# two builds' .ll compared function by function, names aside
#   ircmp.py <dir-before> <dir-after>
# local value names (%wx9) are numbered by first use, debug
# metadata is dropped: a rename-only change compares equal
import re, sys, glob, os

DEFINE = re.compile(r'^define .*?@("[^"]+"|[\w.$]+)\(')
LOCAL = re.compile(r'%("[^"]+"|[\w.$-]+)')
NOISE = re.compile(r', !dbg ![0-9]+|, !srcloc ![0-9]+|#dbg_\w+\(.*')


CONST = re.compile(r'^@((?:new_)?const_\w+) = (.*)$')
CONST_REF = re.compile(r'@((?:new_)?const_\w+)')
SRC_LINE = re.compile(r'(\.ag\\00">, i32 )\d+')
NOLINES = '--nolines' in sys.argv
NAME_CONST = re.compile(r'<private constant \[\d+ x i8\] c"[A-Za-z_]\w*\\00">')


def functions(folder):
    out = {}
    for f in sorted(glob.glob(os.path.join(folder, '*.ll'))):
        text = open(f, errors='ignore').read().split('\n')
        # a constant reads as its contents: numbering follows order
        consts = {}
        for line in text:
            m = CONST.match(line)
            if m:
                consts[m.group(1)] = m.group(2)
        name, body = None, []
        for line in text:
            line = CONST_REF.sub(
                lambda m: '<' + consts.get(m.group(1), m.group(1)) + '>',
                line) + '\n'
            if name is None:
                m = DEFINE.match(line)
                if m:
                    name, body = m.group(1), [line]
                continue
            body.append(line)
            if line.startswith('}'):
                out[name] = normal(body)
                name = None
    return out


def normal(body):
    seen = {}

    def local(m):
        k = m.group(1)
        if k not in seen:
            seen[k] = 'v%d' % len(seen)
        return '%' + seen[k]
    lines = []
    for line in body:
        line = NOISE.sub('', line.rstrip())
        if not line.strip():
            continue
        # a block label reads as a local; a bare-word constant is
        # a variable's name (alloc sites record it)
        lab = re.match(r'^([\w.$-]+):(.*)$', line)
        if lab:
            line = '%' + lab.group(1) + ':' + lab.group(2)
        line = NAME_CONST.sub('<name>', line)
        if NOLINES:
            line = SRC_LINE.sub(r'\1L', line)
        lines.append(LOCAL.sub(local, line))
    return '\n'.join(lines)


def main():
    a, b = functions(sys.argv[1]), functions(sys.argv[2])
    if len(sys.argv) > 4 and sys.argv[3] == '--show':
        import difflib
        k = sys.argv[4]
        for line in difflib.unified_diff(a[k].split('\n'),
                                         b[k].split('\n'),
                                         lineterm='', n=0):
            print(line)
        return
    same = [k for k in a if k in b and a[k] == b[k]]
    diff = [k for k in a if k in b and a[k] != b[k]]
    gone = [k for k in a if k not in b]
    new = [k for k in b if k not in a]
    print('%d same, %d differ, %d only before, %d only after'
          % (len(same), len(diff), len(gone), len(new)))
    for k in diff[:40]:
        print('differ:', k)
    for k in gone[:20]:
        print('only before:', k)
    for k in new[:20]:
        print('only after:', k)


main()
