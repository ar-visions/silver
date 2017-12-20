#include <obj.h>

implement(App)

static App app;

void App_init(App self) {
    self->delegates = new(List);
    app = self;
}

void App_free(App self) {
    release(self->delegates);
}

void App_loop(App self) {
    for (;;) {
        AppDelegate d = NULL;
        each(self->delegates, d) {
            call(d, loop);
        }
    }
}

void App_push(App self, AppDelegate d) {
    push(self->delegates, d);
}

void App_remove(App self, AppDelegate plug) {
    remove(self->delegates, d);
}

implement(AppDelegate)

void AppDelegate_loop(AppDelegate self) { }

// users of app loop would be things like console, timers, ui, etc; these have a context