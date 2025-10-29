#!/usr/bin/env python3
"""import software from graph files (.g) in src"""

import site, pprint
import os, sys, glob, platform, argparse, subprocess, re
from pathlib import Path
from graph import parse_g_file, get_env_vars

v = get_env_vars()
root   = v['PROJECT_PATH']
IMPORT = v['IMPORT']
SDK    = v['SDK']
os.environ['IMPORT'] = IMPORT
os.environ['SDK']    = SDK

def run(cmd, cwd=None, check=True):
    print(">", cmd)
    subprocess.run(cmd, cwd=cwd, shell=True, check=check)

def parse_from_config(all):
    res  = []
    pre  = []
    post = []
    env  = {}
    env_pattern = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)=(.*)$")
    for c in all:
        if c.startswith('>>'):
            pre.append(c[2:].strip())
        elif c.startswith('>'):
            post.append(c[1:].strip())
        else:
            m = env_pattern.match(c)
            if m:
                key, val = m.groups()
                env[key] = val.strip()
            else:
                res.append(c.strip())
    return res, pre, post, env

def eval_braces(s, context=None):
    """Replace {expr} in a string with eval(expr) using given context dict."""
    if context is None:
        context = {}
    pattern = re.compile(r"\{([^{}]+)\}")
    win = sys.platform.startswith("win")
    context.update({
        "win": win,
        "lin": sys.platform.startswith("linux"),
        "mac": sys.platform == "darwin",
        "default_generator": 'Visual Studio 17 2022' if win else 'Ninja'
    })
    def repl(match):
        expr = match.group(1).strip()
        try:
            return str(eval(expr, {}, context))
        except Exception as e:
            return f"<err:{e}>"

    return pattern.sub(repl, s)

def vreplace(s: str) -> str:
    """Expand $VARS using the current environment."""
    if os.name == "nt":
        return eval_braces(re.sub(
            r"\$([A-Za-z_][A-Za-z0-9_]*)",
            lambda m: os.environ.get(m.group(1), m.group(0)),
            s
        ))
    return eval_braces(s)

def build_import(name, uri, commit, _config_lines, install_dir):
    global IMPORT
    global root
    checkout_dir = Path(root)   / Path('checkout') / name
    build_dir    = Path(IMPORT) / Path('release')  / name
    
    config_lines, pre, post, env = parse_from_config(_config_lines) # pre has > and post as >> infront ... everything else is a config_line
    
    token_file = build_dir / 'silver-token'
    # continue if silver-token exists
    if token_file.exists():
        return
    
    # checkout_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    # fetch or clone
    if checkout_dir.exists():
        run(f"git -C {checkout_dir} fetch origin")
        run(f"git -C {checkout_dir} checkout {commit}")
        run(f"git -C {checkout_dir} pull origin {commit}", check=False)
    else:
        run(f"git clone {uri} {checkout_dir}")
        run(f"git -C {checkout_dir} checkout {commit}")

    for key in env:
        env[key] = vreplace(env[key])
    
    for key in env:
        os.environ[key] = env[key]

    print(f'the number of pre-config commands is {len(pre)} ... map = {env}')
    
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
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
    ]

    cmake_args = " ".join(config_lines)

    s = '/'
    win = sys.platform.startswith('win')
    ext = '.exe' if win else ''

    # generally, software producers throw in MSVC-style command-line switches for cl compiler, and generally do NOT test against clang-cl
    # to this end, we need to go with the compiler its designed for first
    # on linux/mac though, we can attempt our own clang as its preferrable to go with our version we built
    if not '-DCMAKE_C_COMPILER=' in cmake_args:
        cc         = 'cl' if win else 'clang'
        cpp        = 'cl' if win else 'clang++'
        idir       = ''   if win else install_dir + s + 'bin' + s
        clang      = f'{idir}{cc}{ext}'
        clangpp    = f'{idir}{cpp}{ext}'
        cmake_args = f'-DCMAKE_C_COMPILER="{clang}" -DCMAKE_CXX_COMPILER="{clangpp}" ' + cmake_args
    
    if not '-G ' in cmake_args:
        config_lines = '-G Ninja ' + cmake_args

    if not '-S ' in cmake_args:
        cmake_args = f'-S {checkout_dir} ' + cmake_args

    if os.name == "nt":
        cmake_args = cmake_args.replace("gcc", "cl")
        cmake_args = cmake_args.replace("g++", "cl")

    r_cmake_args = vreplace(cmake_args)
    is_ninja = '-G Ninja' in r_cmake_args

    # replace environment vars in cmake_args that begin with $something with %something% on windows
    run(f"cmake {r_cmake_args}", cwd=build_dir)
    
    # build & install
    cpu_count = os.cpu_count() or 4
    if is_ninja:
        run(f"ninja -j{max(1, cpu_count//4)}", cwd=build_dir)
        run("ninja install", cwd=build_dir)
    else:
        run('cmake --build . --config Release --target INSTALL', cwd=build_dir)
    
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