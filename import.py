#!/usr/bin/env python3
"""ninja build generator for silver project"""

import os, sys, glob, platform, argparse, subprocess, shlex
from pathlib import Path
from graph import parse_g_file, get_env_vars

def run(cmd, cwd=None):
    print(">", cmd)
    subprocess.run(cmd, cwd=cwd, shell=True, check=True)

def parse_from_config(all):
    res = []
    pre = []
    post = []
    for c in all:
        if c.startswith('>>'):
            pre.append(c[2:].strip())
        elif c.startswith('>'):
            post.append(c[1:].strip())
        else:
            res.append(c.strip())
    return res, pre, post

def build_import(name, uri, commit, _config_lines, install_dir):
    checkout_dir = Path("checkout") / name
    build_dir    = Path("build")    / name / 'release'
    
    config_lines, pre, post = parse_from_config(_config_lines) # pre has > and post as >> infront ... everything else is a config_line
    
    token_file = build_dir / 'silver-token'
    # continue if silver-token exists
    if token_file.exists():
        return
    
    # checkout_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    # fetch or clone
    if checkout_dir.exists():
        print('cwd =', os.getcwd())
        run(f"git -C {checkout_dir} fetch origin")
        run(f"git -C {checkout_dir} checkout {commit}")
        run(f"git -C {checkout_dir} pull origin {commit} || true")
    else:
        run(f"git clone {uri} {checkout_dir}")
        run(f"git -C {checkout_dir} checkout {commit}")

    print(f'the number of pre-config commands is {len(pre)}')
    for cmd in pre:
        print(f'running {cmd}')
        run(cmd, cwd=checkout_dir)

    # os-specific
    sysname = platform.system().lower()
    if sysname == "darwin":
        sysroot = subprocess.getoutput("xcrun --sdk macosx --show-sdk-path || echo ''")
        config_lines += [
            f"-DCMAKE_OSX_SYSROOT={sysroot}",
            "-DCMAKE_C_COMPILER_TARGET=arm64-apple-darwin",
        ]
    elif sysname == "linux":
        config_lines += ["-DCMAKE_C_COMPILER_TARGET=x86_64-unknown-linux-gnu"]

    # generate type / install dir
    config_lines += [
        "-G Ninja",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
    ]

    cmake_args = " ".join(config_lines)
    run(f"cmake -S $IMPORT/{checkout_dir} {cmake_args}", cwd=build_dir)
    
    # build & install
    cpu_count = os.cpu_count() or 4
    run(f"ninja -j{4}", cwd=build_dir)
    run("ninja install", cwd=build_dir)

    # run post commands
    for cmd in post:
        run(cmd, cwd=build_dir)

    token_file.touch()

def import_src(root_dir="src"):
    IMPORT = os.environ.get('IMPORT')
    for path in Path(root_dir).rglob("*.g"):
        _, _, _, _, imports = parse_g_file(str(path))
        for i in imports:
            print(f'{path.stem:<10} import {i[0]}')
            build_import(i[0], i[1], i[2], i[3], IMPORT)

import_src()