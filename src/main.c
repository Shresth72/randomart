#include "node.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define WIDTH 400
#define HEIGHT 400

void node_print(Node *node) {
  switch (node->kind) {
  case NK_X:
    printf("x");
    break;
  case NK_Y:
    printf("y");
    break;
  case NK_NUMBER:
    printf("%f", node->as.number);
    break;
  case NK_ADD:
    printf("add(");
    node_print(node->as.binop.lhs);
    printf(", ");
    node_print(node->as.binop.rhs);
    printf(")");
    break;
  case NK_MULT:
    printf("mult(");
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
  default:
    UNREACHABLE("node_print");
  }
}

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

#define node_kind_string(kind)                                                 \
  ((kind) == NK_X        ? "x"                                                 \
   : (kind) == NK_Y      ? "y"                                                 \
   : (kind) == NK_NUMBER ? "number"                                            \
   : (kind) == NK_ADD    ? "add"                                               \
   : (kind) == NK_MULT   ? "multiply"                                          \
   : (kind) == NK_TRIPLE ? "triple"                                            \
                         : "unknown")
bool expect_kind(Node *expr, Node_Kind kind) {
  if (expr->kind != kind) {
    printf("%s:%d: ERROR: expected '%s' but got '%s'\n", expr->file, expr->line,
           node_kind_string(kind), node_kind_string(expr->kind));
    return false;
  }
  return true;
}

Node *eval_binop(Node *expr, float x, float y) {
  Node *lhs = eval(expr->as.binop.lhs, x, y);
  if (!lhs || !expect_kind(lhs, NK_NUMBER))
    return NULL;

  Node *rhs = eval(expr->as.binop.rhs, x, y);
  if (!rhs || !expect_kind(rhs, NK_NUMBER))
    return NULL;

  float result = (expr->kind == NK_ADD) ? (lhs->as.number + rhs->as.number)
                                        : (lhs->as.number * rhs->as.number);
  return node_number_loc(expr->file, expr->line, result);
}

Node *eval(Node *expr, float x, float y) {
  switch (expr->kind) {
  case NK_X:
    return node_number_loc(expr->file, expr->line, x);

  case NK_Y:
    return node_number_loc(expr->file, expr->line, y);

  case NK_NUMBER:
    return expr;

  case NK_ADD:
  case NK_MULT:
    return eval_binop(expr, x, y);

  case NK_TRIPLE: {
    Node *first = eval(expr->as.triple.first, x, y);
    Node *second = eval(expr->as.triple.second, x, y);
    Node *third = eval(expr->as.triple.third, x, y);
    return (first && second && third)
               ? node_triple_loc(expr->file, expr->line, first, second, third)
               : NULL;
  }

  default:
    UNREACHABLE("eval");
  }
}

bool eval_func(Node *body, float x, float y, Color *c) {
  Node *result = eval(body, x, y);

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

void render_pixels(Color (*f)(float x, float y)) {
  for (size_t y = 0; y < HEIGHT; ++y) {
    // 0..<HEIGHT => 0..1 => 0..2 => -1..1
    float ny = (float)y / HEIGHT * 2.0f - 1;
    for (size_t x = 0; x < WIDTH; ++x) {
      float nx = (float)x / WIDTH * 2.0f - 1;
      Color c = f(nx, ny);
      size_t index = y * WIDTH + x;

      pixels[index].r = (c.r + 1) / 2 * 255;
      pixels[index].g = (c.g + 1) / 2 * 255;
      pixels[index].b = (c.b + 1) / 2 * 255;
      pixels[index].a = 255;
    }
  }
}

// MACROS
#define node_print_ln(node) (node_print(node), printf("\n"))

#define node_number(number) node_number_loc(__FILE__, __LINE__, number)
#define node_x() node_loc(__FILE__, __LINE__, NK_X)
#define node_y() node_loc(__FILE__, __LINE__, NK_Y)
#define node_add(lhs, rhs) node_add_loc(__FILE__, __LINE__, lhs, rhs)
#define node_mult(lhs, rhs) node_mult_loc(__FILE__, __LINE__, lhs, rhs)
#define node_triple(first, second, third)                                      \
  node_triple_loc(__FILE__, __LINE__, first, second, third)

int main() {
  Node *node =
      node_triple(node_add(node_triple(node_x(), node_x(), node_x()), node_y()),
                  node_y(), node_number(0.5));

  node_print_ln(node);
  Node *result = eval(node, 2, 2);
  if (!result)
    return 0;
  node_print_ln(result);
  exit(69);

  render_pixels(cool);

  const char *output_path = "output/output_path.png";
  if (!stbi_write_png(output_path, WIDTH, HEIGHT, 4, pixels,
                      WIDTH * sizeof(RGBA32))) {
    nob_log(ERROR, "Could not save image: %s", output_path);
    return 1;
  }

  nob_log(INFO, "Generated: %s", output_path);

  return 0;
}
