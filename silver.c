#include <stdio.h>

typedef const char * symbol;

typedef struct node {
    struct node *parent;
    struct node *child;
    struct node *next;
    int          count;
} node;

typedef struct line {
    node* n;
};

/*
    # this is better than having curly braces around all setters?
    # it must be used for args too, but what happens when you introduce a type definition to a map; i suppose its legal?
    
    loop (int i:0 until i = 10) {
    }
*/

lang.keywords.loop = (expression *e) {
    /// a language parser would effectively make symbol nodes out of this, the child nodes
    /// start inside the block
    /// the ( expr ) counts as a peer node
}

int main(int argc, symbol argv[]) {
}