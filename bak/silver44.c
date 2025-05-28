/*#include <silver44.h>

// Helper function to replace all occurrences of a character in a string
static string string_replace_char(string s, char old_char, char new_char) {
    string result = ctr(string, cstr, s->chars, s->len);
    for (int i = 0; i < result->len; i++) {
        if (result->chars[i] == old_char) {
            result->chars[i] = new_char;
        }
    }
    return result;
}

// Helper function to convert a string to uppercase
static string string_to_upper(string s) {
    string result = ctr(string, cstr, s->chars, s->len);
    for (int i = 0; i < result->len; i++) {
        result->chars[i] = toupper(result->chars[i]);
    }
    return result;
}

static string change_ext(string f, string ext_to) {
    path p = ctr(path, cstr, f->chars, f->len);
    string stem = call(p, stem);
    string result = ctr(string, cstr, "", 0);
    call(result, append, stem->chars);
    call(result, append, ".");
    call(result, append, ext_to->chars);
    return result;
}

static string format_identifier(string text) {
    string u = string_to_upper(text);
    string f = string_replace_char(u, ' ', '_');
    f = string_replace_char(f, ',', '_');
    f = string_replace_char(f, '-', '_');
    
    while (call(f, index_of, "__") != -1) {
        string temp = string_replace_char(f, '_', ' ');
        array parts = call(temp, split, " ");
        string joined = ctr(string, cstr, "", 0);
        for (int i = 0; i < call(parts, count); i++) {
            string part = call(parts, get, i);
            if (part->len > 0) {
                if (joined->len > 0) {
                    call(joined, append, "_");
                }
                call(joined, append, part->chars);
            }
        }
        f = joined;
    }
    
    return f;
}



// Global variable
static string build_root;

// Helper function to create a path
static path create_path(string dir) {
    return ctr(path, cstr, dir->chars, dir->len);
}

// Helper function to join paths
static string path_join(string a, string b) {
    string result = ctr(string, cstr, a->chars, a->len);
    call(result, append, "/");
    call(result, append, b->chars);
    return result;
}

static i32 contains_main(string obj_file) {
    string command = format("nm %s", obj_file->chars);
    FILE* pipe = popen(command->chars, "r");
    if (!pipe) {
        return -1;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        if (strstr(buffer, " T main") != NULL) {
            pclose(pipe);
            return 1;
        }
    }

    i32 result = pclose(pipe);
    if (result == -1) {
        print("Error running nm: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static path install_dir() {
    string install_dir_str = path_join(build_root, ctr(string, cstr, "install", -1));
    path i = create_path(install_dir_str);
    call(i, make_dir);
    return i;
}

static i32 cmd(string command) {
    print("> %o", command);
    
    i32 result = system(command->chars);
    
    if (result == -1) {
        string error_msg = ctr(string, cstr, strerror(errno), -1);
        print("error executing command: %s\n", error_msg->chars);
        return -1;
    }
    
    return result;
}

static path folder_path(path p) {
    bool is_dir = call(p, is_dir);
    if (is_dir) {
        return p;
    } else {
        return call(p, parent);
    }
}


static EContext EContext_with_EModule(EContext self, EModule module, EMember method) {
    self->module = module;
    self->method = method;
    self->states = ctr(array, sz, 0);
    self->raw_primitives = false;
    self->values = ctr(map, sz, 0);
    self->indent_level = 0;
    return self;
}

define_class(EContext, A)

*/

int main(int n_args, const char* args[]) {
    return 0;
}