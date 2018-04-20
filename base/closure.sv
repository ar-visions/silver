class Closure {
    ClosureFunc func;
    ClosureDealloc dealloc_func;
    void *memory;
    Closure init_closure(ClosureFunc func, void *memory, ClosureDealloc dealloc) {
        self.memory = memory;
        self.dealloc_func = dealloc;
        self.func = func;
        return self;
    }
    void dealloc() {
        (self.dealloc_func)(self.memory);
    }
}