#include <ui.h>

implement(Window)

static void Window_error(int error, const char * msg) {
    fprintf(stderr, "GL Error: %s\n", msg);
}

static void Window_sized(GLFWwindow* window, int w, int h) {
    if (!window)
        return;
	Window self = glfwGetWindowUserPointer(window);
    if (!self)
        return;
	int fb_w = 0, fb_h = 0;
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	glViewport(0, 0, fb_w, fb_h);
    self->width = fb_w;
    self->height = fb_h;
}

void Window_class_init(Class c) {
    glfwSetErrorCallback(Window_error);
    if (!glfwInit())
        exit(0);
}

void Window_init(Window self) {
    self->major_version = 3;
    self->minor_version = 0;
    self->resizable = false;
    self->vsync = true;
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
    call(app, remove_delegate, inherits(self, AppDelegate));
}

void Window_show(Window self) {
    if (!self->window) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, self->major_version);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, self->minor_version);
        glfwWindowHint(GLFW_RESIZABLE, (int)self->resizable);
        glfwWindowHint(GLFW_DOUBLEBUFFER, (int)self->double_buffer);
        self->window = glfwCreateWindow(self->width, self->height,
            self->title ? self->title->buffer : "GL", NULL, NULL);
        glfwSetWindowUserPointer(self->window, self);
        glfwSetWindowSizeCallback(self->window, Window_sized);
        glfwMakeContextCurrent(self->window);
        glfwSwapInterval((int)self->vsync);
        call(app, push_delegate, inherits(self, AppDelegate));
    } else
        glfwShowWindow(self->window);
}

void Window_render(Window self) {
	GLFWwindow *window = self->window;
	glClear(GL_COLOR_BUFFER_BIT);
	//drawing_test(gfx);
    glfwSwapBuffers(self->window);
    glfwPollEvents();
}

void Window_loop(Window self) {
    if (self->window) {
        if (!glfwWindowShouldClose(self->window))
            call(self, render);
        else
            call(self, destroy);
    }
}
