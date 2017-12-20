#include <obj.h>

static void Gl_error(int error, const char * msg) {
    fprintf(stderr, "GL Error: %s\n", msg);
}

void Window_classinit(Class c) {
    glfwSetErrorCallback(Gl_error);
    if (!glfwInit())
        exit(0);
}

void Window_init(Window self) {
    self->major_version = 3;
    self->minor_version = 0;
    self->resizable = false;
    self->width = 1024;
    self->height = 768;
}

void Window_free(Window self) {
    call(self, destroy);
}

void Window_destroy(Window self) {
    if (!self->window)
        return;
    glfwSetWindowUserPointer(self->window, NULL);
    glfwDestroyWindow(self->window);
    self->window = NULL;
}

void Window_show() {
    if (!self->window) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, self->major_version);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, self->minor_version);
        glfwWindowHint(GLFW_RESIZABLE, (int)self->resizable);
        glfwWindowHint(GLFW_DOUBLEBUFFER, (int)self->double_buffer);
        self->window = glfwCreateWindow(self->width, self->height,
            self->title ? self->title->buffer : "GL", NULL, NULL);
        glfwSetWindowUserPointer(self->window, self);
        glfwMakeContextCurrent(self->window);
        glfwSwapInterval((int)self->vsync);
    } else
        glfwShowWindow(self->window);
}

void Window_loop() {
    if (self->window) {
        if (!glfwWindowShouldClose(self->window))
            self->render(self);
        else {
            call(self, destroy);
        }
    }
}

int main() {

	Gfx *gfx = gfx_init(w_width, w_height);
	if (!gfx)
		return 1;
	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSetWindowUserPointer(window, gfx);

	last_tick = gfx_millis();
#ifndef EMSCRIPTEN
	while (!glfwWindowShouldClose(window))
		render(window);
	glfwSetWindowUserPointer(window, NULL);
	gfx_destroy(gfx);
#else
	EM_ASM({
		Module.loaded();
	});
	emscripten_set_main_loop_arg(render, window, 0, 0);
#endif
	return 0;
}