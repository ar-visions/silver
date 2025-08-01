#!/usr/bin/env python3
"""
Direct Ninja build file generator for Silver project
Converts CMakeLists.txt logic to build.ninja
"""

import  os
import  sys
import  glob
import  platform
import  argparse
import  subprocess
from    pathlib import Path

parser      = argparse.ArgumentParser(description='Generate Ninja')
parser.add_argument('--debug', action='store_true', help='Debug')

args        = parser.parse_args()
is_debug    = args.debug
system      = platform.system()
silver      = Path(__file__).resolve().parent

def get_platform_info():
    """Get platform-specific settings"""
    global system
    global silver
    
    base_info = {
        'Windows': {
            'exe_suffix':   '.exe',
            'lib_prefix':   '',
            'lib_suffix':   '.lib',
            'obj_suffix':   '.obj',
            'llvm_ar_exe':  f'{silver}/bin/llvm-ar.exe',
            'clang_exe':    f'{silver}/bin/clang.exe',
            'clangcpp_exe': f'{silver}/bin/clang++.exe',
            'ninja_exe':    'build/ninja/ninja.exe',
            'includes': [
                r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared'
            ],
            'libs': [
                f'{silver}\\bin',
                r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64',
                r'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64',
                r'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64'
            ],
            'lflags': [
                '-fuse-ld=lld',
                '-Wl,/debug',
                '-Wl,/pdb:$root/bin/silver.pdb',
                '-Wl,/SUBSYSTEM:CONSOLE',
                '-Wl,/NODEFAULTLIB:libcmt', '-Wl,/DEFAULTLIB:msvcrt' ],
            'cflags':       [ '-D_MT' ],
            'cxxflags':     [ '-D_MT' ],
            'system_libs':  ['-luser32', '-lkernel32', '-lshell32', '-llegacy_stdio_definitions']
        },
        'Darwin': {
            'exe_suffix':   '',
            'lib_prefix':   'lib',
            'lib_suffix':   '.dylib',
            'obj_suffix':   '.o',
            'clang_exe':    f'{silver}/bin/clang',
            'clangcpp_exe': f'{silver}/bin/clang++',
            'ninja_exe':    'ninja',
            'includes':     [],  # Will be filled dynamically
            'libs':         [],  # Will be filled dynamically
            'lflags':       [ ],
            'cflags':       [ ],
            'cxxflags':     [ ],
            'system_libs':  ['-lc', '-lm']
        },
        'Linux': {
            'exe_suffix':   '',
            'lib_prefix':   'lib',
            'lib_suffix':   '.so',
            'obj_suffix':   '.o',
            'clang_exe':    f'{silver}/bin/clang',
            'clangcpp_exe': f'{silver}/bin/clang++',
            'ninja_exe':    'ninja',
            'includes':     [
                '/usr/include',
                '/usr/include/x86_64-linux-gnu'
            ],
            'libs':         [ ],
            'lflags':       [ ],
            'cflags':       [ '-D_DLL', '-D_MT' ],
            'cxxflags':     [ '-D_DLL', '-D_MT' ],
            'system_libs':  ['-lc', '-lm']
        }
    }
    
    info = base_info.get(system, base_info['Linux'])
    
    # Special handling for macOS SDK
    if system == "Darwin":
        sdk_root = subprocess.check_output(["xcrun", "--show-sdk-path"]).decode().strip()
        info['includes'] = [f'{sdk_root}/usr/include']
        info['libs'] = [f'{sdk_root}/usr/lib']
    
    return info

def find_sources(pattern):
    """Find source files matching pattern"""
    return glob.glob(pattern)

