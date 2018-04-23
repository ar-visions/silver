class Array<T> {
    T *buffer;
    int alloc_size;
    int count;
    
    void *resize() {
        if (self.count + 1 > self.alloc_size) {
            int prev_size = self.alloc_size;
            T *prev = self.buffer;
            self.alloc_size += max(8, self.alloc_size << 1);
            self.buffer = (T *)malloc(self.alloc_size * sizeof(T));
            if (prev) {
                if (prev_size > 0)
                    memcpy(self.buffer, prev, prev_size * sizeof(T));
                free(prev);
            }
        }
        return self.buffer;
    }
    void push(T item) {
        self.resize();
        self.buffer[self.count] = item;
        self.count++;
    }
    T pop() {
        if (self.count == 0)
            return null;
        T item = self.buffer[--self.count];
        self.buffer[self.count] = null;
        return item;
    }
    int index_of(T item) {
        for (int i = 0; i < self.count; i++) {
            T o = self.buffer[i];
            if (o == item)
                return i;
        }
        return -1;
    }
}

