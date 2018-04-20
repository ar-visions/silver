class Closure {
    ClosureFunc func;
    ClosureDealloc dealloc;
    void *memory;
    Closure init_closure(ClosureFunc func, void *memory, ClosureDealloc dealloc) {
        self.memory = memory;
        self.dealloc = dealloc;
        self.func = func;
        return self;
    }
    void dealloc() {
        self.dealloc(self.memory);
    }
}