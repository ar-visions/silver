#include <obj.h>
#include <obj-ui.h>

int main() {
    class_init();
    Window window = new(Window);
    window->title = new_string("Hello World");
    set(window, resizable, true);
    call(window, show);
    call(app, loop);
    release(window);
    return 0;
}