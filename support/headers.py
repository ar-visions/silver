#!/usr/bin/env python3
import os
import sys
import re
import tempfile
from pathlib import Path
import subprocess
import argparse

'''
def get_env_vars():
    """Get required environment variables"""
    return {
        'PROJECT_PATH': os.environ.get('PROJECT_PATH',  ''),
        'DIRECTIVE':    os.environ.get('DIRECTIVE',     ''),
        'BUILD_PATH':   os.environ.get('BUILD_PATH',    ''),
        'PROJECT':      os.environ.get('PROJECT',       ''),
        'IMPORT':       os.environ.get('IMPORT',        '')
    }
'''

def get_env_vars():
    """Get required variables from command line arguments"""
    parser = argparse.ArgumentParser(description='Generate header files')
    parser.add_argument('--project-path', required=True, help='Project root path')
    parser.add_argument('--directive', required=True, help='Directive (src or app)')
    parser.add_argument('--build-path', required=True, help='Build output path')
    parser.add_argument('--project', required=True, help='Project name')
    parser.add_argument('--import', required=True, help='Import path')
    
    args = parser.parse_args()
    
    return {
        'PROJECT_PATH': args.project_path,
        'DIRECTIVE': args.directive,
        'BUILD_PATH': args.build_path,
        'PROJECT': args.project,
        'IMPORT': getattr(args, 'import')  # 'import' is a Python keyword
    }

def setup_paths(env_vars):
    """Setup and validate paths"""
    project_path  = env_vars['PROJECT_PATH']
    directive     = env_vars['DIRECTIVE']
    build_path    = env_vars['BUILD_PATH']
    
    build_file    = os.path.join(project_path, "build.sf")
    src_directive = os.path.join(project_path, directive)
    
    # print(f"path is {os.environ.get('PATH', '')}")
    
    if not os.path.isdir(src_directive):
        print(f"skipping directive {directive} (null-path: {src_directive})")
        sys.exit(0)
    
    gen_dir = os.path.join(build_path, directive)
    os.makedirs(gen_dir, exist_ok=True)
    
    return {
        'build_file': build_file,
        'src_directive': src_directive,
        'gen_dir': gen_dir,
        'directive': directive,
        'project': env_vars['PROJECT'],
        'import_path': env_vars['IMPORT']
    }

def write_import_header(module, paths, env_vars):
    """Generate import header for a module"""
    src_directive = paths['src_directive']
    build_path = env_vars['BUILD_PATH']
    directive = paths['directive']
    project = paths['project']
    
    # Handle graph file and imports
    graph_file = os.path.join(src_directive, f"{module}.g")
    
    if module == "A":
        imports = ["A"]
    else:
        if os.path.isfile(graph_file):
            with open(graph_file, 'r') as f:
                graph_content = f.read().strip()
            sp = graph_content.split()
            graph_nodes = []
            for g in sp:
                if not g.startswith('-l'):
                    graph_nodes.append(g)
            imports = [module] + graph_nodes
        else:
            imports = ["A", module]
    
    # Add project to imports for app directive
    if directive == "app" and project not in imports:
        imports.append(project)
    
    import_header = os.path.join(build_path, directive, module, "import")
    os.makedirs(os.path.dirname(import_header), exist_ok=True)
    
    # Remove existing file
    if os.path.lexists(import_header):
        os.remove(import_header)
    
    build_file = paths['build_file']
    if (not os.path.exists(import_header) or 
        (os.path.exists(build_file) and os.path.getmtime(build_file) > os.path.getmtime(import_header))):
        
        with open(import_header, 'w') as f:
            f.write(f"/* generated {project} import for {{{directive}}} */\n")
            f.write("#ifndef _IMPORT_ // only one per compilation unit\n")
            f.write("#define _IMPORT_\n")
            
            # First pass: public headers
            for imp in imports:
                if imp != module:
                    f.write(f"#include <{imp}/public>\n")
            
            # Second pass: module headers
            for imp in imports:
                if imp != module:
                    f.write(f"#include <{imp}/{imp}>\n")
            
            # Module-specific includes
            f.write(f"#include <{module}/intern>\n")
            f.write(f"#include <{module}/{module}>\n")
            f.write(f"#include <{module}/methods>\n")
            f.write("#undef init\n")
            f.write("#undef dealloc\n")
            
            # Third pass: init headers
            for imp in imports:
                if imp != module:
                    f.write(f"#include <{imp}/init>\n")
            
            # Fourth pass: methods headers
            for imp in imports:
                if imp != module:
                    f.write(f"#include <{imp}/methods>\n")
            
            f.write(f"#include <{module}/init>\n")
            f.write("\n")
            f.write("#endif\n")

