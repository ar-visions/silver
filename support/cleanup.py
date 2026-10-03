#!/usr/bin/env python3
# public cleanup for .ag sources: numbered names out (Rule #3b)
#   cleanup.py names <file.ag> [--apply]
# per function: a numbered local or parameter (wx9) becomes its
# name without the digits (wx) when that is safe; the rest are
# listed for a person to name
import os, re, sys

KEYWORDS = set('''
a if el else for while switch case default return break continue
func cast operator construct getter setter lambda class struct
enum alias scalar import export expect public intern mutable
static persist manual context override post in reverse is to
true false null new local ref asm try catch finally throw fault
no-op ifdef ifndef and or not with using sizeof typeid typeof
super self extend element app any object none void bool
'''.split())

IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
NUMBERED = re.compile(r'^[A-Za-z_][A-Za-z_0-9]*[0-9][A-Za-z_0-9]*$')
HEADER = re.compile(
    r'^(\s*)((public|intern|export|expect|static|override|post)\s+)*'
    r'(func|cast|operator|construct|getter|setter)\b')
# a declaration: `name :` at a statement start, in an arg list,
# or a for/lambda binding
DECL = [
    re.compile(r'^\s*([A-Za-z_][A-Za-z_0-9]*)\s*:(?!:)'),
    re.compile(r'(?:\[|,|::)\s*([A-Za-z_][A-Za-z_0-9]*)\s*:(?!:)'),
]


def indent(line):
    return len(line) - len(line.lstrip(' \t').expandtabs(4)) \
        if False else len(line.expandtabs(4)) - \
        len(line.expandtabs(4).lstrip(' '))


def code_part(line):
    # the line without its comment (a # outside quotes)
    q = None
    for k, c in enumerate(line):
        if q:
            if c == q:
                q = None
        elif c in '\'"':
            q = c
        elif c == '#':
            return line[:k]
    return line


def blocks(lines):
    # (start, end) of each function: header, then deeper lines
    out = []
    k = 0
    while k < len(lines):
        m = HEADER.match(lines[k])
        if not m:
            k += 1
            continue
        ind = indent(lines[k])
        e = k + 1
        while e < len(lines):
            s = lines[e]
            # a comment, even at column 0, does not end the function
            if s.strip() and not s.strip().startswith('#') \
                    and indent(s) <= ind:
                break
            e += 1
        out.append((k, e))
        k = e
    return out


def decls(body):
    # numbered locals and parameters declared in a function body;
    # a key in a Type [ ... ] constructor is not a declaration
    found = []
    carry, header_open = 0, False
    for n, line in enumerate(body):
        code = code_part(line)
        in_header = n == 0 or (carry > 0 and header_open)
        m = DECL[0].match(code)
        if m and (carry == 0 or in_header):
            nm = m.group(1)
            if NUMBERED.match(nm) and nm not in found:
                found.append(nm)
        for m in DECL[1].finditer(code):
            k = m.start()
            opener = code[:k + 1].rstrip()
            word = re.findall(r'([A-Za-z_]\w*)\s*\[?$', opener)
            word = word[0] if word else ''
            if not (in_header or word in ('for', 'lambda')
                    or opener.endswith('::')):
                continue
            nm = m.group(1)
            if NUMBERED.match(nm) and nm not in found:
                found.append(nm)
        depth = brackets_at(code + ' ', carry)[-1]
        if carry == 0 and depth > 0:
            header_open = n == 0
        carry = max(depth, 0)
    return found


def asm_rows(body):
    # indexes of the lines inside asm bodies
    rows, at = set(), None
    for k, line in enumerate(body):
        if at is not None:
            if line.strip() and len(lead(line).expandtabs(4)) <= at:
                at = None
            else:
                rows.add(k)
                continue
        if ASM.search(code_part(line)):
            at = len(lead(line).expandtabs(4))
    return rows


ACCESS = r'(?:(?:public|intern|mutable|context|manual|static|persist|expect|override)\s+)*'
MEMBER = re.compile(r'^\s+' + ACCESS + r'(?:\[[^\]]*\]\s*)?([A-Za-z_]\w*)\s*:(?!:)')
TYPE_HEAD = re.compile(r'^(class|struct|element|enum|app|[A-Z]\w*\s+[A-Za-z_]\w*)\b')


