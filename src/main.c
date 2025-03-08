#include "node.h"
#include <stdio.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define WIDTH 400
#define HEIGHT 400

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} RGBA32;

static RGBA32 pixels[WIDTH * HEIGHT];

typedef struct {
  float r, g, b;
} Color;

Color gray_gradient(float x, float y) {
  UNUSED(y);
  return (Color){x, x, x};
}

Color cool(float x, float y) {
  if (x * y >= 0)
    return (Color){x, y, 1};

  float r = fmodf(x, y);
  return (Color){r, r, r};
}

bool expect_number(Node *expr) {
  if (expr->kind != NK_NUMBER) {
    printf("%s:%d: ERROR: expected number\n", expr->file, expr->line);
    return false;
  }
  return true;
}

bool expect_triple(Node *expr) {
  if (expr->kind != NK_TRIPLE) {
    printf("%s:%d: ERROR: expected triple\n", expr->file, expr->line);
    return false;
  }
  return true;
}

bool expect_kind(Node *expr, Node_Kind kind) {
  if (expr->kind != kind) {
    printf("%s:%d: ERROR: expected '%s' but got '%s'\n", expr->file, expr->line,
           node_kind_string(kind), node_kind_string(expr->kind));
    return false;
  }
  return true;
}

#define node_print_ln(node) (node_print(node), printf("\n"))
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
    UNREACHABLE("node_print");
  }
}

#define node_binop_loc(kind, lhs, rhs)                                         \
  ((kind) == NK_ADD    ? ((lhs) + (rhs))                                       \
   : (kind) == NK_MULT ? ((lhs) * (rhs))                                       \
   : (kind) == NK_MOD  ? fmodf((lhs), (rhs))                                   \
   : (kind) == NK_GT   ? ((lhs) > (rhs))                                       \
                       : 0)
Node *eval_binop(Node *expr, float x, float y, Node_Kind kind) {
  Node *lhs = eval(expr->as.binop.lhs, x, y);
  if (!lhs || !expect_kind(lhs, kind))
    return NULL;

  Node *rhs = eval(expr->as.binop.rhs, x, y);
  if (!rhs || !expect_kind(rhs, kind))
    return NULL;

  if (expr->kind == NK_ADD || expr->kind == NK_MULT || expr->kind == NK_MOD) {
    return node_number_loc(
        expr->file, expr->line,
        node_binop_loc(expr->kind, lhs->as.number, rhs->as.number));
  } else if (expr->kind == NK_GT) {
    return node_boolean_loc(
        expr->file, expr->line,
        node_binop_loc(expr->kind, lhs->as.number, rhs->as.number));
  }

  return NULL;
}

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
    if (!then || !expect_kind(then, NK_BOOLEAN))
      return NULL;

    Node *elze = eval(expr->as.iff.elze, x, y);
    if (!elze || !expect_kind(elze, NK_BOOLEAN))
      return NULL;

    return cond->as.boolean ? then : elze;
  }

  default:
    UNREACHABLE("eval");
  }
}

bool eval_func(Node *f, float x, float y, Color *c) {
  Node *result = eval(f, x, y); // Evaluated ast

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

Node *gray_gradient_ast() {
  Node *node = node_triple(node_x(), node_x(), node_x());
  node_print_ln(node);
  return node;
}

int main() {
  bool ok = render_pixels(gray_gradient_ast());
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
