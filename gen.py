#!/usr/bin/env python3
"""ninja build generator for silver project"""

import os, sys, glob, platform, argparse, subprocess
from pathlib import Path

# Collect .c files from your src directory (or any other)
source_dir = Path('src')
source_files = list(source_dir.glob('**/*.c*'))  # recursively finds .c files

parser = argparse.ArgumentParser(description='generate ninja')
parser.add_argument('--debug', action='store_true', help='debug')
args = parser.parse_args()

is_debug = args.debug
fname  = "debug" if is_debug else "release"
system = platform.system()
silver = Path(__file__).resolve().parent

def get_platform_info():
    """get platform-specific settings"""
    global fname
    base = {
        'Windows': {
            'exe': '.exe', 'lib_pre': '', 'lib': '.lib', 'shared': '.dll', 'obj': '.obj',
            'ar': f'{silver}/bin/llvm-ar.exe', 'cc': f'{silver}/bin/clang.exe',
            'cxx': f'{silver}/bin/clang++.exe', 'ninja': 'build/ninja/ninja.exe',
            'inc': [
                r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared'
            ],
            'lib_dirs': [
                f'{silver}\\bin',
                r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64',
                r'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64',
                r'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64'
            ],
            'lflags': ['-fuse-ld=lld', f'-Wl,/{fname}', '-Wl,/pdb:$root/bin/silver.pdb',
                      '-Wl,/SUBSYSTEM:CONSOLE', '-Wl,/NODEFAULTLIB:libcmt', '-Wl,/DEFAULTLIB:msvcrt'],
            'cflags': ['-D_MT'], 'cxxflags': ['-D_MT'],
            'libs': ['-luser32', '-lkernel32', '-lshell32', '-llegacy_stdio_definitions']
        },
        'Darwin': {
            'exe': '', 'lib_pre': 'lib', 'lib': '.a', 'shared': '.dylib', 'obj': '.o',
            'cc': f'{silver}/bin/clang', 'cxx': f'{silver}/bin/clang++',
            'ar': f'{silver}/bin/llvm-ar', 'ninja': 'ninja',
            'inc': [], 'lib_dirs': [], 'lflags': [], 
            'cflags': [], 'cxxflags': [], 'libs': ['-lc', '-lm']
        },
        'Linux': {
            'exe': '', 'lib_pre': 'lib', 'lib': '.a', 'shared': '.so', 'obj': '.o',
            'cc': f'{silver}/bin/clang', 'cxx': f'{silver}/bin/clang++',
            'ar': f'{silver}/bin/llvm-ar', 'ninja': 'ninja',
            'inc': ['/usr/include', '/usr/include/x86_64-linux-gnu'],
            'lib_dirs': [],
            'lflags': ['-fuse-ld=lld', '-lstdc++', '-fsanitize=address'],
            'cflags': ['-D_DLL', '-D_MT', '-fsanitize=address'],
            'cxxflags': ['-D_DLL', '-D_MT', '-fsanitize=address'], # todo: we should have levels of debug, so make asan vs make debug
            'libs': ['-lc', '-lm']
        }
    }
    
    info = base.get(system, base['Linux'])
    
    if system == "Darwin":
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"]).decode().strip()
        info['inc'] = [f'{sdk}/usr/include']
        info['lib_dirs'] = [f'{sdk}/usr/lib']
    
    return info

def parse_g_file(path):
    """parse .g file for deps, link flags, and target type"""
    if not os.path.exists(path):
        return [], [], None
    
    content = open(path).read().strip()
    if not content:
        return [], [], None
    
    deps, links, target = [], [], None
    
    for part in content.split():
        if part in ['@app', '@static', '@shared']:
            target = part[1:]
        else:
            is_not = part.startswith('!')
            if is_not: part = part[1:]
            
            if ':' in part:
                kind, r = part.split(':', 1)
                if is_not ^ (kind.lower() == system.lower()):
                    (links if r.startswith('-l') else deps).append(r)
            elif part.startswith('-l'):
                links.append(part)
            else:
                deps.append(part)
    
    return deps, links, target

def get_modules(src_dir):
    """get all modules with their info"""
    modules = []
    for src in glob.glob(f"{src_dir}/*.c") + glob.glob(f"{src_dir}/*.cc"):
        deps, links, target = parse_g_file(Path(src).with_suffix('.g'))
        modules.append({
            'name': Path(src).stem,
            'src': src,
            'deps': deps,
            'links': links,
            'target': target,
            'is_cxx': src.endswith(('.cc', '.cpp', '.cxx'))
        })
    return modules