def member_names():
    # every class member name in the repo's .ag sources, once
    if hasattr(member_names, 'cache'):
        return member_names.cache
    import glob
    names = set()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for f in glob.glob(os.path.join(root, '*', '*.ag')):
        in_type, step = False, None
        for line in open(f, errors='ignore'):
            if line.strip() and not line[0].isspace():
                in_type = bool(TYPE_HEAD.match(line)) \
                    and not line.startswith(('func', 'expect', 'export'))
                step = None
                continue
            if not in_type or not line.strip():
                continue
            ind = len(lead(line).expandtabs(4))
            if step is None:
                step = ind
            if ind != step:
                continue
            m = MEMBER.match(line)
            if m:
                names.add(m.group(1))
    member_names.cache = names
    return names


def plan(lines, a, b):
    body = lines[a:b]
    names = decls(body)
    if not names:
        return {}, []
    asm = asm_rows(body)
    in_asm = set()
    for k in asm:
        in_asm.update(IDENT.findall(body[k]))
    used = set()
    for line in body:
        for t in IDENT.findall(line):
            if t not in names:
                used.add(t)
    stripped = {}
    for nm in names:
        stripped.setdefault(nm.rstrip('0123456789'), []).append(nm)
    ren, manual = {}, []
    taken = set()
    for nm in names:
        base = nm.rstrip('0123456789')
        ok = (nm not in in_asm and base not in in_asm
              and base not in member_names()
              and base and not re.search('[0-9]', base)
              and base not in KEYWORDS and base not in used
              and len(stripped[base]) == 1 and base not in taken
              and (len(base) > 1 or base in 'ixy'))
        if ok:
            ren[nm] = base
            taken.add(base)
        else:
            manual.append(nm)
    return ren, manual


def rename_code(line, ren):
    # renames in code and in {...} of strings; never in comments
    out, k, q, brace = [], 0, None, 0
    word = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
    while k < len(line):
        c = line[k]
        live = q is None or (q == "'" and brace > 0)
        if q is None and c == '#':
            out.append(line[k:])
            break
        if live and (c.isalpha() or c == '_'):
            m = word.match(line, k)
            w = m.group(0)
            prev = line[k - 1] if k else ''
            if prev != '.' and not line[max(0, k - 2):k] == '->':
                w = ren.get(w, w)
            out.append(w)
            k = m.end()
            continue
        if q is None and c in '\'"':
            q = c
        elif q and brace == 0 and c == q:
            q = None
        elif q == "'" and c == '{':
            brace += 1
        elif q == "'" and c == '}' and brace:
            brace -= 1
        out.append(c)
        k += 1
    return ''.join(out)


def apply(lines, a, b, ren):
    asm = asm_rows(lines[a:b])
    for k in range(a, b):
        if k - a not in asm:
            lines[k] = rename_code(lines[k], ren)


WIDTH = 72
ONE_LINE_EL = re.compile(r'^(\s*)el\s+(?!\[)(\S.*)$')
IMPORT = re.compile(r'^\s*import\b')
ASM = re.compile(r'(^\s*|\s)asm(\s|$)')
PRIMS = r'(i8|i16|i32|i64|u8|u16|u32|u64|f32|f64|bool|num|sz)'
# a cast with no brackets reads to the end of its line
BARE_CAST = re.compile(
    r'\b(%s|string|path|symbol|cstr)\s+(?!\[)[A-Za-z_(@\'"]'
    r'|\b[A-Za-z_]\w*\s+\'' % PRIMS[1:-1])
ONE_LINE_IF = re.compile(r'^(\s*)((?:if|el|for|while)\s*)\[')


def one_line_if(line):
    # (indent, 'if [ c ]', body) when a body follows on the line
    m = ONE_LINE_IF.match(line)
    if not m:
        return None
    depth = brackets_at(line)
    k = m.end() - 1
    for e in range(k + 1, len(line)):
        if line[e] == ']' and depth[e] == 1:
            body = line[e + 1:].strip()
            if not body:
                return None
            return m.group(1), line[len(m.group(1)):e + 1], body
    return None


def width(s):
    return len(s.expandtabs(4))


