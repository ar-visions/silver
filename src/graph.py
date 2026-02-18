#!/usr/bin/env python3
import site
import os, re, sys, glob, platform, argparse, subprocess
from pathlib import Path


def git_remote_info(path='.'):
    """Return (domain, owner, repo) from the current Git remote."""

    remote = subprocess.check_output(
        ['git', '-C', Path(path).parent, 'remote', 'get-url', 'origin'],
        text=True
    ).strip()

    # Match SSH (git@github.com:owner/repo) or HTTPS (https://github.com/owner/repo.git)
    m = re.search(
        r'(?:@|//)(?P<domain>[^/:]+)[:/](?P<owner>[^/]+)/(?P<repo>[^/]+?)(?:\.git)?$',
        remote
    )

    if not m:
        raise ValueError(f"could not parse remote URL: {remote}")

    return m.group('domain'), m.group('owner'), m.group('repo')


def get_env_vars():
    """Get required variables from command line arguments"""
    parser = argparse.ArgumentParser(description='Generate header files')
    parser.add_argument('--project-path',   required=True, help='Project root path')
    parser.add_argument('--cache-file',     required=False, help='Cache identifier (if exists, the job has previously run)')
    parser.add_argument('--build-path',     required=True, help='Build output path')
    parser.add_argument('--project-name',   required=True, help='Project name')
    parser.add_argument('--import',         required=True, help='Import path')
    parser.add_argument('--debug',          action='store_true', default=True,  help='debug')
    parser.add_argument('--release',        action='store_true', default=False, help='release')
    parser.add_argument('--asan',           action='store_true', default=False, help='enable address sanitizer')
    parser.add_argument('sdk', nargs='?', default='native',
                        help='target SDK / platform triple (default: native)')

    args = parser.parse_args()

    return {
        'SILVER':       Path(__file__).resolve().parent.parent,
        'PROJECT_PATH': args.project_path,
        'PROJECT_NAME': args.project_name,
        'CACHE_FILE':   args.cache_file,
        'DIRECTIVE':    'src',
        'BUILD_PATH':   args.build_path,
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
        return [], [], [], [], None, []

    domain, owner, _ = git_remote_info(path)

    install_headers = []
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

            elif current_key == "install":
                install_headers.extend(val.split())
            
            elif current_key == "modules":
                deps.extend(val.split())

            elif current_key == "link":
                links.extend(val.split())
        
            elif current_key == "cflags":
                cflags.extend(expand_vars(val).split())

            elif current_key == "import":
                w4 = val.strip()
                parts = w4.split(':') # account:project:
                count = len(parts)
                
                if len(parts) >= 1:
                    iservice = 0 if count <= 2 else 1
                    iname   = iservice+1
                    account = owner if count < 2 else parts[0]
                    service = domain if count < 3 else parts[iservice]
                    sp      = parts[iname].split('/')
                    name    = sp[0]
                    alias   = name
                    commit  = sp[1] if len(sp) > 1 else None
                    commit_sp = commit.split() if commit else None
                    extra   = None
                    if commit and len(commit_sp) > 1:
                        commit = commit_sp[0]
                        if commit_sp[1] == 'as':
                            alias = commit_sp[2]
                            if len(commit_sp) > 3:
                                extra = commit_sp[3]
                        else:
                            extra = commit_sp[1]
                        
                    
                    uri     = f'https://{service}/{account}/{name}'
                    configs = []

                    #print (f'alias = {alias}, account = {account}, service = {service}, name = {name}, commit = {commit}, extra = {extra}')

                    # collect indented block lines
                    j = i + 1
                    while j < len(lines) and (lines[j].startswith(" ") or lines[j].startswith("\t")):
                        cfg_line = lines[j].strip()
                        if cfg_line:
                            configs.append(cfg_line)
                        j += 1
                    imports.append((alias, uri, commit, configs, extra))
                    i = j - 1
                else:
                    print(f"warning: invalid import line: {raw}")

        i += 1

    return install_headers, deps, links, cflags, target, imports