def parse_g_file(g_file_path):
    """Parse .g file for dependencies and link flags"""
    if not os.path.exists(g_file_path):
        return [], []
    
    with open(g_file_path, 'r') as f:
        content = f.read().strip()
    
    if not content:
        return [], []
    
    parts = content.split()
    dependencies = []
    link_flags = []
    
    global system
    for part in parts:
        is_not = part[0] == '!'
        if is_not: part = part[1:]

        if ':' in part:
            kind, r = part.split(':', 1)
            if is_not ^ (kind.lower() == system.lower()):
                if r.startswith('-l'):
                    link_flags.append(r)
                else:
                    dependencies.append(r)
                
        elif part.startswith('-l'):
            # Library flag
            link_flags.append(part)
        else:
            # Dependency symbol for build ordering
            dependencies.append(part)
    
    return dependencies, link_flags

def get_module_info(src_file):
    """Get module information including .g file data"""
    src_path = Path(src_file)
    g_file = src_path.with_suffix('.g')
    
    dependencies, link_flags = parse_g_file(str(g_file))
    
    return {
        'name': src_path.stem,
        'source': str(src_file),
        'dependencies': dependencies,
        'link_flags': link_flags,
        'has_g_file': g_file.exists()
    }

def escape_path(path):
    """Escapes a path for use in a Ninja file."""
    # Normalize path to forward slashes
    path = str(path).replace("\\", "/")

    # Escape characters per Ninja's spec: $ and :
    path = path.replace('$', '$$')
    path = path.replace(':', '$:')
    return path

def normalize_path(path):
    """Normalize path for ninja (always use forward slashes)"""
    return str(path).replace('\\', '/')

def resolve_build_order(modules):
    """Resolve build order based on dependencies in .g files"""
    ordered = []
    remaining = modules.copy()
    module_map = {m['name']: m for m in modules}
    
    while remaining:
        # Find modules with no unresolved dependencies
        ready = []
        for module in remaining:
            deps_satisfied = True
            for dep in module['dependencies']:
                if dep in module_map and module_map[dep] in remaining:
                    deps_satisfied = False
                    break
            if deps_satisfied:
                ready.append(module)
        
        if not ready:
            # Circular dependency or missing dependency - just add remaining
            ready = remaining.copy()
        
        ordered.extend(ready)
        for module in ready:
            remaining.remove(module)
    
    return ordered

class NinjaBuilder:
    """Builder class to construct ninja file content"""
    def __init__(self, project_name, root_dir, build_dir, platform):
        self.project_name = project_name
        self.root_dir = root_dir
        self.build_dir = build_dir
        self.platform = platform
        self.content = []
        self.indent = 0
        
    def add_comment(self, comment):
        self.content.append(f"# {comment}")
        
    def add_blank(self):
        self.content.append("")
        
    def add_variable(self, name, value):
        self.content.append(f"{name} = {value}")
        
    def add_rule(self, name, command, description, **kwargs):
        self.content.append(f"rule {name}")
        self.content.append(f"  command = {command}")
        self.content.append(f"  description = {description}")
        for key, value in kwargs.items():
            self.content.append(f"  {key} = {value}")
            
    def add_build(self, outputs, rule, inputs, deps=None, **variables):
        line = f"build {outputs}: {rule} {inputs}"
        if deps:
            line += f" | {deps}"
        self.content.append(line)
        for key, value in variables.items():
            self.content.append(f"  {key} = {value}")
            
    def add_default(self, targets):
        self.content.append(f"default {targets}")
        
    def get_content(self):
        return "\n".join(self.content)

