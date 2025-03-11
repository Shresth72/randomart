#include "grammar.h"
#include "node.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define ARENA_IMPLEMENTATION
#include "lib/arena.h"

#define WIDTH 400
#define HEIGHT 400
#define GEN_RULE_MAX_ATTEMPTS 10

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} RGBA32;

typedef struct {
  float r, g, b;
} Color;

static Arena node_arena = {0};
static RGBA32 pixels[WIDTH * HEIGHT];

static_assert(COUNT_NK == 12, "Amount of nodes have changed");
const char *nk_names[COUNT_NK] = {
    [NK_X] = "x",           [NK_Y] = "y",
    [NK_NUMBER] = "number", [NK_BOOLEAN] = "boolean",
    [NK_ADD] = "add",       [NK_MULT] = "mult",
    [NK_MOD] = "mod",       [NK_GT] = "gt",
    [NK_TRIPLE] = "triple", [NK_IF] = "if",

    [NK_RULE] = "rule",     [NK_RANDOM] = "random",
};

// Arena allocator for 'Node' in node_arena
Node *node_loc(const char *file, int line, Node_Kind kind) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = kind;
  node->file = file;
  node->line = line;

  return node;
}

// Dynamic Arena allocator for 'Grammar' in node_arena
void append_branch(Grammar_Branches *branches, Node *node, float probability) {
  arena_da_append(&node_arena, branches,
                  ((Grammar_Branch){.node = node, .probability = probability}));
}

// Macro Mapper for binary operations
#define BINOP_MAPPER(kind, lhs, rhs)                                           \
  ((kind) == NK_ADD    ? ((lhs) + (rhs))                                       \
   : (kind) == NK_MULT ? ((lhs) * (rhs))                                       \
   : (kind) == NK_MOD  ? fmodf((lhs), (rhs))                                   \
   : (kind) == NK_GT   ? ((lhs) > (rhs))                                       \
                       : 0)
/// Evaluate Binary Operations
Node *eval_binop(Node *expr, float x, float y, Node_Kind kind) {
  Node *lhs = eval(expr->as.binop.lhs, x, y);
  if (!lhs || !expect_kind(lhs, kind))
    return NULL;

  Node *rhs = eval(expr->as.binop.rhs, x, y);
  if (!rhs || !expect_kind(rhs, kind))
    return NULL;

  if (expr->kind == NK_GT) {
    return node_boolean_loc(
        expr->file, expr->line,
        BINOP_MAPPER(expr->kind, lhs->as.number, rhs->as.number));
  }

  return node_number_loc(
      expr->file, expr->line,
      BINOP_MAPPER(expr->kind, lhs->as.number, rhs->as.number));
}

/// Evaluate Node Expressions (AST)
Node *eval(Node *expr, float x, float y) {
  switch (expr->kind) {
  case NK_X:
    return node_number_loc(expr->file, expr->line, x);

  case NK_Y:
    return node_number_loc(expr->file, expr->line, y);

  case NK_NUMBER:
  case NK_BOOLEAN:
    return expr;

  case NK_ADD:
  case NK_MULT:
  case NK_MOD:
  case NK_GT:
    return eval_binop(expr, x, y, NK_NUMBER);

  case NK_TRIPLE: {
    Node *first = eval(expr->as.triple.first, x, y);
    Node *second = eval(expr->as.triple.second, x, y);
    Node *third = eval(expr->as.triple.third, x, y);
    return (first && second && third)
               ? node_triple_loc(expr->file, expr->line, first, second, third)
               : NULL;
  }

  case NK_IF: {
    Node *cond = eval(expr->as.iff.cond, x, y);
    if (!cond || !expect_kind(cond, NK_BOOLEAN))
      return NULL;

    Node *then = eval(expr->as.iff.then, x, y);
    if (!then || !expect_kind(then, NK_TRIPLE))
      return NULL;

    Node *elze = eval(expr->as.iff.elze, x, y);
    if (!elze || !expect_kind(elze, NK_TRIPLE))
      return NULL;

    return cond->as.boolean ? then : elze;
  }

  case NK_RANDOM:
  case NK_RULE:
    printf("%s:%d: ERROR: cannot evaluate a node that is only valid for "
           "grammar definitions\n",
           expr->file, expr->line);
    return NULL;

  case COUNT_NK:
  default:
    UNREACHABLE_CODE("eval");
  }
}

/// Evaluate and assign triple/colors to each pixel
bool eval_func(Node *f, float x, float y, Color *c) {
  Node *result = eval(f, x, y);

  if (!result || !expect_kind(result, NK_TRIPLE))
    return false;
  if (!expect_kind(result->as.triple.first, NK_NUMBER) ||
      !expect_kind(result->as.triple.second, NK_NUMBER) ||
      !expect_kind(result->as.triple.third, NK_NUMBER))
    return false;

  c->r = result->as.triple.first->as.number;
  c->g = result->as.triple.second->as.number;
  c->b = result->as.triple.third->as.number;
  return true;
}

/// Render the evaluated pixel values from the ast
bool render_pixels(Node *f) {
  for (size_t y = 0; y < HEIGHT; ++y) {
    // 0..<HEIGHT => 0..1 => 0..2 => -1..1
    float ny = (float)y / HEIGHT * 2.0f - 1;
    for (size_t x = 0; x < WIDTH; ++x) {
      float nx = (float)x / WIDTH * 2.0f - 1;

      Color c;
      if (!eval_func(f, nx, ny, &c))
        return false;

      size_t index = y * WIDTH + x;
      pixels[index].r = (c.r + 1) / 2 * 255;
      pixels[index].g = (c.g + 1) / 2 * 255;
      pixels[index].b = (c.b + 1) / 2 * 255;
      pixels[index].a = 255;
    }
  }

  return true;
}