def lead(s):
    return s[:len(s) - len(s.lstrip(' \t'))]


def split_comment(line):
    # (code, comment) with the comment's # kept
    code = code_part(line)
    return code.rstrip(), line[len(code):].strip()


def flow(prefix, words, cont=None):
    # words onto lines of at most WIDTH, each led by prefix
    out, cur = [], prefix
    cont = cont if cont is not None else prefix
    for w in words:
        if cur.strip() and width(cur + ' ' + w) > WIDTH:
            out.append(cur)
            cur = cont + w
        else:
            cur = (cur + ' ' + w) if cur.strip() else cur + w
    out.append(cur)
    return out


def brackets_at(code, start=0):
    # depth inside [ ] before each character, quotes skipped
    depth, q, out = start, None, []
    for c in code:
        out.append(depth if not q else -1)
        if q:
            if c == q:
                q = None
        elif c in '\'"':
            q = c
        elif c == '[':
            depth += 1
        elif c == ']':
            depth -= 1
    return out


# where a line may break, best first: (text, rank, before)
BREAKS = [(', ', 0, False), (' && ', 1, True), (' || ', 1, True),
          (' ? ', 2, True), (' : ', 2, True), (' + ', 3, True),
          (' - ', 3, True), (' * ', 4, True), (' / ', 4, True)]


def break_code(line, carry=0):
    # break inside the outermost [ ] that has a place that fits;
    # carry: brackets left open by the lines above
    if BARE_CAST.search(line) or lead(line).startswith('\t'):
        return None
    depth = brackets_at(line, carry)
    paren = parens_at(line)
    # a function's one-line body after -> stays on its line
    stop = len(line)
    if carry == 0 and HEADER.match(line) and '->' in line:
        stop = line.index('->')
    cuts = []
    for k in range(len(line)):
        if depth[k] < 0 or paren[k] != 0 or width(line[:k]) > WIDTH \
                or k >= stop:
            continue
        # right after an opening [: the rest a level deeper
        if line[k] == '[' and line[k + 1:k + 2] == ' ' \
                and stop == len(line) \
                and line[k + 2:].strip() and k > len(lead(line)):
            cuts.append((depth[k] + 1, 0, k + 1, k + 2, True))
        if depth[k] < 1:
            continue
        for text, rank, before in BREAKS:
            if not line.startswith(text, k):
                continue
            # a : is a ternary's only after a ? at its depth
            if text == ' : ':
                q = line.rfind(' ? ', 0, k)
                if q < 0 or depth[q] != depth[k]:
                    continue
            if before:
                cuts.append((depth[k], rank, k, k + 1, False))
            else:
                cuts.append((depth[k], rank, k + 1, k + 2, False))
    if not cuts:
        return statement_break(line) if carry == 0 else None
    best = min((c[0], c[1]) for c in cuts)
    d, rank, a, b, opened = [c for c in cuts
                             if (c[0], c[1]) == best][-1]
    # continuation lines up after that [
    open_at = None
    for k in range(a - 1, -1, -1):
        if line[k] == '[' and depth[k] == d - 1:
            open_at = k
            break
    if opened:
        pad = lead(line) + '    '
    elif open_at is None:
        # opened above: the line's own indent carries on
        pad = lead(line)
    else:
        pad = ' ' * (width(line[:open_at + 1]) + 1)
    head, tail = line[:a].rstrip(), pad + line[b:].lstrip()
    if not head.strip() or width(tail) >= width(line):
        return None
    if width(tail) > WIDTH:
        more = break_code(tail, depth[a])
        if more:
            return [head] + more
    return [head, tail]


def parens_at(code):
    # depth inside ( ) before each character, quotes skipped
    depth, q, out = 0, None, []
    for c in code:
        out.append(depth if not q else -1)
        if q:
            if c == q:
                q = None
        elif c in '\'"':
            q = c
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
    return out


