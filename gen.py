#!/usr/bin/env python3
"""ninja build generator for silver project"""

import os, sys, glob, platform, argparse, subprocess
from pathlib import Path
from graph import parse_g_file, get_env_vars

# Collect .c files from your src directory (or any other)
source_dir = Path('src')
source_files = list(source_dir.glob('**/*.c*'))  # recursively finds .c files

parser = argparse.ArgumentParser(description='generate ninja')
parser.add_argument('--debug', action='store_true', help='debug')
parser.add_argument('--asan', action='store_true', help='enable address sanitizer')
args = parser.parse_args()

is_debug = args.debug
fname  = "debug" if is_debug else "release"
system = platform.system()
silver = Path(__file__).resolve().parent

def get_platform_info():
    """get platform-specific settings"""
    global fname
    sdk=''
    if system == 'Darwin':
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"]).decode().strip()
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
            'cflags': [f'-isysroot{sdk}'], 'cxxflags': [], 'libs': ['-lc', '-lm'],
            'cxxflags': ['-stdlib=libc++', f'-isysroot{sdk}'],   # <-- add this
            'lflags': [f'-isysroot{sdk}'],
            'libs': ['-lc', '-lm', '-lc++', '-lc++abi']
        },
        'Linux': {
            'exe': '', 'lib_pre': 'lib', 'lib': '.a', 'shared': '.so', 'obj': '.o',
            'cc': f'{silver}/bin/clang', 'cxx': f'{silver}/bin/clang++',
            'ar': f'{silver}/bin/llvm-ar', 'ninja': 'ninja',
            'inc': ['/usr/include', '/usr/include/x86_64-linux-gnu'],
            'lib_dirs': [],
            'lflags': ['-fuse-ld=lld', '-lstdc++'],
            'cflags': ['-D_DLL', '-D_MT'],
            'cxxflags': ['-D_DLL', '-D_MT'],
            'libs': ['-lc', '-lm']
        }
    }
    
    info = base.get(system, base['Linux'])
    
    if system == "Darwin":
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"]).decode().strip()
        info['inc'] = [f'{sdk}/usr/include']
        info['lib_dirs'] = [f'{sdk}/usr/lib']
    
    return info


