#include "node.h"

#define ARENA_IMPLEMENTATION
#include "lib/arena.h"

static Arena node_arena = {0};

Node *node_loc(const char *file, int line, Node_Kind kind) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = kind;
  node->file = file;
  node->line = line;

  return node;
}

Node *node_number_loc(const char *file, int line, float number) {
  Node *node = node_loc(file, line, NK_NUMBER);
  node->as.number = number;
  return node;
}

Node *node_boolean_loc(const char *file, int line, bool boolean) {
  Node *node = node_loc(file, line, NK_BOOLEAN);
  node->as.boolean = boolean;
  return node;
}

#define NODE_BINOP_LOC(func_name, kind)                                        \
  Node *func_name(const char *file, int line, Node *lhs, Node *rhs) {          \
    Node *node = node_loc(file, line, kind);                                   \
    node->as.binop.lhs = lhs;                                                  \
    node->as.binop.rhs = rhs;                                                  \
    return node;                                                               \
  }
NODE_BINOP_LOC(node_add_loc, NK_ADD);
NODE_BINOP_LOC(node_mult_loc, NK_MULT);
NODE_BINOP_LOC(node_mod_loc, NK_MOD);
NODE_BINOP_LOC(node_gt_loc, NK_GT);

Node *node_triple_loc(const char *file, int line, Node *first, Node *second,
                      Node *third) {
  Node *node = node_loc(file, line, NK_TRIPLE);
  node->as.triple.first = first;
  node->as.triple.second = second;
  node->as.triple.third = third;

  return node;
}

Node *node_if_loc(const char *file, int line, Node *cond, Node *then,
                  Node *elze) {
  Node *node = node_loc(file, line, NK_IF);
  node->as.iff.cond = cond;
  node->as.iff.then = then;
  node->as.iff.elze = elze;

  return node;
}