def generate_ninja_build():
    """Generate build.ninja file"""
    
    # Configuration
    project_name = "silver"
    root_dir = Path.cwd()
    build_dir = root_dir / "debug"
    platform = get_platform_info()

    os.makedirs(build_dir, exist_ok=True)
    compile_rsp = os.path.join(build_dir, "compile.rsp")
    link_rsp = os.path.join(build_dir, "link.rsp")

    # --- Compile .rsp ---
    with open(compile_rsp, "w") as f:
        for inc in platform.get("includes", []):
            f.write(f'-I"{inc}"\n')

    # --- Link .rsp ---
    with open(link_rsp, "w") as f:
        for lib in platform.get("libs", []):
            f.write(f'-L"{lib}"\n')
        
        # Add system libraries
        for lib in platform.get("system_libs", []):
            f.write(f"{lib}\n")

    # Normalize paths for ninja
    root_path = normalize_path(root_dir)
    build_path = normalize_path(build_dir)
    
    # Find Python executable
    python_paths = [
        root_dir / "bin" / f"python{platform['exe_suffix']}"
    ]
    
    python_exe = None
    for path in python_paths:
        if path.exists():
            python_exe = normalize_path(path)
            break
    
    if not python_exe:
        python_exe = "python3" if platform['exe_suffix'] == '' else "python"
    
    clang_exe = platform['clang_exe']
    clangcpp_exe = platform['clangcpp_exe']
    llvm_ar_exe = platform['llvm_ar_exe']

    # Find source files
    src_c_files = find_sources("src/*.c")
    src_cc_files = find_sources("src/*.cc") 
    app_c_files = find_sources("app/*.c")
    app_cc_files = find_sources("app/*.cc")
    
    # Parse module information
    src_modules = [get_module_info(f) for f in src_c_files + src_cc_files]
    app_modules = [get_module_info(f) for f in app_c_files + app_cc_files]
    
    # Resolve build orders
    src_modules_ordered = resolve_build_order(src_modules)
    app_modules_ordered = resolve_build_order(app_modules)
    
    global is_debug
    
    # Compiler flags
    base_c_flags = [
        "-Wno-write-strings", 
        "-Wno-incompatible-function-pointer-types",
        "-Wno-compare-distinct-pointer-types",
        "-Wno-deprecated-declarations",
        "-Wno-incompatible-pointer-types", 
        "-Wno-shift-op-parentheses",
        "-Wfatal-errors",
        "-fno-omit-frame-pointer"
    ]
    
    # Platform-specific flags
    if platform['exe_suffix'] == '.exe':  # Windows
        base_c_flags.extend([
            "--target=x86_64-pc-windows-msvc",
            "-fno-ms-compatibility",
            "-fno-delayed-template-parsing"
        ])
    else:  # Unix-like
        base_c_flags.extend([
            "-fPIC",
            "-fvisibility=default"
        ])
    
    debug_flags = base_c_flags + ["-g", "-O0"]
    release_flags = base_c_flags + ["-O2"]

    if is_debug:
        c_flags = debug_flags + platform['cflags']
        cxx_flags = debug_flags + platform['cxxflags'] + ["-std=c++17"]
    else:
        c_flags = release_flags + platform['cflags']
        cxx_flags = release_flags + platform['cxxflags'] + ["-std=c++17"]

    # Include directories
    includes = [
        f"-I{build_path}/src/{project_name}",
        f"-I{root_path}/src",
        f"-I{build_path}/src", 
        f"-I{build_path}/app",
        f"-I{root_path}/include"
    ]
    
    cflags = ' '.join(c_flags + includes)
    cxxflags = ' '.join(cxx_flags + includes)
    
    # Create ninja builder
    ninja = NinjaBuilder(project_name, root_dir, build_dir, platform)
    
    # Header
    ninja.add_comment(f"Generated Ninja build file for {project_name}")
    ninja.add_comment(f"Build directory: {build_path}")
    ninja.add_blank()
    
    ninja.add_variable("ninja_required_version", "1.5")
    ninja.add_blank()
    
    # Variables
    ninja.add_comment("Variables")
    ninja.add_variable("llvm_ar", llvm_ar_exe)
    ninja.add_variable("clang", clang_exe)
    ninja.add_variable("clangcpp", clangcpp_exe)
    ninja.add_variable("python", python_exe)
    ninja.add_variable("root", root_path)
    ninja.add_variable("builddir", build_path)
    ninja.add_variable("project", project_name)
    ninja.add_blank()
    
    # Compiler flags
    ninja.add_comment("Compiler flags")
    ninja.add_variable("cflags", cflags)
    ninja.add_variable("cxxflags", cxxflags)
    ninja.add_variable("ldflags", f"-L{root_path}/lib " + ' '.join(platform['lflags']))
    ninja.add_blank()
    
    # Build rules
    ninja.add_comment("Build rules")
    ninja.add_rule("cc",
        command=f"$clang @{compile_rsp} $cflags -DMODULE=\"\\\"$project\\\"\" -c $in -o $out",
        description="Compiling C $in",
        depfile="$out.d",
        deps="gcc"
    )
    ninja.add_blank()
    
    ninja.add_rule("cxx",
        command=f"$clangcpp @{compile_rsp} $cxxflags -DMODULE=\"\\\"$project\\\"\" -c $in -o $out",
        description="Compiling C++ $in",
        depfile="$out.d",
        deps="gcc"
    )
    ninja.add_blank()
    
    # Platform-specific link rules
    if system == "Windows":
        ninja.add_rule("link_static",
            command=f"$llvm_ar rcs $root/$out $in",
            #command=f"$clang @{link_rsp} $in -o $root/$out $ldflags $libs",
            description="Linking shared library $out"
        )
        ninja.add_blank()
        
        ninja.add_rule("link_exe",
            command=f"$clang @{link_rsp} $in -o $root/$out $ldflags -l$project $libs",
            description="Linking executable $out"
        )
    elif system == "Darwin":
        ninja.add_rule("link_static",
            command=f"$clang @{link_rsp} $in -o $root/$out $ldflags $libs",
            description="Linking shared library $out"
        )
        ninja.add_blank()
        
        ninja.add_rule("link_exe",
            command=f"$clang @{link_rsp} $in -o $root/$out $ldflags -l$project $libs -Wl,-rpath,@executable_path/../lib",
            description="Linking executable $out"
        )
    else:  # Linux
        ninja.add_rule("link_static",
            command=f"$clang @{link_rsp} $in -o $root/$out $ldflags $libs",
            description="Linking shared library $out"
        )
        ninja.add_blank()
        
        ninja.add_rule("link_exe",
            command=f"$clang @{link_rsp} $in -o $root/$out $ldflags -l$project $libs -Wl,-rpath,$ORIGIN/../lib",
            description="Linking executable $out"
        )
    ninja.add_blank()
    
    # Python header generation rules
    ninja.add_rule("python_header_src",
        command="$python $root/support/headers.py --project-path $root --directive src --build-path $builddir --project $project --import $root",
        description="Generating src headers"
    )
    ninja.add_blank()
    
    ninja.add_rule("python_header_app",
        command="$python $root/support/headers.py --project-path $root --directive app --build-path $builddir --project $project --import $root",
        description="Generating app headers"
    )
    ninja.add_blank()
    
    # Header generation targets
    ninja.add_comment("Header generation targets")
    ninja.add_build("src_headers", "python_header_src", "")
    ninja.add_build("app_headers", "python_header_app", "")
    ninja.add_blank()
    
    # Object files for library
    ninja.add_comment("Object files for library (respecting build order)")
    lib_objects = []
    lib_link_flags = set()
    
    for module in src_modules_ordered:
        src = Path(module['source']).resolve()
        stem = src.stem
        src_dbg_path = f'-I{build_path}/src/{stem}'
        
        obj = f"$builddir/lib_{module['name']}{platform['obj_suffix']}"
        lib_objects.append(obj)
        
        # Collect link flags from .g files
        lib_link_flags.update(module['link_flags'])
        
        # Build dependencies
        deps = ["src_headers"]
        for dep_name in module['dependencies']:
            for other_module in src_modules_ordered:
                if other_module['name'] == dep_name:
                    dep_obj = f"$builddir/lib_{dep_name}{platform['obj_suffix']}"
                    deps.append(dep_obj)
                    break
        
        # Determine compiler rule
        rule = "cxx" if str(src).endswith(('.cc', '.cpp', '.cxx')) else "cc"
        deps_str = " ".join(deps) if deps else None
        
        # Build variables
        build_vars = {}
        if rule == "cc":
            build_vars["cflags"] = f"{src_dbg_path} {cflags}"
        else:
            build_vars["cxxflags"] = f"{src_dbg_path} {cxxflags}"
        
        ninja.add_build(obj, rule, escape_path(normalize_path(src)), deps=deps_str, **build_vars)
        
        if module['has_g_file']:
            ninja.add_comment(f"Dependencies from {module['name']}.g: {' '.join(module['dependencies'])}")
            if module['link_flags']:
                ninja.add_comment(f"Link flags: {' '.join(module['link_flags'])}")
    
    ninja.add_blank()
    
    # Shared library
    ninja.add_comment("Shared library")
    lib_name = f"lib/{platform['lib_prefix']}{project_name}{platform['lib_suffix']}"
    lib_flags = ' '.join(platform['lflags'] + sorted(lib_link_flags)) if lib_link_flags else ''
    
    build_vars = {}
    if lib_flags:
        build_vars["libs"] = lib_flags
    
    ninja.add_build(lib_name, "link_static", ' '.join(lib_objects), **build_vars)
    ninja.add_blank()
    
    # Executables
    ninja.add_comment("Executables")
    exe_targets = []
    
    for module in app_modules_ordered:
        src = Path(module['source']).resolve()
        app_name = module['name']
        obj = f"$builddir/app_{app_name}{platform['obj_suffix']}"
        exe = f"bin/{app_name}{platform['exe_suffix']}"
        exe_targets.append(exe)
        
        # Build dependencies for app
        deps = ["app_headers"]
        for dep_name in module['dependencies']:
            # Check if dependency is in src modules
            for src_module in src_modules_ordered:
                if src_module['name'] == dep_name:
                    dep_obj = f"$builddir/lib_{dep_name}{platform['obj_suffix']}"
                    deps.append(dep_obj)
                    break
            # Check if dependency is in other app modules
            for app_module in app_modules_ordered:
                if app_module['name'] == dep_name and app_module != module:
                    dep_obj = f"$builddir/app_{dep_name}{platform['obj_suffix']}"
                    deps.append(dep_obj)
                    break
        
        # Object file
        rule = "cxx" if str(src).endswith(('.cc', '.cpp', '.cxx')) else "cc"
        deps_str = " ".join(deps) if deps else None
        
        ninja.add_build(obj, rule, escape_path(normalize_path(src)), deps=deps_str)
        
        if module['has_g_file']:
            ninja.add_comment(f"Dependencies from {app_name}.g: {' '.join(module['dependencies'])}")
        
        # Executable with link flags
        exe_vars = {}
        if module['link_flags']:
            exe_vars["libs"] = ' '.join(module['link_flags'])
        
        ninja.add_build(exe, "link_exe", obj, deps=lib_name, **exe_vars)
    
    ninja.add_blank()
    
    # Default target
    ninja.add_comment("Default target")
    all_targets = f"{lib_name} {' '.join(exe_targets)}"
    ninja.add_build("all", "phony", all_targets)
    ninja.add_default("all")
    
    # Write build.ninja
    build_ninja = build_dir / "build.ninja"
    build_dir.mkdir(exist_ok=True)
    
    with open(build_ninja, 'w') as f:
        f.write(ninja.get_content())
        f.write('\n')
    
    print(f"Generated {build_ninja}")
    return build_ninja

def run_ninja_build(ninja_file):
    """Run the ninja build"""
    platform = get_platform_info()
    ninja_exe = Path.cwd() / platform['ninja_exe'].replace('.exe', platform['exe_suffix'])
    
    if not ninja_exe.exists():
        ninja_exe = "ninja"  # Try system ninja
    
    try:
        result = subprocess.run([
            str(ninja_exe), 
            "-f", str(ninja_file),
            "-C", str(ninja_file.parent)
        ], check=True)
        print("Build completed successfully!")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Build failed with exit code {e.returncode}")
        return False
    except FileNotFoundError:
        print("Error: ninja not found. Install ninja or run bootstrap script.")
        return False

def main():
    """Main function"""
    if len(sys.argv) > 1 and sys.argv[1] == "--build":
        ninja_file = generate_ninja_build()
        run_ninja_build(ninja_file)
    else:
        generate_ninja_build()
        print("Run with --build to also execute the build")

if __name__ == "__main__":
    main()