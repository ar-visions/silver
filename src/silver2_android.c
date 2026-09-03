// silver2 android: the apk (binary manifest, aligned zip, v2 signature
// via openssl), the NativeActivity host, the emulator and adb. Included
// by silver2.c: it shares that translation unit.

// ---- android: an apk is a zip with a binary manifest and a v2
// signature block, written here; openssl does the rsa part
typedef struct {
    u8*    data;
    size_t count, capacity;
} Bytes;
static void bput(Bytes* b, const void* d, size_t n) {
    if (b->count + n > b->capacity) {
        b->capacity = (b->count + n) * 2 + 1024;
        b->data     = realloc(b->data, b->capacity);
    }
    memcpy(b->data + b->count, d, n);
    b->count += n;
}
static void b16(Bytes* b, u32 v) {
    u8 d[2] = {(u8)v, (u8)(v >> 8)};
    bput(b, d, 2);
}
static void b32(Bytes* b, u32 v) {
    u8 d[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
    bput(b, d, 4);
}
static void b64(Bytes* b, u64 v) {
    b32(b, (u32)v);
    b32(b, (u32)(v >> 32));
}
static void bfix32(Bytes* b, size_t at, u32 v) {
    u8* d = b->data + at;
    d[0]  = v;
    d[1]  = v >> 8;
    d[2]  = v >> 16;
    d[3]  = v >> 24;
}
static void bpad(Bytes* b, size_t al) {
    u8 z = 0;
    while (b->count % al) bput(b, &z, 1);
}
static bool bload(Bytes* b, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    u8     buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) bput(b, buf, n);
    fclose(f);
    return true;
}
static u32 crc32_of(const u8* d, size_t n) {
    static u32 table[256];
    if (!table[1])
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    u32 c = 0xffffffffu;
    for (size_t i = 0; i < n; i++)
        c = table[(c ^ d[i]) & 0xff] ^ (c >> 8);
    return ~c;
}
static void sha256(const u8* d, size_t n, u8 out[32]) {
    static const u32 K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
        0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
        0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
        0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
        0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
        0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
        0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
        0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    u32    h[8]  = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t total = ((n + 9 + 63) / 64) * 64;
    u8*    m     = calloc(1, total);
    memcpy(m, d, n);
    m[n]     = 0x80;
    u64 bits = (u64)n * 8;
    for (int i = 0; i < 8; i++)
        m[total - 1 - i] = (u8)(bits >> (8 * i));
#define R(x, k) (((x) >> (k)) | ((x) << (32 - (k))))
    for (size_t off = 0; off < total; off += 64) {
        u32 w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (u32)m[off + 4 * i] << 24 |
                   (u32)m[off + 4 * i + 1] << 16 |
                   (u32)m[off + 4 * i + 2] << 8 | m[off + 4 * i + 3];
        for (int i = 16; i < 64; i++) {
            u32 s0 =
                R(w[i - 15], 7) ^ R(w[i - 15], 18) ^ (w[i - 15] >> 3);
            u32 s1 =
                R(w[i - 2], 17) ^ R(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        u32 a = h[0], b = h[1], c = h[2], dd = h[3], e = h[4], f = h[5],
            g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            u32 t1 = hh + (R(e, 6) ^ R(e, 11) ^ R(e, 25)) +
                     ((e & f) ^ (~e & g)) + K[i] + w[i];
            u32 t2 = (R(a, 2) ^ R(a, 13) ^ R(a, 22)) +
                     ((a & b) ^ (a & c) ^ (b & c));
            hh = g;
            g  = f;
            f  = e;
            e  = dd + t1;
            dd = c;
            c  = b;
            b  = a;
            a  = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += dd;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }
#undef R
    free(m);
    for (int i = 0; i < 8; i++) {
        out[4 * i]     = h[i] >> 24;
        out[4 * i + 1] = h[i] >> 16;
        out[4 * i + 2] = h[i] >> 8;
        out[4 * i + 3] = h[i];
    }
}
typedef struct {
    const char* text;
    u32         id;
} AxmlString; // the binary xml android reads: a string pool
              // (resource-id names first), the id map, the element
              // chunks
typedef struct {
    AxmlString strings[64];
    int        count, id_count;
    Bytes      body;
} Axml;
typedef struct {
    const char* name;
    int         type;
    const char* text;
    u32         value;
} AxmlAttr;
enum { AXML_STRING = 3, AXML_INT = 0x10, AXML_BOOL = 0x12 };
static int axml_index(Axml* x, const char* text) {
    for (int i = 0; i < x->count; i++)
        if (same(x->strings[i].text, text)) return i;
    x->strings[x->count].text = text;
    return x->count++;
}
static void axml_elem(Axml* x, const char* name, AxmlAttr* attrs,
                      int n) {
    Bytes* b = &x->body;
    b32(b, 0x00100102);
    b32(b, 36 + 20 * n);
    b32(b, 0);
    b32(b, 0xffffffff);
    b32(b, 0xffffffff);
    b32(b, axml_index(x, name));
    b16(b, 20);
    b16(b, 20);
    b16(b, n);
    b16(b, 0);
    b16(b, 0);
    b16(b, 0);
    for (int i = 0; i < n; i++) {
        int ni = axml_index(x, attrs[i].name);
        b32(b,
            ni < x->id_count
                ? axml_index(
                      x, "http://schemas.android.com/apk/res/android")
                : 0xffffffff);
        b32(b, ni);
        int raw = attrs[i].type == AXML_STRING
                      ? axml_index(x, attrs[i].text)
                      : -1;
        b32(b, raw);
        b16(b, 8);
        bput(b, "\0", 1);
        bput(b, &(u8){attrs[i].type}, 1);
        b32(b, attrs[i].type == AXML_STRING ? (u32)raw
               : attrs[i].type == AXML_BOOL
                   ? (attrs[i].value ? 0xffffffff : 0)
                   : attrs[i].value);
    }
}
static void axml_end(Axml* x, const char* name) {
    Bytes* b = &x->body;
    b32(b, 0x00100103);
    b32(b, 24);
    b32(b, 0);
    b32(b, 0xffffffff);
    b32(b, 0xffffffff);
    b32(b, axml_index(x, name));
}
static void axml_write(Axml* x, Bytes* out) {
    int prefix = axml_index(x, "android"),
        uri =
            axml_index(x, "http://schemas.android.com/apk/res/android");
    Bytes pool = {0};
    for (int i = 0; i < x->count; i++) b32(&pool, 0);
    for (int i = 0; i < x->count; i++) {
        bfix32(&pool, 4 * i, (u32)(pool.count - 4 * x->count));
        const char* t = x->strings[i].text;
        b16(&pool, (u32)strlen(t));
        for (const char* c = t; *c; c++) b16(&pool, (u8)*c);
        b16(&pool, 0);
    }
    bpad(&pool, 4);
    Bytes ns = {0};
    b32(&ns, 0x00100100);
    b32(&ns, 24);
    b32(&ns, 0);
    b32(&ns, 0xffffffff);
    b32(&ns, prefix);
    b32(&ns, uri);
    b32(out, 0x00080003);
    b32(out, 0);
    b16(out, 1);
    b16(out, 28);
    b32(out, 28 + pool.count);
    b32(out, x->count);
    b32(out, 0);
    b32(out, 0);
    b32(out, 28 + 4 * x->count);
    b32(out, 0);
    bput(out, pool.data, pool.count);
    b16(out, 0x180);
    b16(out, 8);
    b32(out, 8 + 4 * x->id_count);
    for (int i = 0; i < x->id_count; i++) b32(out, x->strings[i].id);
    bput(out, ns.data, ns.count);
    bput(out, x->body.data, x->body.count);
    ns.data[0] = 0x01;
    bput(out, ns.data, ns.count);
    bfix32(out, 4, out->count);
    free(pool.data);
    free(ns.data);
}
static void android_manifest(Bytes* out, const char* version) {
    Axml       x     = {0};
    AxmlString ids[] = {{"label", 0x01010001},
                        {"name", 0x01010003},
                        {"hasCode", 0x0101000c},
                        {"debuggable", 0x0101000f},
                        {"exported", 0x01010010},
                        {"launchMode", 0x0101001d},
                        {"configChanges", 0x0101001f},
                        {"value", 0x01010024},
                        {"minSdkVersion", 0x0101020c},
                        {"targetSdkVersion", 0x01010270},
                        {"versionCode", 0x0101021b},
                        {"versionName", 0x0101021c},
                        {"extractNativeLibs", 0x010104ea}};
    for (int i = 0; i < 13; i++) x.strings[x.count++] = ids[i];
    x.id_count          = x.count;
    const char* name    = S->modname;
    char*       package = format("com.silver.%s", name);
    char*       host    = format("%s-host", name);
    axml_elem(&x, "manifest",
              (AxmlAttr[]){{"versionCode", AXML_INT, 0, 1},
                           {"versionName", AXML_STRING, version},
                           {"package", AXML_STRING, package}},
              3);
    axml_elem(&x, "uses-sdk",
              (AxmlAttr[]){{"minSdkVersion", AXML_INT, 0, 33},
                           {"targetSdkVersion", AXML_INT, 0, 34}},
              2);
    axml_end(&x, "uses-sdk");
    axml_elem(&x, "uses-permission",
              (AxmlAttr[]){
                  {"name", AXML_STRING, "android.permission.INTERNET"}},
              1);
    axml_end(&x, "uses-permission");
    axml_elem(&x, "application",
              (AxmlAttr[]){{"label", AXML_STRING, name},
                           {"hasCode", AXML_BOOL, 0, 0},
                           {"debuggable", AXML_BOOL, 0, 1},
                           {"extractNativeLibs", AXML_BOOL, 0, 1}},
              4);
    axml_elem(&x, "activity",
              (AxmlAttr[]){
                  {"label", AXML_STRING, name},
                  {"name", AXML_STRING, "android.app.NativeActivity"},
                  {"exported", AXML_BOOL, 0, 1},
                  {"launchMode", AXML_INT, 0, 2},
                  {"configChanges", AXML_INT, 0, 0x17a0}},
              5);
    axml_elem(
        &x, "meta-data",
        (AxmlAttr[]){{"name", AXML_STRING, "android.app.lib_name"},
                     {"value", AXML_STRING, host}},
        2);
    axml_end(&x, "meta-data");
    axml_elem(&x, "intent-filter", 0, 0);
    axml_elem(&x, "action",
              (AxmlAttr[]){
                  {"name", AXML_STRING, "android.intent.action.MAIN"}},
              1);
    axml_end(&x, "action");
    axml_elem(&x, "category",
              (AxmlAttr[]){{"name", AXML_STRING,
                            "android.intent.category.LAUNCHER"}},
              1);
    axml_end(&x, "category");
    axml_end(&x, "intent-filter");
    axml_end(&x, "activity");
    axml_end(&x, "application");
    axml_end(&x, "manifest");
    axml_write(&x, out);
    free(x.body.data);
}
static void
apk_add(Bytes* zip, Bytes* cd, const char* name, const u8* d, size_t n,
        size_t align) { // one stored entry, its data aligned so .so
                        // files map straight from the zip
    u32    crc = crc32_of(d, n);
    size_t at = zip->count, nlen = strlen(name),
           pad = (align - (at + 30 + nlen + 6) % align) % align;
    b32(zip, 0x04034b50);
    b16(zip, 10);
    b16(zip, 0);
    b16(zip, 0);
    b16(zip, 0);
    b16(zip, 0x21);
    b32(zip, crc);
    b32(zip, n);
    b32(zip, n);
    b16(zip, nlen);
    b16(zip, 6 + pad);
    bput(zip, name, nlen);
    b16(zip, 0xd935);
    b16(zip, 2 + pad);
    b16(zip, align);
    for (size_t i = 0; i < pad; i++) bput(zip, "\0", 1);
    bput(zip, d, n);
    b32(cd, 0x02014b50);
    b16(cd, 20);
    b16(cd, 10);
    b16(cd, 0);
    b16(cd, 0);
    b16(cd, 0);
    b16(cd, 0x21);
    b32(cd, crc);
    b32(cd, n);
    b32(cd, n);
    b16(cd, nlen);
    b16(cd, 0);
    b16(cd, 0);
    b16(cd, 0);
    b16(cd, 0);
    b32(cd, 0);
    b32(cd, at);
    bput(cd, name, nlen);
}
static void apk_eocd(Bytes* b, int entries, size_t cd_size,
                     size_t cd_off) {
    b32(b, 0x06054b50);
    b16(b, 0);
    b16(b, 0);
    b16(b, entries);
    b16(b, entries);
    b32(b, cd_size);
    b32(b, cd_off);
    b16(b, 0);
}
static void apk_chunks(Bytes* digests, const u8* d, size_t n,
                       int* count) {
    for (size_t off = 0; off < n; off += 1048576) {
        size_t len = n - off < 1048576 ? n - off : 1048576;
        Bytes  c   = {0};
        bput(&c, "\xa5", 1);
        b32(&c, len);
        bput(&c, d + off, len);
        u8 h[32];
        sha256(c.data, c.count, h);
        bput(digests, h, 32);
        free(c.data);
        (*count)++;
    }
}
static bool
apk_sign(const char* root, Bytes* zip, Bytes* cd, Bytes* out,
         int entries) { // the v2 block over the three zip sections; a
                        // key is made once per device dir
    char *key = format("%s/sign.key", root),
         *crt = format("%s/sign.crt", root),
         *tmp = format("%s/build", root);
    run_shell(format("mkdir -p %s", tmp));
    if (access(key, R_OK) &&
        run_shell(format(
            "openssl req -x509 -newkey rsa:2048 -nodes -days 10000 "
            "-subj /CN=silver -keyout %s -out %s 2>/dev/null",
            key, crt)))
        return false;
    Bytes der = {0}, pub = {0};
    if (run_shell(
            format("openssl x509 -in %s -outform DER -out %s/sign.der",
                   crt, tmp)) ||
        run_shell(format("openssl x509 -in %s -pubkey -noout | openssl "
                         "pkey -pubin -outform DER -out %s/sign.pub",
                         crt, tmp)) ||
        !bload(&der, format("%s/sign.der", tmp)) ||
        !bload(&pub, format("%s/sign.pub", tmp)))
        return false;
    Bytes eocd = {0};
    apk_eocd(&eocd, entries, cd->count, zip->count);
    Bytes chunks = {0};
    int   count  = 0;
    apk_chunks(&chunks, zip->data, zip->count, &count);
    apk_chunks(&chunks, cd->data, cd->count, &count);
    apk_chunks(&chunks, eocd.data, eocd.count, &count);
    Bytes top = {0};
    bput(&top, "\x5a", 1);
    b32(&top, count);
    bput(&top, chunks.data, chunks.count);
    u8 digest[32];
    sha256(top.data, top.count, digest);
    Bytes sd = {0};
    b32(&sd, 4 + 4 + 4 + 32);
    b32(&sd, 4 + 4 + 32);
    b32(&sd, 0x0103);
    b32(&sd, 32);
    bput(&sd, digest, 32);
    b32(&sd, 4 + der.count);
    b32(&sd, der.count);
    bput(&sd, der.data, der.count);
    b32(&sd, 0);
    char* sd_file = format("%s/sign.sd", tmp);
    FILE* sf      = fopen(sd_file, "wb");
    if (!sf) return false;
    fwrite(sd.data, 1, sd.count, sf);
    fclose(sf);
    Bytes sig = {0};
    if (run_shell(
            format("openssl dgst -sha256 -sign %s -out %s/sign.sig %s",
                   key, tmp, sd_file)) ||
        !bload(&sig, format("%s/sign.sig", tmp)))
        return false;
    Bytes signer = {0};
    b32(&signer, sd.count);
    bput(&signer, sd.data, sd.count);
    b32(&signer, 4 + 4 + 4 + sig.count);
    b32(&signer, 4 + 4 + sig.count);
    b32(&signer, 0x0103);
    b32(&signer, sig.count);
    bput(&signer, sig.data, sig.count);
    b32(&signer, pub.count);
    bput(&signer, pub.data, pub.count);
    Bytes v2 = {0};
    b32(&v2, 4 + signer.count);
    b32(&v2, signer.count);
    bput(&v2, signer.data, signer.count);
    size_t body = 8 + 4 + v2.count,
           pad  = (4096 - (8 + body + 8 + 16) % 4096) % 4096;
    if (pad && pad < 12) pad += 4096;
    size_t size = body + pad + 8 + 16;
    b64(out, size);
    b64(out, 4 + v2.count);
    b32(out, 0x7109871a);
    bput(out, v2.data, v2.count);
    if (pad) {
        b64(out, pad - 8);
        b32(out, 0x42726577);
        for (size_t i = 0; i < pad - 12; i++) bput(out, "\0", 1);
    }
    b64(out, size);
    bput(out, "APK Sig Block 42", 16);
    return true;
}
static void android_bundle_libs(
    const char* bin, Bytes* zip, Bytes* cd, List* done,
    int* entries) { // the .so closure of a binary: ours, the device lib
                    // dir's, the ndk's shared libc++
    char* root =
        format("%s/platform/%s", S->silver_root, S->target_dir);
    FILE* pipe = popen(
        format("%s/llvm-readelf --needed-libs %s", S->tools, bin), "r");
    char line[1024];
    List needed = {0};
    while (pipe && fgets(line, sizeof line, pipe)) {
        char* t = line;
        while (*t == ' ' || *t == '\t' || *t == '[') t++;
        char* e = t + strlen(t);
        while (e > t && (e[-1] == '\n' || e[-1] == ' ' || e[-1] == ']'))
            *--e = 0;
        if (strstr(t, ".so")) list_push(&needed, strdup(t));
    }
    if (pipe) pclose(pipe);
    for (int i = 0; i < needed.count; i++) {
        const char* leaf = needed.data[i];
        bool        seen = false;
        for (int k = 0; k < done->count; k++)
            if (same(done->data[k], leaf)) seen = true;
        if (seen || !access(format("%s/usr/lib/%s/33/%s", S->sysroot,
                                   android_abi_dir(), leaf),
                            R_OK))
            continue; // the api dir's are the system's
        const char* places[4] = {
            format("%s/install/lib/%s", S->out_dir, leaf),
            format("%s/lib/%s", root, leaf),
            format("%s/usr/lib/%s/%s", S->sysroot, android_abi_dir(),
                   leaf),
            0};
        const char* src = 0;
        for (int k = 0; places[k] && !src; k++)
            if (!access(places[k], R_OK)) src = places[k];
        if (!src) {
            char* found =
                shell_line(format("find %s/.. -name %s -path '*/%s/*' "
                                  "2>/dev/null | head -1",
                                  S->main_dir, leaf, S->target_dir));
            if (*found) src = found;
        }
        if (!src) {
            fprintf(stderr,
                    "[%s] android: %s not found for the package\n",
                    S->modname, leaf);
            continue;
        }
        list_push(done, strdup(leaf));
        Bytes d = {0};
        bload(&d, src);
        apk_add(zip, cd, format("lib/%s/%s", android_abi(), leaf),
                d.data, d.count, 16384);
        (*entries)++;
        free(d.data);
        android_bundle_libs(src, zip, cd, done, entries);
    }
}
static char* android_bundle(
    const char* product, const char* install_dir,
    const char* module_dir) { // <out>/<name>.apk: host, product and
                              // closure under lib/, share/<name> as
                              // assets, the manifest
    const char* name = S->modname;
    char*       root =
        format("%s/platform/%s", S->silver_root, S->target_dir);
    char*       apk     = format("%s/%s.apk", S->out_dir, name);
    const char* version = bundle_version(module_dir);
    fprintf(stderr, "[%s] android: staging %s\n", name, apk);
    const char* leaf = strrchr(product, '/') + 1;
    char*       host = format("%s/lib%s-host.so", S->out_dir, name);
    if (run_shell(format(
            "%s/clang -target %s --sysroot=%s -fuse-ld=lld -B%s %s %s "
            "-shared -fPIC -ftls-model=global-dynamic "
            "-Wl,-soname,lib%s-host.so -I%s/devices "
            "-DSILVER_PRODUCT='\"%s\"' -DSILVER_SHARE_NAME='\"%s\"' "
            "%s/src/silver-host-android.c %s -L%s/lib "
            "-L%s/usr/lib/%s/33 -L%s/usr/lib/%s -lAu -landroid -llog "
            "-o %s",
            S->tools, S->triple, S->sysroot, S->tools,
            S->release ? "-O2" : "-g", abi_link(), name, S->silver_root,
            leaf, name, S->silver_root, devices_lib("so"), root,
            S->sysroot, android_abi_dir(), S->sysroot,
            android_abi_dir(), host))) {
        fprintf(stderr, "android: host link failed\n");
        exit(1);
    }
    Bytes zip = {0}, cd = {0}, manifest = {0};
    int   entries = 0;
    android_manifest(&manifest, version);
    apk_add(&zip, &cd, "AndroidManifest.xml", manifest.data,
            manifest.count, 4);
    entries++;
    List  done = {0};
    Bytes d    = {0};
    bload(&d, host);
    apk_add(&zip, &cd,
            format("lib/%s/lib%s-host.so", android_abi(), name), d.data,
            d.count, 16384);
    entries++;
    free(d.data);
    list_push(&done, format("lib%s-host.so", name));
    d = (Bytes){0};
    bload(&d, product);
    apk_add(&zip, &cd, format("lib/%s/%s", android_abi(), leaf), d.data,
            d.count, 16384);
    entries++;
    free(d.data);
    list_push(&done, strdup(leaf));
    android_bundle_libs(product, &zip, &cd, &done, &entries);
    android_bundle_libs(host, &zip, &cd, &done, &entries);
    char* share = format(
        "%s/share/%s", install_dir,
        name); // the asset dir lists no subdirectories: a list goes
               // with it, and a stamp says when to extract again
    if (!access(share, R_OK)) {
        FILE* pipe = popen(
            format(
                "cd %s && find -L . -type f | sort | sed 's|^\\./||'",
                share),
            "r");
        char  rel[1024];
        Bytes list = {0};
        while (pipe && fgets(rel, sizeof rel, pipe)) {
            rel[strcspn(rel, "\n")] = 0;
            if (!*rel) continue;
            Bytes fd = {0};
            bload(&fd, format("%s/%s", share, rel));
            apk_add(&zip, &cd, format("assets/share/%s/%s", name, rel),
                    fd.data, fd.count, 4);
            entries++;
            free(fd.data);
            bput(&list, rel, strlen(rel));
            bput(&list, "\n", 1);
        }
        if (pipe) pclose(pipe);
        apk_add(&zip, &cd, "assets/share.list", list.data, list.count,
                4);
        entries++;
    }
    char* stamp = format("%s-%ld", version, (long)time(0));
    apk_add(&zip, &cd, "assets/share.stamp", (u8*)stamp, strlen(stamp),
            4);
    entries++;
    Bytes block = {0};
    if (!apk_sign(root, &zip, &cd, &block, entries)) {
        fprintf(stderr,
                "android: signing failed — is openssl installed?\n");
        exit(1);
    }
    FILE* f = fopen(apk, "wb");
    if (!f) {
        fprintf(stderr, "android: cannot write %s\n", apk);
        exit(1);
    }
    fwrite(zip.data, 1, zip.count, f);
    fwrite(block.data, 1, block.count, f);
    fwrite(cd.data, 1, cd.count, f);
    Bytes eocd = {0};
    apk_eocd(&eocd, entries, cd.count, zip.count + block.count);
    fwrite(eocd.data, 1, eocd.count, f);
    fclose(f);
    return apk;
}
static int android_run(void) { // adb finds the phone itself; host is a
                               // serial only when several are on
    const char* name = S->modname;
    const char* host = S->device.host;
    char*       apk  = format("%s/%s.apk", S->out_dir, name);
    char*       sdk =
        format("%s/platform/%s/sdk", S->silver_root, S->target_dir);
    char* adb = format("%s/platform-tools/adb%s%s", sdk,
                       host ? " -s " : "", host ? host : "");
    if (access(apk, R_OK)) {
        fprintf(stderr, "[%s] android: no package at %s\n", name, apk);
        return 1;
    }
    bool sim = strstr(S->platform, "sim") != 0;
    if (sim) { // the emulator: its avd is written the first time,
               // started when none runs, waited for until android
               // is up
        char* avd = format("%s/platform/%s/avd/silver.avd",
                           S->silver_root, S->target_dir);
        if (access(avd, R_OK)) {
            run_shell(format("mkdir -p %s", avd));
            const char* abi = android_abi();
            save_text(
                format("%s/../silver.ini", avd),
                format("avd.ini.encoding=UTF-8\npath=%s\npath.rel="
                       "avd/silver.avd\ntarget=android-34\n",
                       avd));
            save_text(
                format("%s/config.ini", avd),
                format("AvdId=silver\navd.ini.displayname=silver\navd."
                       "ini.encoding=UTF-8\nabi.type=%s\nhw.cpu.arch=%"
                       "s\nhw.cpu.ncore=4\nhw.ramSize=2048\n"
                       "image.sysdir.1=system-images/android-34/"
                       "google_apis/%s/"
                       "\ntag.id=google_apis\ntag.display=Google "
                       "APIs\nPlayStore.enabled=no\nhw.lcd.width="
                       "1080\nhw.lcd.height=2400\nhw.lcd.density=420\n"
                       "hw.gpu.enabled=yes\nhw.gpu.mode=host\nhw."
                       "keyboard=yes\nhw.sdCard=no\nhw.audioInput="
                       "no\ndisk.dataPartition.size=4G\nfastboot."
                       "forceColdBoot=no\n",
                       abi, same(abi, "x86_64") ? "x86_64" : "arm64",
                       abi));
        }
        if (run_shell(
                format("%s devices 2>/dev/null | grep -q '^emulator-'",
                       adb))) {
            fprintf(stderr, "[%s] starting the emulator\n", name);
            run_shell(
                format("ANDROID_SDK_ROOT=%s ANDROID_AVD_HOME=%s/.. "
                       "%s/emulator/emulator -avd silver -gpu auto "
                       "-no-boot-anim -no-snapshot-save > "
                       "%s/../emulator.log 2>&1 &",
                       sdk, avd, sdk, avd));
        }
        if (run_shell(format(
                "%s wait-for-device shell 'while [ \"$(getprop "
                "sys.boot_completed 2>/dev/null | tr -d \"\\r\")\" "
                "!= \"1\" ]; do sleep 1; done'",
                adb))) {
            fprintf(stderr,
                    "[%s] android: the emulator did not come up — "
                    "see platform/%s/emulator.log\n",
                    name, S->target_dir);
            return 1;
        }
    } else {
        run_shell(format("%s kill-server >/dev/null 2>&1", adb));
        if (!run_shell(format(
                "%s devices 2>/dev/null | grep -q 'no permissions'",
                adb))) {
            fprintf(stderr,
                    "[%s] android: the phone's usb node is "
                    "root-only (no udev rule for adb)\n",
                    name);
            return 1;
        }
        if (!run_shell(format(
                "%s devices 2>/dev/null | grep -q 'unauthorized'",
                adb))) {
            fprintf(stderr,
                    "[%s] android: the phone is asking \"Allow USB "
                    "debugging?\" — tap Allow, then retry\n",
                    name);
            return 1;
        }
    }
    fprintf(stderr, "[%s] installing on %s\n", name,
            sim ? "the emulator" : "the phone");
    if (run_shell(format("%s install -r %s", adb, apk))) {
        fprintf(stderr,
                "[%s] android: install failed — is a phone plugged "
                "in with usb debugging on?\n",
                name);
        return 1;
    }
    fprintf(stderr, "[%s] starting\n", name);
    run_shell(format("%s shell am start -n "
                     "com.silver.%s/android.app.NativeActivity",
                     adb, name));
    run_shell(format("p=''; for i in 1 2 3 4 5 6 7 8 9 10; do p=$(%s "
                     "shell pidof -s com.silver.%s 2>/dev/null | tr -d "
                     "'\\r'); [ -n \"$p\" ] && break; sleep 1; done; [ "
                     "-n \"$p\" ] && %s logcat --pid=$p",
                     adb, name, adb));
    return 0;
}