def statement_break(line):
    # outside brackets: after && or ||, the rest indented deeper
    depth = brackets_at(line)
    paren = parens_at(line)
    cut = None
    for op in (' && ', ' || '):
        k = line.find(op)
        while k >= 0:
            if depth[k] == 0 and paren[k] == 0 \
                    and width(line[:k + 3]) <= WIDTH:
                cut = max(cut or 0, k + 3)
            k = line.find(op, k + 1)
    if not cut:
        return None
    pad = lead(line) + '    '
    head, tail = line[:cut].rstrip(), pad + line[cut:].lstrip()
    if width(tail) > WIDTH:
        more = break_code(tail, 0)
        if more:
            return [head] + more
    return [head, tail]


def comment_text(line):
    # the words of a comment-only line, else None
    s = line.strip()
    if not s.startswith('#') or s.startswith('#!'):
        return None
    return s[1:].strip()


def continues(text):
    # a comment line that carries on the sentence above it
    return bool(text) and text[0].islower() and not text.startswith('-')


def wrap(lines):
    out = []
    brace = 0
    pending = list(lines)
    carry = 0
    stmt_lead = ''
    asm_at = None
    while pending:
        line = pending.pop(0)
        # an asm body is one instruction a line: never touched
        if asm_at is not None:
            if line.strip() and width(lead(line)) <= asm_at:
                asm_at = None
            else:
                out.append(line)
                continue
        # an import's settings are build and shell lines: kept
        if ASM.search(code_part(line)) or IMPORT.match(line):
            asm_at = width(lead(line))
            out.append(line)
            continue
        opened = brace
        held = carry
        if carry == 0:
            stmt_lead = lead(line)
        if brace == 0:
            dep = brackets_at(code_part(line) + ' ', carry)
            carry = max(dep[-1], 0)
        q = None
        for c in code_part(line):
            if q:
                if c == q:
                    q = None
            elif c in '\'"':
                q = c
            elif c == '{':
                brace += 1
            elif c == '}':
                brace -= 1
        if width(line) <= WIDTH or opened > 0 or brace > 0:
            out.append(line)
            continue
        ind = lead(line)
        code, comment = split_comment(line)
        if not code.strip():
            words = comment.lstrip('#').split()
            # the sentence carries on below: flow it in
            while pending and lead(pending[0]) == ind:
                nxt = comment_text(pending[0])
                if nxt is None or not continues(nxt):
                    break
                words += nxt.split()
                pending.pop(0)
            out.extend(flow(ind + '#', words, ind + '# '))
            continue
        if comment:
            out.extend(flow(ind + '#', comment.lstrip('#').split(),
                            ind + '# '))
            line = code
            if width(line) <= WIDTH:
                out.append(line)
                continue
        m = one_line_if(line)
        bare = ONE_LINE_EL.match(line)
        if not m and bare:
            m = (bare.group(1), 'el', bare.group(2))
        if m:
            step = '\t' if ind.startswith('\t') else '    '
            out.append(m[0] + m[1])
            line = m[0] + step + m[2]
            if width(line) <= WIDTH:
                out.append(line)
                continue
        # a continuation lines up with the line above it
        if held > 0 and out and not line.lstrip().startswith(']'):
            above = lead(out[-1])
            if len(above) <= len(stmt_lead):
                above = stmt_lead + '    '
            if len(above) < len(lead(line)) and not above.startswith('\t'):
                line = above + line.lstrip()
                if width(line) <= WIDTH:
                    out.append(line)
                    continue
        parts = break_code(line, held)
        out.extend(parts if parts else [line])
    return out


def main():
    mode, path = sys.argv[1], sys.argv[2]
    write = '--apply' in sys.argv
    lines = open(path).read().split('\n')
    if mode == 'wrap':
        before = sum(1 for s in lines if width(s) > WIDTH)
        lines = wrap(lines)
        after = sum(1 for s in lines if width(s) > WIDTH)
        if write:
            open(path, 'w').write('\n'.join(lines))
        print('%s: %d over %d -> %d' % (path, before, WIDTH, after))
        return
    auto = manual = 0
    for a, b in blocks(lines):
        ren, man = plan(lines, a, b)
        head = lines[a].strip()[:60]
        for nm in man:
            print('%s:%d manual %s  (%s)' % (path, a + 1, nm, head))
        auto += len(ren)
        manual += len(man)
        if write and ren:
            apply(lines, a, b, ren)
    if write:
        open(path, 'w').write('\n'.join(lines))
    print('%s: %d renamed, %d to name by hand' % (path, auto, manual))


main()
