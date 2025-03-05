#include "node.h"

#define ARENA_IMPLEMENTATION
#include "lib/arena.h"

static Arena node_arena = {0};

Node *node_number(float number) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = NK_NUMBER;
  node->as.number = number;

  return node;
}

Node *node_x(void) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = NK_X;

  return node;
}

Node *node_y(void) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = NK_Y;

  return node;
}

Node *node_add(Node *lhs, Node *rhs) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = NK_ADD;
  node->as.binop.lhs = lhs;
  node->as.binop.rhs = rhs;

  return node;
}

Node *node_mult(Node *lhs, Node *rhs) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = NK_MULT;
  node->as.binop.lhs = lhs;
  node->as.binop.rhs = rhs;

  return node;
}

Node *node_triple(Node *first, Node *second, Node *third) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = NK_TRIPLE;
  node->as.triple.first = first;
  node->as.triple.second = second;
  node->as.triple.third = third;

  return node;
}
