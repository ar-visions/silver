
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
            return 0;
        T item = self.buffer[--self.count];
        self.buffer[self.count] = 0;
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

class Pairs<K,V> {
    K[] keys;
    V[] values;
    V push(K key, V value) {
        long hash = key.hash();
        for (int i = 0; i < self.keys.count; i++) {
            K k = self.keys[i];
            if (k.hash() == hash) {
                if (k.compare(key) == 0) {
                    self.values[i] = value;
                    return value;
                }
            }
        }
        self.keys.push(key);
        self.values.push(value);
        return value;
    }
    V value_for(K key) {
        int index = self.keys.index_of(key);
        if (index)
            return self.values[index];
        return 0;
    }
}