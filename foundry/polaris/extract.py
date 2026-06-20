#!/usr/bin/env python3
# polaris stage 1 — corpus extraction.
#
# walk a diverse set of cloned C projects and split each .c/.h into
# FUNCTION-LEVEL regions (the unit you navigate to: 'the area where ...'),
# recording file + line span + source identity (G) + remote flag + the code
# text. deterministic, no LLM. output is regions.jsonl, the corpus that the
# annotation stage (stage 2) turns into training pairs.
#
# heuristic C splitter (stdlib only): find a top-level line that looks like a
# function header (`name(...)` not ending in ';', not a control keyword) whose
# body opens a brace, then brace-match to the close. imperfect but fine to start.

import os, re, json
from pathlib import Path

ROOT     = Path("/Users/kalen/src/silver")
CHECKOUT = ROOT / "checkout"
OUT      = ROOT / "foundry" / "polaris" / "corpus" / "regions.jsonl"

# project dir -> is this an external/vendored (remote) source? (all true here)
PROJECTS = {
    "Python-3.11.9": True,
    "mbedtls":       True,
    "freetype":      True,
    "glfw":          True,
}

EXTS     = (".c", ".h")
MAX_LINES = 200          # skip/clip pathologically large regions
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "typedef",
            "struct", "enum", "union", "do", "else", "case", "static_assert"}

# a plausible function header: ... name ( ... , and NOT terminated by ';'
HEADER = re.compile(r'^[A-Za-z_][A-Za-z0-9_\s\*\(\)]*?\b([A-Za-z_]\w*)\s*\(')

def looks_like_header(line):
    s = line.rstrip()
    if not s or s.lstrip() != s:        # must start at column 0 (top-level def)
        return None
    if s.endswith(";") or s.endswith(","):
        return None
    if s.startswith(("#", "//", "/*", "*", "}", "{")):
        return None
    m = HEADER.match(s)
    if not m:
        return None
    if m.group(1) in KEYWORDS:
        return None
    return m.group(1)

def extract_file(path, source, remote, rel):
    lines = path.read_text(errors="ignore").split("\n")
    out, i, n = [], 0, len(lines)
    while i < n:
        name = looks_like_header(lines[i])
        if name is None:
            i += 1
            continue
        # find the opening brace within a few lines of the header
        j, brace_at = i, -1
        while j < min(i + 6, n):
            if "{" in lines[j]:
                brace_at = j
                break
            if ";" in lines[j]:            # a prototype, not a definition
                break
            j += 1
        if brace_at < 0:
            i += 1
            continue
        # brace-match to the end of the body
        depth, k = 0, brace_at
        while k < n:
            depth += lines[k].count("{") - lines[k].count("}")
            if depth <= 0 and k >= brace_at:
                break
            k += 1
        start, end = i, k
        if end - start + 1 <= MAX_LINES:
            out.append({
                "source": source,         # the G identity (project)
                "remote": remote,
                "path":   rel,            # path relative to silver root
                "name":   name,
                "start":  start + 1,      # 1-based, matches editor
                "end":    end + 1,
                "code":   "\n".join(lines[start:end + 1]),
            })
        i = end + 1
    return out

def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    total, per = 0, {}
    with OUT.open("w") as f:
        for proj, remote in PROJECTS.items():
            base = CHECKOUT / proj
            count = 0
            for dirpath, _, files in os.walk(base):
                for fn in files:
                    if not fn.endswith(EXTS):
                        continue
                    p = Path(dirpath) / fn
                    rel = str(p.relative_to(ROOT))
                    try:
                        regs = extract_file(p, proj, remote, rel)
                    except Exception:
                        continue
                    for r in regs:
                        f.write(json.dumps(r) + "\n")
                    count += len(regs)
            per[proj] = count
            total += count
    print(f"wrote {OUT.relative_to(ROOT)}")
    for proj, c in per.items():
        print(f"  {c:>7}  {proj}")
    print(f"  {total:>7}  TOTAL regions")

if __name__ == "__main__":
    main()
