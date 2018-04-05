class String {
    int length;
    int alloc_size;
    char *buffer;

    void init() {
        self.resize(self.length + 1, true, false);
        self.buffer[0] = 0;
    }
    static String from_cstring(const char *value) {
        int len = strlen(value);
        String self = new String();
        self.resize(len + 1, true, false);
        memcpy(self.buffer, value, len + 1);
        return (String)self.autorelease();
    }
    void resize(int new_size, bool tight, bool copy) {
        if (new_size >= self.alloc_size) {
            int prev_size = self.alloc_size;
            char *prev = (char *)self.buffer;
            self.alloc_size = tight ? new_size : max(8, new_size << 1);
            self.buffer = (char *)malloc(self.alloc_size);
            if (prev) {
                if (copy && prev_size > 0 && prev_size < self.alloc_size)
                    memcpy((void *)self.buffer, prev, prev_size);
                free(prev);
            }
        }
    }
}