// Color: {x, x, x}
Node *gray_gradient_ast() {
  Node *node = node_triple(node_x(), node_x(), node_x());
  NODE_PRINT_LN(node);
  return node;
}

// Color: if (x * y > 0) then {x, y, 1} else {fmod(x, y), fmodf(x, y), fmodf(x,
// y)}
Node *cool_gradient_ast() {
  Node *node = node_if(node_gt(node_mult(node_x(), node_y()), node_number(0)),
                       node_triple(node_x(), node_y(), node_number(1)),
                       node_triple(node_mod(node_x(), node_y()),
                                   node_mod(node_x(), node_y()),
                                   node_mod(node_x(), node_y())));
  NODE_PRINT_LN(node);
  return node;
}

float rand_float(void) { return (float)rand() / RAND_MAX; }

Node *gen_node(Grammar grammar, Node *node, int depth) {
  switch (node->kind) {
  case NK_X:
  case NK_Y:
  case NK_NUMBER:
  case NK_BOOLEAN:
    return node;

  case NK_ADD:
  case NK_MULT:
  case NK_MOD:
  case NK_GT: {
    Node *lhs = gen_node(grammar, node->as.binop.lhs, depth);
    if (!lhs)
      return NULL;
    Node *rhs = gen_node(grammar, node->as.binop.rhs, depth);
    if (!rhs)
      return NULL;
    return node_binop_loc(node->file, node->line, node->kind, lhs, rhs);
  }

  case NK_TRIPLE: {
    Node *first = gen_node(grammar, node->as.triple.first, depth);
    if (!first)
      return NULL;
    Node *second = gen_node(grammar, node->as.triple.second, depth);
    if (!second)
      return NULL;
    Node *third = gen_node(grammar, node->as.triple.third, depth);
    if (!third)
      return NULL;
    return node_triple_loc(node->file, node->line, first, second, third);
  }

  case NK_IF: {
    Node *cond = gen_node(grammar, node->as.iff.cond, depth);
    if (!cond)
      return NULL;
    Node *then = gen_node(grammar, node->as.iff.then, depth);
    if (!then)
      return NULL;
    Node *elze = gen_node(grammar, node->as.iff.elze, depth);
    if (!elze)
      return NULL;
    return node_if_loc(node->file, node->line, cond, then, elze);
  }

  case NK_RULE: {
    return gen_rule(grammar, node->as.rule, depth - 1);
  }

  case NK_RANDOM: {
    return node_number_loc(node->file, node->line, rand_float() * 2.0f - 1.0f);
  }

  case COUNT_NK:
  default:
    UNREACHABLE_CODE("gen_node");
  }
}

Node *gen_rule(Grammar grammar, size_t rule, int depth) {
  if (depth <= 0)
    return NULL;

  assert(rule < grammar.count);

  Grammar_Branches *branches = &grammar.items[rule];
  assert(branches->count > 0);

  Node *node = NULL;
  for (size_t attempts = 0; node == NULL && attempts < GEN_RULE_MAX_ATTEMPTS;
       ++attempts) {
    float p = rand_float();
    float t = 0.0f;
    for (size_t i = 0; i < branches->count; ++i) {
      t += branches->items[i].probability;
      if (t >= p) {
        node = gen_node(grammar, branches->items[i].node, depth - 1);
        break;
      }
    }
  }

  return node;
}

// Grammar:
// E ::= (C, C, C) ^ (1, 1),
// A ::=〈random number ∈ [-1, 1]〉^ (1/3) | x ^ (1/3) | y ^ (1/3),
// C ::= A ^ (1/4) | add(C, C) ^ (3/8) | multi(C, C) ^ (3/8)
Node *simple_grammar() {
  srand(time(0));
  Grammar grammar = {0};
  Grammar_Branches branches = {0};
  size_t e = 0;
  int a = 1;
  int c = 2;

  // E
  append_branch(&branches,
                node_triple(node_rule(c), node_rule(c), node_rule(c)), 1.f);
  arena_da_append(&node_arena, &grammar, branches);
  memset(&branches, 0, sizeof(branches));

  // A
  append_branch(&branches, node_random(), 1.f / 3.f);
  append_branch(&branches, node_x(), 1.f / 3.f);
  append_branch(&branches, node_y(), 1.f / 3.f);
  arena_da_append(&node_arena, &grammar, branches);
  memset(&branches, 0, sizeof(branches));

  // C
  append_branch(&branches, node_rule(a), 1.f / 4.f);
  append_branch(&branches, node_add(node_rule(c), node_rule(c)), 3.f / 8.f);
  append_branch(&branches, node_mult(node_rule(c), node_rule(c)), 3.f / 8.f);
  arena_da_append(&node_arena, &grammar, branches);
  memset(&branches, 0, sizeof(branches));

  GRAMMAR_PRINT_LN(grammar);

  Node *f = gen_rule(grammar, e, 10);
  if (!f) {
    fprintf(stderr, "ERROR: Process could not terminate\n");
    exit(69);
  }
  NODE_PRINT_LN(f);

  return f;
}

int main() {
  // bool ok = render_pixels(gray_gradient_ast());
  // bool ok = render_pixels(cool_gradient_ast());
  bool ok = render_pixels(simple_grammar());
  if (!ok)
    return 1;

  const char *output_path = "output/output_path.png";
  if (!stbi_write_png(output_path, WIDTH, HEIGHT, 4, pixels,
                      WIDTH * sizeof(RGBA32))) {
    nob_log(ERROR, "Could not save image: %s", output_path);
    return 1;
  }

  nob_log(INFO, "Generated: %s", output_path);

  return 0;
}
