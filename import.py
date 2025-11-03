#!/usr/bin/env python3
"""import software from graph files (.g) in src"""

import site, pprint
import os, sys, glob, platform, argparse, subprocess, re
from pathlib import Path
from graph import parse_g_file, get_env_vars

v = get_env_vars()
root   = v['PROJECT_PATH']
IMPORT = v['IMPORT']
NATIVE = Path(v['SILVER']) / "sdk" / "native"
SDK    = v['SDK']
os.environ['IMPORT'] = IMPORT
os.environ['SDK']    = SDK

def run(cmd, cwd=None, check=True):
    print(">", cmd)
    subprocess.run(cmd, cwd=cwd, shell=True, check=check)

def eval_braces(s, context=None):
    """Replace {expr} in a string with eval(expr) using given context dict."""
    if context is None:
        context = {}
    pattern = re.compile(r"\{([^{}]+)\}")
    win = sys.platform.startswith("win")
    global SDK
    context.update({
        "win": win,
        "lin": sys.platform.startswith("linux"),
        "mac": sys.platform == "darwin",
        "SDK": SDK,
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

def parse_from_config(all):
    res  = []
    pre  = []
    post = []
    env  = {}
    env_pattern = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)=(.*)$")
    for c in all:
        c = vreplace(c)
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

last_name   = None
last_uri    = None
last_commit = None

def get_element(arr, index, default=None):
    if index < len(arr):
        return arr[index]
    return default

def build_import(name, uri, commit, _config_lines, install_dir, extra):
    # we might need an argument for this in import, but i dont see cases other than the compiler
    if extra == 'native':
        install_dir = NATIVE
    
    global last_uri
    global last_commit
    global last_name
    global IMPORT
    global root
    global SDK

    checkout_dir = Path(root)   / Path('checkout') / name
    build_dir    = Path(IMPORT) / Path('release')  / name

    config_lines, pre, post, env = parse_from_config(_config_lines) # pre has > and post as >> infront ... everything else is a config_line
    
    token_file = build_dir / 'silver-token'
    # continue if silver-token exists
    if token_file.exists():
        last_uri = uri
        last_commit = commit
        last_name = name
        return
    
    # checkout_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    # fetch or clone
    if last_uri == uri and last_commit == commit:
        # make symlink for name -> last_name in the checkout dir (so we dont do double checkouts on these combined projects)
        if last_name and last_name != name:
            target_path = Path(root) / 'checkout' / last_name
            if not checkout_dir.exists():
                checkout_dir.symlink_to(target_path, target_is_directory=True)
    elif checkout_dir.exists():
        if commit:
            run(f"git -C {checkout_dir} fetch origin")
            run(f"git -C {checkout_dir} checkout {commit}")
            run(f"git -C {checkout_dir} pull origin {commit}", check=False)
    else:
        run(f"git clone {uri} {checkout_dir}")
        if commit:
            run(f"git -C {checkout_dir} checkout {commit}")

    last_uri = uri
    last_commit = commit
    last_name = name

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
        f"-DCMAKE_INSTALL_PREFIX={install_dir}"
    ]

    if extra != 'native' and SDK !='native':
        config_lines += [
            f"-DCMAKE_SYSROOT={IMPORT}/usr",
            f"-DCMAKE_C_COMPILER_TARGET={SDK}",
            f"-DCMAKE_CXX_COMPILER_TARGET={SDK}",
        ]


    # lets loop through config_lines and omit
    cfg = []
    for l in config_lines:
        if name != 'llvm-project':
            if l.startswith('-DCMAKE_C_COMPILER') or l.startswith('-DCMAKE_CXX_COMPILER'):
                continue
        cfg.append(l)

    cmake_args = " ".join(cfg)

    s = '/'
    win = sys.platform.startswith('win')
    ext = '.exe' if win else ''

    # generally, software producers throw in MSVC-style command-line switches for cl compiler, and generally do NOT test against clang-cl
    # to this end, we need to go with the compiler its designed for first
    # on linux/mac though, we can attempt our own clang as its preferrable to go with our version we built
    if SDK == 'native' and not '-DCMAKE_C_COMPILER=' in cmake_args:
        cc         = 'cl' if win else 'clang'
        cpp        = 'cl' if win else 'clang++'
        idir       = ''   if win else install_dir + s + 'bin' + s
        clang      = f'{idir}{cc}{ext}'
        clangpp    = f'{idir}{cpp}{ext}'
        cmake_args = f'-DCMAKE_C_COMPILER="{clang}" -DCMAKE_CXX_COMPILER="{clangpp}" ' + cmake_args
    
    if not '-G ' in cmake_args:
        cmake_args = '-G Ninja ' + cmake_args

    if not '-S ' in cmake_args:
        cmake_args = f'-S {checkout_dir} ' + cmake_args

    is_ninja = '-G Ninja' in cmake_args or '-G "Ninja"' in cmake_args
    tc = ''
    if name != 'llvm-project':
        tc = f"--toolchain='{IMPORT}/target.cmake'"
    
    # replace environment vars in cmake_args that begin with $something with %something% on windows
    run(f"cmake {tc} {cmake_args}", cwd=build_dir)
    
    # build & install
    cpu_count = os.cpu_count() or 4
    if is_ninja:
        print(f'running ninja for {name}')
        run(f"ninja -j{max(1, cpu_count//2)}", cwd=build_dir)
        run("ninja install", cwd=build_dir)
    else:
        run('cmake --build . --config Release --target INSTALL', cwd=build_dir)
    
    # run post commands
    for cmd in post:
        run(cmd, cwd=build_dir)

    token_file.touch()

def import_src(root_dir="src"):
    IMPORT = os.environ.get('IMPORT')
    global NATIVE
    for path in Path(root_dir).rglob("*.g"):
        _, _, _, _, imports = parse_g_file(str(path))
        for i in imports:
            print(f'{path.stem:<10} import {i[0]}')
            build_import(i[0], i[1], i[2], i[3], IMPORT, i[4])

# ((name, uri, commit, configs, extra))

import_src()