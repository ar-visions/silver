class Array : Base {
    Base *buffer;
    int alloc_size;
    int count;
    
    void resize() {
        if (self.count + 1 > self.alloc_size) {
            int prev_size = self.alloc_size;
            Base *prev = self.buffer;
            self.alloc_size += max(8, self.alloc_size << 1);
            self.buffer = (Base *)malloc(self.alloc_size * sizeof(Base *));
            if (prev) {
                if (prev_size > 0)
                    memcpy(self.buffer, prev, prev_size * sizeof(Base *));
                free(prev);
            }
        }
    }
    void push(Base item) {
        self.resize();
        self.buffer[self.count] = item;
        self.count++;
    }
    Base pop() {
        if (self.count == 0)
            return null;
        Base item = self.buffer[--self.count];
        self.buffer[self.count] = null;
        return item;
    }
    int index_of(Base item) {
        for (int i = 0; i < self.count; i++) {
            Base o = self.buffer[i];
            if (o == item)
                return i;
        }
        return -1;
    }
}
