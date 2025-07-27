#!/usr/bin/env python3
"""
Direct Ninja build file generator for Silver project
Converts CMakeLists.txt logic to build.ninja
"""

import os
import sys
import glob
import platform
from pathlib import Path
import argparse

parser = argparse.ArgumentParser(description='Generate Ninja')
parser.add_argument('--debug', action='store_true', help='Debug')
args = parser.parse_args()
is_debug = args.debug

system = platform.system()

silver = Path(__file__).resolve().parent

def get_platform_info():
    """Get platform-specific settings"""
    global system
    global silver
    # fetch the Windows SDK directory dynamically; there can be many and we may want the latest sort
    if system == "Windows":
        return {
            'exe_suffix': '.exe',
            'lib_prefix': '',
            'lib_suffix': '.dll',
            'obj_suffix': '.obj',
            'clang_exe': f'{silver}/bin/clang.exe',
            'clangcpp_exe': f'{silver}/bin/clang++.exe',
            'ninja_exe': 'build/ninja/ninja.exe',
            'includes': [
                r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt',
                r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared'
            ],
            'libs': [
                r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64',
                r'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64',
                r'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64'
            ]
        }
    elif system == "Darwin":
        sdk_root = subprocess.check_output(["xcrun", "--show-sdk-path"]).decode().strip()

        return {
            'exe_suffix': '',
            'lib_prefix': 'lib',
            'lib_suffix': '.dylib',
            'obj_suffix': '.o',
            'clang_exe': f'{silver}/bin/clang',
            'clangcpp_exe': f'{silver}/bin/clang++',
            'ninja_exe': 'ninja',
            'includes': [ f'{sdk_root}/usr/include' ],
            'libs': [ f'{sdk_root}/usr/lib' ]
        }
    else:  # Linux
        return {
            'exe_suffix': '',
            'lib_prefix': 'lib',
            'lib_suffix': '.so',
            'obj_suffix': '.o',
            'clang_exe': f'{silver}/bin/clang',
            'clangcpp_exe': f'{silver}/bin/clang++',
            'ninja_exe': 'ninja',
            'includes': [
                '/usr/include',
                '/usr/include/x86_64-linux-gnu'
            ],
            'libs': [
            ]
        }

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
    
    for part in parts:
        if part.startswith('-l'):
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

def normalize_path_for_ninja(path, root_dir):
    """Normalize path for ninja, using relative paths on Windows to avoid colon issues"""
    path_obj = Path(path)
    root_obj = Path(root_dir)
    
    try:
        # Try to make it relative to root directory
        rel_path = path_obj.relative_to(root_obj)
        return str(rel_path).replace('\\', '/')
    except ValueError:
        # If not relative, use absolute but escape colons on Windows
        abs_path = str(path_obj).replace('\\', '/')
        if ':' in abs_path and system == "Windows":
            # Use $root variable for absolute paths
            return abs_path.replace(str(root_obj).replace('\\', '/'), '$root')
        return abs_path

def normalize_path(path):
    """Normalize path for ninja (always use forward slashes)"""
    return str(path).replace('\\', '/')

