#include <A>

map parse_cmd_args(int argc, symbol argv[], map default_values, object default_key) {
    map res = new(map);
    for (item ii = default_values->refs->first; ii; ii = ii->next) {
        item  hm = ii->val;
        object k = hm->key;
        object v = hm->val;
        call(res, set, k, v);
    }

    int i = 1;
    while (i < argc) {
        symbol arg = argv[i];
        if (!arg) {
            i++;
            continue;
        }
        if (arg[0] == '-') {
            // -s or --silver
            bool32 doub  = arg[1] == '-';
            string s_arg = ctr(string, cstr, &arg[doub + 1], -1);
        }
        //object v = call(defs, get, arg);

    }  
}

int main(int argc, char **argv) {
    A_start();
}