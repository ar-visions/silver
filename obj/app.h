class AppDelegate {
    void loop(C);
}

class Timer : AppDelegate {
    override void loop(C);
    void start(C);
    void stop(C);
    int running;
    int interval;
    int wake;
}

class App {
    override void class_init(Class);
    override void init(C);
    override void free(C);
    void loop(C);
    void push_delegate(C, AppDelegate);
    void remove_delegate(C, AppDelegate);
    private List delegates;
}

EXPORT App app;
