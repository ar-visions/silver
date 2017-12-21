#include <obj/obj.h>

implement(App)

App app;

void App_class_init(Class c) {
    app = new(App);
}

void App_init(App self) {
    self->delegates = new(List);
    app = self;
}

void App_free(App self) {
    release(self->delegates);
}

void App_loop(App self) {
    if (!self)
        self = app;
    for (;;) {
        AutoRelease ar = class_call(AutoRelease, current);
        AppDelegate d = NULL;
        each(self->delegates, d) {
            call(d, loop);
        }
        call(ar, drain);
#ifdef EMSCRIPTEN
        break;
#endif
        if (call(self->delegates, count) == 0)
            break;
    }
}

void App_push_delegate(App self, AppDelegate d) {
    list_push(self->delegates, d);
}

void App_remove_delegate(App self, AppDelegate d) {
    list_remove(self->delegates, d);
}


implement(AppDelegate)

void AppDelegate_loop(AppDelegate self) { }

implement(Timer)

void Timer_loop(Timer self) { }
void Timer_start(Timer self) {
    if (!self->running) {
        self->running = true;
        call(app, push_delegate, inherits(self, AppDelegate));
    }
}
void Timer_stop(Timer self) {
    if (self->running) {
        self->running = false;
        call(app, remove_delegate, inherits(self, AppDelegate));
    }
}