def get_modules(src_dir):
    """get all modules with their info"""
    modules = []
    for src in glob.glob(f"{src_dir}/*.c") + glob.glob(f"{src_dir}/*.cc"):
        deps, links, cflags, target, _ = parse_g_file(Path(src).with_suffix('.g'))
        modules.append({
            'name': Path(src).stem,
            'src': src,
            'deps': deps,
            'links': links,
            'cflags': cflags,
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

def resolve_deps(modules, deps, plat, root_p, builddir):
    out = []
    mod_map = {m['name']: m for m in modules}
    for d in deps:
        if d in mod_map:
            dep_mod = mod_map[d]
            if dep_mod['target'] == 'static':
                out.append(f"{root_p}/lib/{plat['lib_pre']}{d}{plat['lib']}")
            elif dep_mod['target'] == 'shared':
                out.append(f"{root_p}/lib/{plat['lib_pre']}{d}{plat['shared']}")
            elif dep_mod['target'] == 'app':
                out.append(f"{root_p}/bin/{d}{plat['exe']}")
            else:
                out.append(f"{builddir}/{d}{plat['obj']}")
        else:
            out.append(d)
    return out

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
    
    # check for a main target
    main_mod, target_type = None, None
    silver_g = root / "src" / "silver.g"
    if silver_g.exists():
        _, links, cflags, target_type, _ = parse_g_file(silver_g)
        if target_type:
            for m in modules:
                if m['name'] == 'silver':
                    main_mod = m
                    m['links'].extend(links)
                    break
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
                  "-Wno-nullability-completeness", "-Wno-expansion-to-defined",
                  "-Wfatal-errors", "-fno-omit-frame-pointer"]
    if plat['exe'] == '.exe':
        base_flags.extend(["--target=x86_64-pc-windows-msvc", "-fno-ms-compatibility", 
                          "-fno-delayed-template-parsing"])
    else:
        base_flags.extend(["-fPIC", "-fvisibility=default"])
    
    global is_debug
    opt_flags = ["-g", "-O0"] if is_debug else ["-O2"]

    #if args.asan:
        #opt_flags.extend(["-fsanitize=address"])
        # On Linux you also need this to get runtime symbols linked:
        #plat['lflags'].append("-fsanitize=address")
        #plat['libs'].append("-lasan")

    includes = [f"-I{build_p}/src/{project}", f"-I{root_p}/src", 
                f"-I{build_p}/src", f"-I{root_p}/include"]
    
    cflags = ' '.join(opt_flags + base_flags + plat['cflags'] + includes)
    cxxflags = ' '.join(opt_flags + base_flags + plat['cxxflags'] + ["-std=c++17"] + includes)
    
    # write ninja file
    n = []
    n.append(f"# generated ninja build for {project}")
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
    
    # rules for compiling
    n.append("rule cc")
    n.append(f"  command = $clang @$builddir/compile.rsp $cflags -DPROJECT=\"\\\"$project\\\"\" -c $in -o $out")
    n.append("  description = compiling c $in")
    n.append("  depfile = $out.d")
    n.append("  deps = gcc")
    n.append("")
    n.append("rule cxx")
    n.append(f"  command = $clangcpp @$builddir/compile.rsp $cxxflags -DPROJECT=\"\\\"$project\\\"\" -c $in -o $out")
    n.append("  description = compiling c++ $in")
    n.append("  depfile = $out.d")
    n.append("  deps = gcc")
    n.append("")
    
    # separate link rules
    n.append("rule link_app")

    lpath = ''
    if system == "Darwin":
        lpath = '-Wl,-rpath,@executable_path/../lib'
    n.append(f"  command = $clang @$builddir/link.rsp $in -o $out $ldflags $libs {lpath}")

    n.append("  description = linking executable $out")
    n.append("")
    
    n.append("rule link_static")
    n.append("  command = $llvm_ar rcsU $out $in")
    n.append("  description = creating static library $out")
    n.append("  restat = 1")
    n.append("")
    
    n.append("rule link_shared")
    cmd = f"$clang @$builddir/link.rsp -shared $in -o $out $ldflags $libs"
    if system == "Darwin":
        cmd += ' -Wl,-rpath,@executable_path/../lib -install_name $install_name'
    elif system == "Linux":
        cmd += " -Wl,-soname,$out"
    n.append(f"  command = {cmd}")
    n.append("  description = linking shared library $out")
    n.append("")
    
    # header gen
    n.append("rule headers")
    n.append(f"  command = $python $root/headers.py --project-path $root --build-path $builddir --project $project --import $root && touch $out")
    n.append("  description = generating headers")
    n.append("  generator = 1")
    n.append("")
    
    stamp = "$builddir/.headers_generated"
    n.append(f"build {stamp}: headers")
    n.append("")
    
    # objects
    module_objs = {}
    for m in modules:
        obj = f"$builddir/{m['name']}{plat['obj']}"
        module_objs.setdefault(m['name'], []).append(obj)
        
        deps = [stamp]
        if global_deps:
            deps.append(global_deps)
        deps.extend(f"$builddir/{d}{plat['obj']}" for d in m['deps'] if any(x['name'] == d for x in modules))
        
        rule = "cxx" if m['is_cxx'] else "cc"
        flags_var = "cxxflags" if m['is_cxx'] else "cflags"
        
        n.append(f"build {obj}: {rule} {escape_path(norm_path(m['src']))} | {' '.join(deps)}")
        n.append(f"  {flags_var} = -I{build_p}/src/{m['name']}  ${flags_var} {' '.join(m['cflags'])} -DMODULE=\\\"{m['name']}\\\"")
    n.append("")
    
    # handle extra source files not tied to modules
    for f in source_files:
        ff     = Path(f)
        stem   = ff.stem
        suffix = ff.suffix
        is_cxx = suffix in ['.cc', '.cpp', '.cxx']
        rule   = 'cxx' if is_cxx else 'cc'
        flags_var = 'cxxflags' if is_cxx else 'cflags'
        input_path  = norm_path(f)
        output_path = f"$builddir/obj/{stem}{plat['obj']}"
        n.append(f"build {output_path}: {rule} {input_path}")
        n.append(f"  {flags_var} = -DMODULE=\\\"{stem}\\\" ${flags_var}")
        module_objs.setdefault("misc", []).append(output_path)
    n.append("")
    
    # final targets
    outputs = []
    for m in modules:
        objs = ' '.join(module_objs.get(m['name'], []))
        if not objs:
            continue
        
        deps = resolve_deps(modules, m['deps'], plat, root_p, "$builddir")
        if m['target'] == 'app':
            output = f"{root_p}/bin/{m['name']}{plat['exe']}"
            n.append(f"build {output}: link_app {objs} {' '.join(deps)}")
            libs = sorted(set(m['links']))
            if libs:
                n.append(f"  libs = {' '.join(libs)}")
            n.append("")

        elif m['target'] == 'static':
            output = f"{root_p}/lib/{plat['lib_pre']}{m['name']}{plat['lib']}"
            n.append(f"build {output}: link_static {objs}")
            n.append("")

        elif m['target'] == 'shared':
            output = f"{root_p}/lib/{plat['lib_pre']}{m['name']}{plat['shared']}"
            install_name = os.path.basename(output)
            n.append(f"build {output}: link_shared {objs} {' '.join(deps)}")
            libs = sorted(set(m['links']))
            if libs:
                n.append(f"  libs = {' '.join(libs)}")
            if system == "Darwin":
                n.append(f"  install_name = @rpath/{install_name}")
            n.append("")
        
        else:
            continue

        outputs.append(output)

    if outputs:
        n.append(f"build all: phony {' '.join(outputs)}")
        n.append("")

    n.append("default all\n")
    
    build_ninja = build_dir / "build.ninja"
    with open(build_ninja, 'w') as f:
        f.write('\n'.join(n) + '\n')
    
    print(f"generated {build_ninja}")
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