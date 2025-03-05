#pragma once

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
  Node_As as;
};