def generate_ninja_build():
    """Generate build.ninja file"""
    
    # Configuration
    project_name = "silver"
    root_dir = Path.cwd()
    build_dir = root_dir / "debug"  # Headers output to debug, ninja file in root
    platform = get_platform_info()

    os.makedirs(build_dir, exist_ok=True)
    compile_rsp = os.path.join(build_dir, "compile.rsp")
    link_rsp    = os.path.join(build_dir, "link.rsp")

    # --- Compile .rsp ---
    with open(compile_rsp, "w") as f:
        for inc in platform.get("includes", []):
            f.write(f'-I"{inc}"\n')

    # --- Link .rsp ---
    with open(link_rsp, "w") as f:
        for lib in platform.get("libs", []):
            f.write(f'-L"{lib}"\n')
        
        # standard system libs for C
        if system == 'Windows':
            f.write("-luser32\n")
            f.write("-lkernel32\n")
            f.write("-lshell32\n")
            f.write("-llegacy_stdio_definitions\n")
        else:
            f.write("-lc\n")
            f.write("-lm\n")


    # lets make the rsp files here for compile, link 
    
    # Normalize paths for ninja
    root_path = normalize_path(root_dir)
    build_path = normalize_path(build_dir)
    
    # Paths - check multiple locations for Python
    python_paths = [
        root_dir / "bin" / f"python{platform['exe_suffix']}"
    ]
    
    python_exe = None
    for path in python_paths:
        if path.exists():
            python_exe = normalize_path(path)
            break
    
    if not python_exe:
        # Fallback to system python
        python_exe = "python3" if platform['exe_suffix'] == '' else "python"
    
    clang_exe = platform['clang_exe']
    clangcpp_exe = platform['clangcpp_exe']
    
    # Source files with dependency analysis
    src_c_files = find_sources("src/*.c")
    src_cc_files = find_sources("src/*.cc") 
    app_c_files = find_sources("app/*.c")
    app_cc_files = find_sources("app/*.cc")
    
    # Parse module information including .g files
    src_modules = [get_module_info(f) for f in src_c_files + src_cc_files]
    app_modules = [get_module_info(f) for f in app_c_files + app_cc_files]
    
    # Build dependency graph for ordering
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
    ]
    
    # Platform-specific flags
    if platform['exe_suffix'] == '.exe':  # Windows
        # Force GNU target triple and add Windows-specific flags
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
    
    debug_flags   = base_c_flags + ["-g2", "-O0"]
    release_flags = base_c_flags + ["-O2"]

    if is_debug:
        c_flags   = debug_flags
        cxx_flags = debug_flags + ["-std=c++17"]  # Using debug by default
    else:
        c_flags   = release_flags
        cxx_flags = release_flags + ["-std=c++17"]  # Using debug by default

    # Include directories
    includes = [
        f"-I{build_path}/src/{project_name}",
        f"-I{root_path}/src",
        f"-I{build_path}/src", 
        f"-I{build_path}/app",
        f"-I{root_path}/include"
    ]
    
    cflags   = ' '.join(c_flags   + includes)
    cxxflags = ' '.join(cxx_flags + includes)
    # Generate build.ninja content
    ninja_content = f"""# Generated Ninja build file for {project_name}
# Build directory: {build_path}

ninja_required_version = 1.5

# Variables
clang = {clang_exe}
clangcpp = {clangcpp_exe}
python = {python_exe}
root = {root_path}
builddir = {build_path}
project = {project_name}

# Compiler flags
cflags = {cflags}
cxxflags = {cxxflags}
ldflags = -L{root_path}/lib

# Build rules
rule cc
  command = $clang @{compile_rsp} $cflags -DMODULE="\\"$project\\"" -c $in -o $out
  description = Compiling C $in
  depfile = $out.d
  deps = gcc

rule cxx  
  command = $clangcpp @{compile_rsp} $cxxflags -DMODULE="\\"$project\\"" -c $in -o $out
  description = Compiling C++ $in
  depfile = $out.d
  deps = gcc

rule link_shared
  command = $clang @{link_rsp} -shared $in -o $root/$out $ldflags $libs"""

    # Add platform-specific linking
    if platform['exe_suffix'] == '':  # Unix-like
        ninja_content += f"""
  description = Linking shared library $out

rule link_exe
  command = $clang @{link_rsp} $in -o $root/$out $ldflags -l$project $libs"""
        if platform['lib_suffix'] == '.dylib':  # macOS
            ninja_content = ninja_content.replace(f'rule link_shared\n  command = $clang @{link_rsp} -shared $in -o $root/$out $ldflags $libs',
                                                f'rule link_shared\n  command = $clang @{link_rsp} -shared $in -o $root/$out $ldflags $libs -Wl,-install_name,@rpath/lib$project.dylib')
            ninja_content += " -Wl,-rpath,@executable_path/../lib"
        else:  # Linux
            ninja_content = ninja_content.replace(f'rule link_shared\n  command = $clang @{link_rsp} -shared $in -o $root/$out $ldflags $libs',
                                                f'rule link_shared\n  command = $clang @{link_rsp} -shared $in -o $root/$out $ldflags $libs -Wl,-soname,lib$project.so')
            ninja_content += " -Wl,-rpath,$ORIGIN/../lib"
    else:  # Windows
        ninja_content += f"""
  description = Linking shared library $out

rule link_exe
  command = $clang @{link_rsp} $in -o $root/$out $ldflags -l$project $libs"""
            
    ninja_content += f"""
  description = Linking executable $out

rule python_header_src
  command = $python  $root/support/headers.py --project-path $root --directive src --build-path $builddir --project $project --import $root
  description = Generating src headers

rule python_header_app  
  command = $python  $root/support/headers.py --project-path $root --directive app --build-path $builddir --project $project --import $root
  description = Generating app headers

# Header generation targets
build src_headers: python_header_src
build app_headers: python_header_app

# Object files for library (respecting build order)
"""

    # Generate object file builds for library sources in dependency order
    lib_objects = []
    lib_link_flags = set()
    
    for module in src_modules_ordered:
        src = Path(module['source']).resolve()
        stem = src.stem
        src_dbg_path = f'-I{build_path}/src/{stem}' # do i append in here?

        obj = f"$builddir/lib_{module['name']}{platform['obj_suffix']}"
        lib_objects.append(obj)
        
        # Collect link flags from .g files
        lib_link_flags.update(module['link_flags'])
        
        # Build dependencies - other modules this depends on
        deps = ["src_headers"]
        for dep_name in module['dependencies']:
            # Find the dependency object file
            for other_module in src_modules_ordered:
                if other_module['name'] == dep_name:
                    dep_obj = f"$builddir/lib_{dep_name}{platform['obj_suffix']}"
                    deps.append(dep_obj)
                    break
        
        # Determine compiler rule
        rule = "cxx" if str(src).endswith(('.cc', '.cpp', '.cxx')) else "cc"
        
        deps_str = " | " + " ".join(deps) if len(deps) > 0 else ""
        ninja_content += f"build {obj}: {rule} {escape_path(normalize_path(src))}{deps_str}\n"
        
        if rule == "cc":
            ninja_content += f"  cflags = {src_dbg_path} {cflags}\n"
        else:
            ninja_content += f"  cxxflags = {src_dbg_path} {cxxflags}\n"
        
        if module['has_g_file']:
            ninja_content += f"  # Dependencies from {module['name']}.g: {' '.join(module['dependencies'])}\n"
            if module['link_flags']:
                ninja_content += f"  # Link flags: {' '.join(module['link_flags'])}\n"
    
    # Shared library with collected link flags
    lib_name = f"lib/{platform['lib_prefix']}{project_name}{platform['lib_suffix']}"
    lib_flags = ' '.join(sorted(lib_link_flags)) if lib_link_flags else ''
    ninja_content += f"\n# Shared library\nbuild {lib_name}: link_shared {' '.join(lib_objects)}\n"
    if lib_flags:
        ninja_content += f"  libs = {lib_flags}\n"
    
    # Executables with dependency tracking
    ninja_content += "\n# Executables\n"
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
        deps_str = " | " + " ".join(deps) if len(deps) > 0 else ""
        ninja_content += f"build {obj}: {rule} {escape_path(normalize_path(src))}{deps_str}\n"
        
        if module['has_g_file']:
            ninja_content += f"  # Dependencies from {app_name}.g: {' '.join(module['dependencies'])}\n"
        
        # Executable with link flags from .g file
        exe_libs = ' '.join(module['link_flags']) if module['link_flags'] else ''
        ninja_content += f"build {exe}: link_exe {obj} | {lib_name}\n"
        if exe_libs:
            ninja_content += f"  libs = {exe_libs}\n"
    
    # Default target
    ninja_content += f"\n# Default target\nbuild all: phony {lib_name} {' '.join(exe_targets)}\ndefault all\n"
    
    # Write build.ninja
    build_ninja = build_dir / "build.ninja"
    build_dir.mkdir(exist_ok=True)
    
    with open(build_ninja, 'w') as f:
        f.write(ninja_content)
    
    print(f"Generated {build_ninja}")
    return build_ninja

def run_ninja_build(ninja_file):
    """Run the ninja build"""
    import subprocess
    
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