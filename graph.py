#!/usr/bin/env python3
import site
import os, re, sys, glob, platform, argparse, subprocess
from pathlib import Path

def get_env_vars():
    """Get required variables from command line arguments"""
    parser = argparse.ArgumentParser(description='Generate header files')
    parser.add_argument('--project-path',   required=True, help='Project root path')
    parser.add_argument('--build-path',     required=True, help='Build output path')
    parser.add_argument('--project',        required=True, help='Project name')
    parser.add_argument('--import',         required=True, help='Import path')
    parser.add_argument('--debug',          action='store_true', default=True,  help='debug')
    parser.add_argument('--release',        action='store_true', default=False, help='release')
    parser.add_argument('--asan',           action='store_true', default=False, help='enable address sanitizer')
    parser.add_argument('sdk', nargs='?', default='native',
                        help='target SDK / platform triple (default: native)')

    args = parser.parse_args()
    
    return {
        'SILVER':       Path(__file__).resolve().parent,
        'PROJECT_PATH': args.project_path,
        'DIRECTIVE':    'src',
        'BUILD_PATH':   args.build_path,
        'PROJECT':      args.project,
        'SDK':          args.sdk,
        'DEBUG':        args.debug,
        'ASAN':         args.asan,
        'IMPORT':       getattr(args, 'import').replace('\\', '/') # 'import' is a Python keyword
    }


def expand_vars(s):
    """expand $(...) and $VARS using the environment"""
    # shell commands
    s = re.sub(
        r"\$\(([^)]+)\)",
        lambda m: subprocess.getoutput(m.group(1)).strip(),
        s,
    )
    # env vars
    s = re.sub(
        r"\$([A-Za-z_][A-Za-z0-9_]*)",
        lambda m: os.getenv(m.group(1), m.group(0)),
        s,
    )
    return s

def parse_g_file(path):
    """parse .g file for deps, link flags, target type, and imports"""
    if not os.path.exists(path):
        return [], [], [], None, []

    lines = open(path).read().splitlines()
    deps, links, cflags, target, imports = [], [], [], None, []
    current_key = None
    i = 0

    while i < len(lines):
        raw = lines[i]
        line = raw.strip()
        if not line or line.startswith("#"):
            i += 1
            continue

        if ":" in line:
            key, val = [x.strip() for x in line.split(":", 1)]
            current_key = key.lower()

            if current_key == "type":
                target = val

            elif current_key == "modules":
                deps.extend(val.split())

            elif current_key == "link":
                links.extend(val.split())
        
            elif current_key == "cflags":
                cflags.extend(expand_vars(val).split())

            elif current_key == "import":
                uri_commit = val.strip()
                parts = uri_commit.split()
                if len(parts) >= 1:
                    name = parts[0]
                    uri = parts[1]
                    commit = parts[2] if len(parts) > 2 else None
                    configs = []
                    extra = None if len(parts) < 4 else parts[3]
                    
                    # collect indented block lines
                    j = i + 1
                    while j < len(lines) and (lines[j].startswith(" ") or lines[j].startswith("\t")):
                        cfg_line = lines[j].strip()
                        if cfg_line:
                            configs.append(cfg_line)
                        j += 1
                    imports.append((name, uri, commit, configs, extra))
                    i = j - 1
                else:
                    print(f"warning: invalid import line: {raw}")

        i += 1

    return deps, links, cflags, target, imports
