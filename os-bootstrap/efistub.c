// the stick's BOOTX64.EFI: cmdline.txt, then vmlinuz.efi; with no
// kernel beside it, the NTFS driver beside it reads \orbiter-os\boot
typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;
typedef u64                status_t;
typedef void*              handle_t;

typedef struct { u32 a; u16 b, c; u8 d[8]; } guid_t;

typedef struct { u64 sig; u32 rev, size, crc, reserved; } header_t;

typedef struct text_out {
    void* reset;
    status_t (*output)(struct text_out*, u16*);
} text_out_t;

// the boot services, in the spec's order, up to StartImage
typedef struct {
    header_t hdr;
    void *raise_tpl, *restore_tpl, *alloc_pages, *free_pages, *memory_map;
    status_t (*alloc_pool)(u32, u64, void**);
    status_t (*free_pool)(void*);
    void *create_event, *set_timer, *wait_event, *signal_event,
         *close_event, *check_event, *install_proto, *reinstall_proto,
         *uninstall_proto;
    status_t (*handle_protocol)(handle_t, guid_t*, void**);
    void *reserved, *register_notify, *locate_handle, *locate_path,
         *install_config;
    status_t (*load_image)(u8, handle_t, void*, void*, u64, handle_t*);
    status_t (*start_image)(handle_t, u64*, u16**);
    void *exit, *unload_image, *exit_boot, *next_count;
    status_t (*stall)(u64);
    void *watchdog;
    status_t (*connect)(handle_t, handle_t*, void*, u8);
    void *disconnect, *open_proto, *close_proto, *open_info, *per_handle;
    status_t (*locate_buffer)(u32, guid_t*, void*, u64*, handle_t**);
} boot_t;

typedef struct {
    header_t hdr;
    u16* vendor;
    u32 revision;
    handle_t in_handle;
    void* in;
    handle_t out_handle;
    text_out_t* out;
    handle_t err_handle;
    void* err;
    void* runtime;
    boot_t* boot;
} system_t;

typedef struct {
    u32 revision;
    handle_t parent;
    system_t* system;
    handle_t device;
    void* file_path;
    void* reserved;
    u32 options_size;
    void* options;
} loaded_image_t;

typedef struct file {
    u64 revision;
    status_t (*open)(struct file*, struct file**, u16*, u64, u64);
    status_t (*close)(struct file*);
    void* del;
    status_t (*read)(struct file*, u64*, void*);
    void* write;
    status_t (*get_pos)(struct file*, u64*);
    status_t (*set_pos)(struct file*, u64);
} file_t;

typedef struct fs {
    u64 revision;
    status_t (*open_volume)(struct fs*, file_t**);
} fs_t;