def order_modules(modules):
    """resolve build order based on dependencies"""
    ordered, remaining = [], modules.copy()
    mod_map = {m['name']: m for m in modules}
    
    while remaining:
        ready = [m for m in remaining if all(
            d not in mod_map or mod_map[d] not in remaining 
            for d in m['deps']
        )]
        if not ready: ready = remaining.copy()
        ordered.extend(ready)
        for m in ready: remaining.remove(m)
    
    return ordered

def norm_path(p):
    """normalize path for ninja"""
    return str(p).replace('\\', '/')

def escape_path(p):
    """escape path for ninja"""
    return norm_path(p).replace('$', '$$').replace(':', '$:')

def write_ninja(project, root, build_dir, plat):
    """generate build.ninja file"""
    # setup
    os.makedirs(build_dir, exist_ok=True)
    
    with open(build_dir / "compile.rsp", "w") as f:
        for inc in plat['inc']:
            f.write(f'-I"{inc}"\n')
    
    with open(build_dir / "link.rsp", "w") as f:
        for lib in plat['lib_dirs']:
            f.write(f'-L"{lib}"\n')
        for lib in plat['libs']:
            f.write(f"{lib}\n")
    
    # paths
    root_p = norm_path(root)
    build_p = norm_path(build_dir)
    python = norm_path(next((p for p in [root / "bin" / f"python{plat['exe']}"] if p.exists()), 
                            "python3" if plat['exe'] == '' else "python"))
    
    # find files
    modules = order_modules(get_modules(root / "src"))
    headers = glob.glob(f"{root}/src/*.h") + glob.glob(f"{root}/include/*.h")
    non_ext = [str(f) for f in (root / "src").iterdir() if f.is_file() and '.' not in f.name]
    global_deps = ' '.join(norm_path(d) for d in headers + non_ext)
    
    # find main module
    main_mod, target_type = None, None
    
    # check silver.g first
    silver_g = root / "src" / "silver.g"
    if silver_g.exists():
        _, links, target_type = parse_g_file(silver_g)
        if target_type:
            for m in modules:
                if m['name'] == 'silver':
                    main_mod = m
                    m['links'].extend(links)
                    break
    
    # fallback to any module with target
    if not main_mod:
        for m in modules:
            if m['target']:
                main_mod, target_type = m, m['target']
                break
    
    if not main_mod:
        print("error: no module with @app/@static/@shared directive")
        return None
    
    # compiler flags
    base_flags = ["-Wno-write-strings", "-Wno-incompatible-function-pointer-types",
                  "-Wno-compare-distinct-pointer-types", "-Wno-deprecated-declarations",
                  "-Wno-incompatible-pointer-types", "-Wno-shift-op-parentheses",
                  "-Wfatal-errors", "-fno-omit-frame-pointer"]
    
    if plat['exe'] == '.exe':
        base_flags.extend(["--target=x86_64-pc-windows-msvc", "-fno-ms-compatibility", 
                          "-fno-delayed-template-parsing"])
    else:
        base_flags.extend(["-fPIC", "-fvisibility=default"])
    
    global is_debug
    opt_flags = ["-g", "-O0"] if is_debug else ["-O2"]
    includes = [f"-I{build_p}/src/{project}", f"-I{root_p}/src", 
                f"-I{build_p}/src", f"-I{root_p}/include"]
    
    cflags = ' '.join(opt_flags + base_flags + plat['cflags'] + includes)
    cxxflags = ' '.join(opt_flags + base_flags + plat['cxxflags'] + ["-std=c++17"] + includes)
    
    # write ninja file
    n = []
    n.append(f"# generated ninja build for {project}")
    n.append(f"# target: {target_type} ({project})")
    n.append("")
    n.append("ninja_required_version = 1.5")
    n.append(f"llvm_ar = {plat['ar']}")
    n.append(f"clang = {plat['cc']}")
    n.append(f"clangcpp = {plat['cxx']}")
    n.append(f"python = {python}")
    n.append(f"root = {root_p}")
    n.append(f"builddir = {build_p}")
    n.append(f"project = {project}")
    n.append(f"cflags = {cflags}")
    n.append(f"cxxflags = {cxxflags}")
    n.append(f"ldflags = -L{root_p}/lib " + ' '.join(plat['lflags']))
    n.append("")
    
    # rules
    n.append("rule cc")
    n.append(f"  command = $clang @{build_dir}/compile.rsp $cflags -DPROJECT=\"\\\"$project\\\"\" -c $in -o $out")
    n.append("  description = compiling c $in")
    n.append("  depfile = $out.d")
    n.append("  deps = gcc")
    n.append("")
    n.append("rule cxx")
    n.append(f"  command = $clangcpp @{build_dir}/compile.rsp $cxxflags -DPROJECT=\"\\\"$project\\\"\" -c $in -o $out")
    n.append("  description = compiling c++ $in")
    n.append("  depfile = $out.d")
    n.append("  deps = gcc")
    n.append("")
    
    # link rule
    if target_type == 'app':
        n.append("rule link")
        n.append(f"  command = $clang @{build_dir}/link.rsp $in -o $root/$out $ldflags $libs")
        n.append("  description = linking executable $out")
    elif target_type == 'static':
        n.append("rule link")
        n.append("  command = $llvm_ar rcs $root/$out $in")
        n.append("  description = creating static library $out")
    else:  # shared
        n.append("rule link")
        cmd = f"$clang @{build_dir}/link.rsp -shared $in -o $root/$out $ldflags $libs"
        if system == "Darwin":
            cmd += " -install_name @rpath/$out"
        elif system == "Linux":
            cmd += " -Wl,-soname,$out"
        n.append(f"  command = {cmd}")
        n.append("  description = linking shared library $out")
    n.append("")
    
    # header gen
    n.append("rule headers")
    n.append(f"  command = $python $root/headers.py --project-path $root --build-path $builddir --project $project --import $root && touch $out")
    n.append( "  description = generating headers")
    n.append( "  generator = 1")
    n.append("")
    
    stamp = "$builddir/.headers_generated"
    n.append(f"build {stamp}: headers")
    n.append("")
    
    # objects
    objs, all_links = [], set()
    for m in modules:
        obj = f"$builddir/{m['name']}{plat['obj']}"
        objs.append(obj)
        all_links.update(m['links'])
        
        deps = [stamp]
        if global_deps: deps.append(global_deps)
        deps.extend(f"$builddir/{d}{plat['obj']}" for d in m['deps'] if any(x['name'] == d for x in modules))
        
        rule = "cxx" if m['is_cxx'] else "cc"
        flags_var = "cxxflags" if m['is_cxx'] else "cflags"
        
        n.append(f"build {obj}: {rule} {escape_path(norm_path(m['src']))} | {' '.join(deps)}")

        
        n.append(f"  {flags_var} = -I{build_p}/src/{m['name']}  ${flags_var} -DMODULE=\\\"{m['name']}\\\"")
    n.append("")
    
    # final target
    if target_type == 'app':
        output = f"bin/{project}{plat['exe']}"
    elif target_type == 'static':
        output = f"lib/{plat['lib_pre']}{project}{plat['lib']}"
    else:
        output = f"lib/{plat['lib_pre']}{project}{plat['shared']}"
    
    n.append(f"build {output}: link {' '.join(objs)}")
    if all_links:
        n.append(f"  libs = {' '.join(sorted(all_links))}")
    n.append("")
    n.append(f"build all: phony {output}")

    for f in source_files:
        ff     = Path(f)
        stem   = ff.stem
        suffix = ff.suffix
        is_cxx = suffix in ['.cc', '.cpp', '.cxx']
        rule   = {
            'input' : f,
            'output': f'obj/{stem}.o',
            'rule'  :  'cxx' if is_cxx else 'cc',
            'flags' : f'-DMODULE=\\"{stem}\\"',
        }
        input_path  = norm_path(rule['input'])
        output_path = f"$builddir/{rule['output']}"
        n.append     (f"build {output_path}: {rule['rule']} {input_path}")
        flags_var   =  'cflags' if rule['rule'] == 'cc' else 'cxxflags'
        n.append     (f"  {flags_var} = {rule['flags']} -DMODULE=\\\"{m['name']}\\\"")
        objs.append(output_path)

    n.append("default all")
    
    build_ninja = build_dir / "build.ninja"
    with open(build_ninja, 'w') as f:
        f.write('\n'.join(n) + '\n')
    
    print(f"generated {build_ninja}")
    print(f"target: {target_type} -> {output}")
    return build_ninja

def main():
    global fname
    root = Path(__file__).resolve().parent
    ninja_file = write_ninja("silver", root, root / fname, get_platform_info())
    
    if ninja_file and len(sys.argv) > 1 and sys.argv[1] == "--build":
        plat = get_platform_info()
        ninja_exe = root / plat['ninja'].replace('.exe', plat['exe'])
        if not ninja_exe.exists():
            ninja_exe = "ninja"
        
        try:
            subprocess.run([str(ninja_exe), "-f", str(ninja_file), "-C", str(ninja_file.parent)], check=True)
            print("build completed successfully!")
        except subprocess.CalledProcessError as e:
            print(f"build failed with exit code {e.returncode}")
        except FileNotFoundError:
            print("error: ninja not found")

if __name__ == "__main__":
    main()