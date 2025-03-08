#pragma once
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node Node;

typedef enum {
  NK_X,
  NK_Y,
  NK_NUMBER,
  NK_ADD,
  NK_MULT,
  NK_TRIPLE,
} Node_Kind;

typedef struct {
  Node *lhs;
  Node *rhs;
} Node_Binop;

typedef struct {
  Node *first;
  Node *second;
  Node *third;
} Node_Triple;

typedef union {
  float number;
  Node_Binop binop;
  Node_Triple triple;
} Node_As;

struct Node {
  Node_Kind kind;
  const char *file;
  int line;
  Node_As as;
};

void node_print(Node *node);
Node *eval(Node *expr, float x, float y);

Node *node_loc(const char *file, int line, Node_Kind kind);

Node *node_number_loc(const char *file, int line, float number);
Node *node_add_loc(const char *file, int line, Node *lhs, Node *rhs);
Node *node_mult_loc(const char *file, int line, Node *lhs, Node *rhs);
Node *node_triple_loc(const char *file, int line, Node *first, Node *second,
                      Node *third);

// MACROS
