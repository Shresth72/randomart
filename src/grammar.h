#include "node.h"

typedef struct {
  Node *node;
  float probability;
} Grammar_Branch;

typedef struct {
  Grammar_Branch *items;
  size_t capacity;
  size_t count;
} Grammar_Branches;

typedef struct {
  Grammar_Branches *items;
  size_t capacity;
  size_t count;
} Grammar;

void grammar_print(Grammar grammar);

Node *gen_rule(Grammar grammar, size_t rule, int depth);
Node *gen_node(Grammar grammar, Node *node, int depth);

#define GRAMMAR_PRINT_LN(grammar) (grammar_print(grammar), printf("\n"))