static guid_t loaded_image_guid =
    { 0x5b1b31a1, 0x9562, 0x11d2,
      { 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
static guid_t fs_guid =
    { 0x964e5b22, 0x6459, 0x11d2,
      { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };

#define LOADER_DATA 2
#define ALL_HANDLES 0
#define BY_PROTOCOL 2

static system_t* st;

// a message on screen, held five seconds
static status_t fail(u16* why, status_t s) {
    st->out->output(st->out, why);
    st->boot->stall(5000000);
    return s ? s : 1;
}

// a whole file from this volume, in pool memory
static status_t read_all(file_t* root, u16* name, u8** data, u64* size) {
    file_t* f;
    status_t s = root->open(root, &f, name, 1, 0);
    if (s) return s;
    f->set_pos(f, ~0ULL);
    f->get_pos(f, size);
    f->set_pos(f, 0);
    s = st->boot->alloc_pool(LOADER_DATA, *size + 2, (void**)data);
    if (!s) s = f->read(f, size, *data);
    f->close(f);
    return s;
}

// no kernel beside us: EfiFs's NTFS driver (beside us) gives each NTFS
// partition a file system, and the one with \orbiter-os\boot's kernel
// is the system's (a Windows partition holding the orbiter-os folder)
static status_t ntfs_kernel(handle_t image, file_t* root, handle_t* from,
                            u8** kernel, u64* kernel_n) {
    u8* drv; u64 drv_n;
    status_t s = read_all(root, u"\\EFI\\orbiter\\ntfs_x64.efi", &drv, &drv_n);
    if (s) return s;
    handle_t dh;
    s = st->boot->load_image(0, image, 0, drv, drv_n, &dh);
    if (s) return s;
    s = st->boot->start_image(dh, 0, 0);
    if (s) return s;
    // every controller again: the driver binds to the partitions
    handle_t* all; u64 all_n;
    s = st->boot->locate_buffer(ALL_HANDLES, 0, 0, &all_n, &all);
    if (s) return s;
    for (u64 i = 0; i < all_n; i++) st->boot->connect(all[i], 0, 0, 1);
    st->boot->free_pool(all);
    handle_t* vols; u64 vols_n;
    s = st->boot->locate_buffer(BY_PROTOCOL, &fs_guid, 0, &vols_n, &vols);
    if (s) return s;
    for (u64 i = 0; i < vols_n; i++) {
        fs_t* fs; file_t* vr;
        if (st->boot->handle_protocol(vols[i], &fs_guid, (void**)&fs)) continue;
        if (fs->open_volume(fs, &vr)) continue;
        if (!read_all(vr, u"\\orbiter-os\\boot\\vmlinuz.efi", kernel, kernel_n)) {
            *from = vols[i];
            st->boot->free_pool(vols);
            return 0;
        }
    }
    st->boot->free_pool(vols);
    return (1ULL << 63) | 14; // EFI_NOT_FOUND
}

u64 efi_main(handle_t image, system_t* system) {
    st = system;
    loaded_image_t* me;
    status_t s = st->boot->handle_protocol(image, &loaded_image_guid,
                                           (void**)&me);
    if (s) return fail(u"orbiter: no loaded image\r\n", s);
    fs_t* fs;
    s = st->boot->handle_protocol(me->device, &fs_guid, (void**)&fs);
    if (s) return fail(u"orbiter: no file system\r\n", s);
    file_t* root;
    s = fs->open_volume(fs, &root);
    if (s) return fail(u"orbiter: no volume\r\n", s);

    u8* line; u64 line_n;
    s = read_all(root, u"\\EFI\\orbiter\\cmdline.txt", &line, &line_n);
    if (s) return fail(u"orbiter: no EFI\\orbiter\\cmdline.txt\r\n", s);
    u8* kernel; u64 kernel_n;
    handle_t from = me->device;
    s = read_all(root, u"\\EFI\\orbiter\\vmlinuz.efi", &kernel, &kernel_n);
    if (s) {
        s = ntfs_kernel(image, root, &from, &kernel, &kernel_n);
        if (s) return fail(u"orbiter: no kernel here or in \\orbiter-os\\boot\r\n", s);
    }

    // the line in UTF-16, ended at its first newline
    u16* wide;
    s = st->boot->alloc_pool(LOADER_DATA, (line_n + 1) * 2, (void**)&wide);
    if (s) return fail(u"orbiter: out of memory\r\n", s);
    u64 n = 0;
    while (n < line_n && line[n] != '\n' && line[n] != '\r') {
        wide[n] = line[n];
        n++;
    }
    wide[n] = 0;

    handle_t child;
    s = st->boot->load_image(0, image, 0, kernel, kernel_n, &child);
    if (s) return fail(u"orbiter: the kernel did not load\r\n", s);
    loaded_image_t* li;
    s = st->boot->handle_protocol(child, &loaded_image_guid, (void**)&li);
    if (s) return fail(u"orbiter: no kernel image\r\n", s);
    // loaded from memory: give it its volume, for initrd=
    li->device       = from;
    li->options      = wide;
    li->options_size = (u32)((n + 1) * 2);
    s = st->boot->start_image(child, 0, 0);
    return fail(u"orbiter: the kernel returned\r\n", s);
}
