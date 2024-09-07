#include <silver>

int main(int argc, char **argv) {
    A_start();
    AF   pool = alloc(AF);
    silver m = ctr(silver, string, str("module"));
    drop(pool);
}