def process_utility_headers(paths):
    """Process utility headers (.h files)"""
    src_directive = paths['src_directive']
    gen_dir = paths['gen_dir']
    project = paths['project']
    
    # Find all .h files
    for h_file in Path(src_directive).glob("*.h"):
        module = h_file.name
        header_path = os.path.join(gen_dir, project, module)
        
        os.makedirs(os.path.dirname(header_path), exist_ok=True)
        
        # Remove existing and create symlink
        if os.path.lexists(header_path):
            os.remove(header_path)
        os.symlink(str(h_file), header_path)

def find_declarations(header_file, pattern):
    """Find declarations matching a pattern in the header file"""
    with open(header_file, 'r') as f:
        content = f.read()
    
    matches = re.findall(pattern, content)
    return matches

def generate_init_header(module, header_file, init_header):
    """Generate init header with class/struct declarations"""
    umodule = module.upper().replace('-', '_')
    
    with open(init_header, 'w') as f:
        f.write("/* generated methods interface */\n")
        f.write(f"#ifndef _{umodule}_INIT_H_\n")
        f.write(f"#define _{umodule}_INIT_H_\n")
        f.write("\n")
        
        # Process class declarations
        class_pattern = r'declare_(class|class_2|class_3|vector)\s*\(\s*([^,)]*)'
        matches = find_declarations(header_file, class_pattern)
        
        for match in matches:
            decl_type, class_name = match
            class_name = class_name.strip()
            
            if not class_name:
                continue
            
            # Generate TC macro
            f.write(f"#define TC_{class_name}(MEMBER, VALUE) ({{ AF_set(&instance->f, FIELD_ID({class_name}, MEMBER)); VALUE; }})\n")
            
            # Variadic argument counting macros
            f.write(f"#define _ARG_COUNT_IMPL_{class_name}(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N\n")
            f.write(f"#define _ARG_COUNT_I_{class_name}(...) _ARG_COUNT_IMPL_{class_name}(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)\n")
            f.write(f"#define _ARG_COUNT_{class_name}(...)   _ARG_COUNT_I_{class_name}(\"A-type\", ## __VA_ARGS__)\n")
            
            # Combination macros
            f.write(f"#define _COMBINE_{class_name}_(A, B)   A##B\n")
            f.write(f"#define _COMBINE_{class_name}(A, B)    _COMBINE_{class_name}_(A, B)\n")
            
            # Argument handling macros
            f.write(f"#define _N_ARGS_{class_name}_0( TYPE)\n")
            
            if decl_type in ["meta", "vector"]:
                f.write(f"#define _N_ARGS_{class_name}_1( TYPE, a)\n")
            else:
                f.write(f"#define _N_ARGS_{class_name}_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, A) A_schema(A, GENERICS, A) const void *: (void)0)(instance, a)\n")
            
            # Property assignment macros for various argument counts
            for i in range(2, 24, 2):
                if i == 2:
                    f.write(f"#define _N_ARGS_{class_name}_2( TYPE, a,b) instance->a = TC_{class_name}(a,b);\n")
                else:
                    prev_i = i - 2
                    args = []
                    for j in range(0, i, 2):
                        letter = chr(ord('a') + j)
                        next_letter = chr(ord('a') + j + 1)
                        args.append(f"{letter},{next_letter}")
                    
                    prev_args = ", ".join(args[:prev_i//2])
                    new_arg = args[prev_i//2]
                    new_var = new_arg.split(',')[0]
                    
                    f.write(f"#define _N_ARGS_{class_name}_{i}( TYPE, {', '.join(args)}) _N_ARGS_{class_name}_{prev_i}(TYPE, {prev_args}) instance->{new_var} = TC_{class_name}({new_arg});\n")
            
            # Helper macros
            f.write(f"#define _N_ARGS_HELPER2_{class_name}(TYPE, N, ...)  _COMBINE_{class_name}(_N_ARGS_{class_name}_, N)(TYPE, ## __VA_ARGS__)\n")
            f.write(f"#define _N_ARGS_{class_name}(TYPE,...)    _N_ARGS_HELPER2_{class_name}(TYPE, _ARG_COUNT_{class_name}(__VA_ARGS__), ## __VA_ARGS__)\n")
            
            # Main constructor macro
            f.write(f"#define {class_name}(...) ({{ \\\n")
            f.write(f"    {class_name} instance = ({class_name})alloc_dbg(typeid({class_name}), 1, __FILE__, __LINE__); \\\n")
            f.write(f"    _N_ARGS_{class_name}({class_name}, ## __VA_ARGS__); \\\n")
            f.write(f"    A_initialize((A)instance); \\\n")
            f.write(f"    instance; \\\n")
            f.write(f"}})\n")
        
        # Process struct declarations
        struct_pattern = r'declare_struct\s*\(\s*([^,)]*)'
        struct_matches = find_declarations(header_file, struct_pattern)
        
        for match in struct_matches:
            struct_name = match.strip() if isinstance(match, str) else match[0].strip()
            if struct_name:
                f.write(f"#define {struct_name}(...) structure_of({struct_name} __VA_OPT__(,) __VA_ARGS__) _N_STRUCT_ARGS({struct_name}, __VA_ARGS__);\n")
        
        f.write(f"\n#endif /* _{module}_INIT_H_ */\n")

def generate_methods_header(module, header_file, methods_header):
    """Generate methods header"""
    umodule = module.upper().replace('-', '_')
    
    with open(methods_header, 'w') as f:
        f.write("/* generated methods */\n")
        f.write(f"#ifndef _{umodule}_METHODS_H_\n")
        f.write(f"#define _{umodule}_METHODS_H_\n")
        f.write("\n")
        
        # Track processed methods to avoid duplicates
        processed_methods = set()
        
        # Process i_guard methods
        guard_pattern = r'i_guard\s*\([^,]*,[^,]*,[^,]*,[^,]*,\s*([a-zA-Z_][a-zA-Z0-9_]*)'
        guard_matches = find_declarations(header_file, guard_pattern)
        
        for method in guard_matches:
            method = method.strip()
            if method and method not in processed_methods:
                processed_methods.add(method)
                f.write(f"#undef {method}\n")
                f.write(f"#define {method}(I,...) ({{ __typeof__(I) _i_ = I; (((A)_i_ != (A)0L) ? ftableI(_i_)->{method}(_i_, ## __VA_ARGS__) : (__typeof__(ftableI(_i_)->{method}(_i_, ## __VA_ARGS__)))0) ; }})\n")
        
        # Process i_method methods
        method_pattern = r'i_method\s*\([^,]*,[^,]*,[^,]*,[^,]*,\s*([a-zA-Z_][a-zA-Z0-9_]*)'
        method_matches = find_declarations(header_file, method_pattern)
        
        for method in method_matches:
            method = method.strip()
            if method and method not in processed_methods:
                processed_methods.add(method)
                f.write(f"#ifndef {method}\n")
                f.write(f"#define {method}(I,...) ({{ __typeof__(I) _i_ = I; ftableI(_i_)->{method}(_i_, ## __VA_ARGS__); }})\n")
                f.write(f"#endif\n")
        
        # Process i_vargs methods
        vargs_pattern = r'i_vargs\s*\([^,]*,[^,]*,[^,]*,[^,]*,\s*([a-zA-Z_][a-zA-Z0-9_]*)'
        vargs_matches = find_declarations(header_file, vargs_pattern)
        
        for method in vargs_matches:
            method = method.strip()
            if method and method not in processed_methods:
                processed_methods.add(method)
                f.write(f"#ifndef {method}\n")
                f.write(f"#define {method}(I,...) ({{ __typeof__(I) _i_ = I; ftableI(_i_)->{method}(_i_, ## __VA_ARGS__); }})\n")
                f.write(f"#endif\n")
        
        f.write(f"\n#endif /* _{umodule}_METHODS_H_ */\n")

def generate_public_header(module, header_file, public_header):
    """Generate public header"""
    umodule = module.upper().replace('-', '_')
    
    with open(public_header, 'w') as f:
        f.write(f"/* generated {umodule} methods */\n")
        f.write(f"#ifndef _{umodule}_PUBLIC_\n")
        f.write(f"#define _{umodule}_PUBLIC_\n")
        f.write("\n")
        
        # Process all class types
        for class_type in ["class", "class_2", "class_3", "class_4", "struct"]:
            pattern = f'declare_{class_type}\\s*\\(\\s*([^,)]*)'
            matches = find_declarations(header_file, pattern)
            
            for match in matches:
                class_name = match.strip() if isinstance(match, str) else match[0].strip()
                if class_name:
                    f.write(f"#ifndef {class_name}_intern\n")
                    f.write(f"#define {class_name}_intern(AA,YY,...) AA##_schema(AA,YY##_EXTERN, __VA_ARGS__)\n")
                    f.write(f"#endif\n")
            f.write("\n")
        
        f.write(f"#endif /* _{umodule}_PUBLIC_ */\n")

def generate_intern_header(module, header_file, intern_header):
    """Generate intern header"""
    umodule = module.upper().replace('-', '_')
    
    with open(intern_header, 'w') as f:
        f.write(f"/* generated {umodule} interns */\n")
        f.write(f"#ifndef _{umodule}_INTERN_H_\n")
        f.write(f"#define _{umodule}_INTERN_H_\n")
        f.write("\n")
        
        # Process class types (not struct)
        for class_type in ["class", "class_2", "class_3", "class_4"]:
            pattern = f'declare_{class_type}\\s*\\(\\s*([^,)]*)'
            matches = find_declarations(header_file, pattern)
            
            for match in matches:
                class_name = match.strip() if isinstance(match, str) else match[0].strip()
                if class_name:
                    f.write(f"#undef {class_name}_intern\n")
                    f.write(f"#define {class_name}_intern(AA,YY,...) AA##_schema(AA,YY, __VA_ARGS__)\n")
            f.write("\n")
        
        f.write(f"#endif /* _{umodule}_INTERN_H_ */\n")


def process_modules(paths):
    """Process all modules (files without extension OR .g files)"""
    src_directive = paths['src_directive']
    gen_dir = paths['gen_dir']
    
    # Find modules from both extensionless files and .g files
    module_names = find_modules_for_headers(src_directive)
    
    for module in module_names:
        umodule = module.upper().replace('-', '_')
        
        # Check if extensionless file exists, otherwise look for .g file
        module_file = Path(src_directive) / module
        if not module_file.exists():
            module_file = Path(src_directive) / f"{module}.g"
        
        if not module_file.exists():
            continue  # Skip if neither exists
        
        # Generate header paths
        methods_header = os.path.join(gen_dir, module, "methods")
        intern_header = os.path.join(gen_dir, module, "intern")
        public_header = os.path.join(gen_dir, module, "public")
        init_header = os.path.join(gen_dir, module, "init")
        
        write_import_header(module, paths, get_env_vars())
        
        # Create symlink to source (prefer extensionless, fallback to .g)
        source_file = Path(src_directive) / module
        if not source_file.exists():
            source_file = Path(src_directive) / f"{module}.g"
            
        header_path = os.path.join(gen_dir, module, module)
        os.makedirs(os.path.dirname(header_path), exist_ok=True)
        if os.path.exists(header_path):
            os.remove(header_path)
        os.symlink(str(source_file), header_path)
        
        # C-compatible upper-case definition name
        umodule = module.upper().replace('-', '_')
        
        # Generate headers if needed
        if (not os.path.exists(init_header) or 
            os.path.getmtime(str(source_file)) > os.path.getmtime(init_header)):
            generate_init_header(module, str(source_file), init_header)
        
        if (not os.path.exists(methods_header) or 
            os.path.getmtime(str(source_file)) > os.path.getmtime(methods_header)):
            generate_methods_header(module, str(source_file), methods_header)
        
        if (not os.path.exists(public_header) or 
            os.path.getmtime(str(source_file)) > os.path.getmtime(public_header)):
            generate_public_header(module, str(source_file), public_header)
        
        if (not os.path.exists(intern_header) or 
            os.path.getmtime(str(source_file)) > os.path.getmtime(intern_header)):
            generate_intern_header(module, str(source_file), intern_header)

def handle_src_directive(paths, env_vars):
    """Handle src directive symlink creation"""
    if paths['directive'] == "src":
        src_directive = paths['src_directive']
        gen_dir = paths['gen_dir']
        import_path = env_vars['IMPORT']
        
        for module_file in Path(src_directive).iterdir():
            if module_file.is_file() and '.' not in module_file.name:
                module = module_file.name
                install_include_dir = os.path.join(import_path, "include", module)
                module_gen_dir = os.path.join(gen_dir, module) 
                if os.path.lexists(install_include_dir):  # handles broken symlinks too
                    os.remove(install_include_dir)
                os.symlink(module_gen_dir, install_include_dir)

def find_modules_for_headers(src_directive):
    """Find all modules that need headers generated (extensionless files)"""
    modules = set()
    
    for file_path in Path(src_directive).iterdir():
        if file_path.is_file():
            name = file_path.name
            if '.' not in name:
                # Extensionless file
                if name not in modules:
                    modules.add(name)
    
    return list(modules)

def main():
    """Main function"""
    env_vars = get_env_vars()
    paths = setup_paths(env_vars)
    
    # Process utility headers (.h files)
    process_utility_headers(paths)
    

    # Process modules (files without extension)
    process_modules(paths)
    
    # Handle src directive
    handle_src_directive(paths, env_vars)
    
    # Return include path
    print(f"-I{paths['gen_dir']}")

if __name__ == "__main__":
    main()