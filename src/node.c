#include "node.h"

#define ARENA_IMPLEMENTATION
#include "lib/arena.h"

static Arena node_arena = {0};

bool expect_kind(Node *expr, Node_Kind kind) {
  if (expr->kind != kind) {
    printf("%s:%d: ERROR: expected '%s' but got '%s'\n", expr->file, expr->line,
           node_kind_string(kind), node_kind_string(expr->kind));
    return false;
  }
  return true;
}

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

void node_print(Node *node) {
  switch (node->kind) {
  case NK_X:
  case NK_Y:
    printf(node_kind_string(node->kind));
    break;

  case NK_NUMBER:
    printf("%f", node->as.number);
    break;

  case NK_BOOLEAN:
    printf("%s", node->as.boolean ? "true" : "false");
    break;

  case NK_ADD:
  case NK_MULT:
  case NK_MOD:
  case NK_GT:
    printf("%s(", node_kind_string(node->kind));
    node_print(node->as.binop.lhs);
    printf(", ");
    node_print(node->as.binop.rhs);
    printf(")");
    break;

  case NK_TRIPLE:
    printf("(");
    node_print(node->as.triple.first);
    printf(", ");
    node_print(node->as.triple.second);
    printf(", ");
    node_print(node->as.triple.third);
    printf(")");
    break;

  case NK_IF:
    printf("if ");
    node_print(node->as.iff.cond);
    printf(" then ");
    node_print(node->as.iff.then);
    printf(" else ");
    node_print(node->as.iff.elze);
    break;

  default:
    // UNREACHABLE("node_print");
    return;
  }
}
