#include <silver/import>

int main(int argc, cstr* argv) {
    startup(argv);

    for (int au = 0; au <= 10; au++)
        print("the number is: %i", au);

    return 0;
}