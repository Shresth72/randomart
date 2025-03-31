#include "grammar.h"
#include "lib/raylib/raylib-5.5_linux_amd64/include/raylib.h"
#include "node.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

#define ARENA_IMPLEMENTATION
#include "lib/arena.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WIDTH 400
#define HEIGHT 400
#define GEN_RULE_MAX_ATTEMPTS 10
#define GRAMMAR_DEPTH 10

static Arena node_arena = {0};
// static Color pixels[WIDTH * HEIGHT];

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

  case NK_SQRT:
    return eval_unop(expr, x, y, NK_NUMBER);

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

  default:
    UNREACHABLE_CODE("eval");
  }
}

/// Evaluate and assign triple/colors to each pixel
bool eval_func(Node *f, float x, float y, Vector3 *c) {
  Node *result = eval(f, x, y);

  if (!result || !expect_kind(result, NK_TRIPLE))
    return false;
  if (!expect_kind(result->as.triple.first, NK_NUMBER) ||
      !expect_kind(result->as.triple.second, NK_NUMBER) ||
      !expect_kind(result->as.triple.third, NK_NUMBER))
    return false;

  c->x = result->as.triple.first->as.number;
  c->y = result->as.triple.second->as.number;
  c->z = result->as.triple.third->as.number;
  return true;
}

/// Render the evaluated pixel values from the ast
bool render_pixels(Image image, Node *f) {
  Color *pixels = image.data;

  for (size_t y = 0; y < HEIGHT; ++y) {
    // 0..<HEIGHT => 0..1 => 0..2 => -1..1
    float ny = (float)y / HEIGHT * 2.0f - 1;
    for (size_t x = 0; x < WIDTH; ++x) {
      float nx = (float)x / WIDTH * 2.0f - 1;

      Vector3 c;
      if (!eval_func(f, nx, ny, &c))
        return false;

      size_t index = y * WIDTH + x;
      pixels[index].r = (c.x + 1) / 2 * 255;
      pixels[index].g = (c.y + 1) / 2 * 255;
      pixels[index].b = (c.z + 1) / 2 * 255;
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

  case NK_SQRT: {
    Node *value = gen_node(grammar, node->as.unop, depth);
    if (!value)
      return NULL;
    return node_unop_loc(node->file, node->line, value);
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
int simple_grammar(Grammar *grammar) {
  Grammar_Branches branches = {0};
  size_t e = 0;
  int a = 1;
  int c = 2;

  // E
  append_branch(&branches,
                node_triple(node_rule(c), node_rule(c), node_rule(c)), 1.f);
  arena_da_append(&node_arena, grammar, branches);
  memset(&branches, 0, sizeof(branches));

  // A
  append_branch(&branches, node_random(), 1.f / 3.f);
  append_branch(&branches, node_x(), 1.f / 3.f);
  append_branch(&branches, node_y(), 1.f / 3.f);
  arena_da_append(&node_arena, grammar, branches);
  memset(&branches, 0, sizeof(branches));

  // C
  append_branch(&branches, node_rule(a), 1.f / 4.f);
  append_branch(&branches, node_add(node_rule(c), node_rule(c)), 3.f / 8.f);
  append_branch(&branches, node_mult(node_rule(c), node_rule(c)), 3.f / 8.f);
  arena_da_append(&node_arena, grammar, branches);
  memset(&branches, 0, sizeof(branches));

  GRAMMAR_PRINT_LN(*grammar);
  return e;
}

Texture GetDefaultTexture() {
  return (Texture){
      .id = rlGetTextureIdDefault(),
      .width = 1,
      .height = 1,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
}

bool compile_node_func_into_fragment_expression(String_Builder *sb,
                                                Node *expr) {
  switch (expr->kind) {
  case NK_X:
    sb_append_cstr(sb, "x");
    break;
  case NK_Y:
    sb_append_cstr(sb, "y");
    break;

  case NK_NUMBER: {
    size_t checkpoint = nob_temp_save();
    sb_append_cstr(sb, temp_sprintf("%f", expr->as.number));
    nob_temp_rewind(checkpoint);
  } break;

  case NK_BOOLEAN:
    sb_append_cstr(sb, expr->as.boolean ? "true" : "false");
    break;

  case NK_ADD:
  case NK_MULT:
  case NK_GT:
    sb_append_cstr(sb, "(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.lhs))
      return false;
    sb_append_cstr(sb, node_kind_operation(expr->kind));
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.rhs))
      return false;
    sb_append_cstr(sb, ")");
    break;

  case NK_MOD:
    sb_append_cstr(sb, "mod(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.lhs))
      return false;
    sb_append_cstr(sb, ",");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.rhs))
      return false;
    sb_append_cstr(sb, ")");
    break;

  case NK_SQRT:
    sb_append_cstr(sb, "sqrt(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.unop))
      return false;
    sb_append_cstr(sb, ")");
    break;

  case NK_TRIPLE:
    sb_append_cstr(sb, "vec3(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.triple.first))
      return false;
    sb_append_cstr(sb, ",");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.triple.second))
      return false;
    sb_append_cstr(sb, ",");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.triple.third))
      return false;
    sb_append_cstr(sb, ")");
    break;

  case NK_IF:
    sb_append_cstr(sb, "(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.iff.cond))
      return false;
    sb_append_cstr(sb, "?");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.iff.then))
      return false;
    sb_append_cstr(sb, ":");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.iff.elze))
      return false;
    sb_append_cstr(sb, ")");
    break;

  case NK_RULE:
  case NK_RANDOM:
    printf("%s:%d: ERROR: cannot compile a node that is only valid for grammar "
           "definitions\n",
           expr->file, expr->line);
    return false;

  default:
    UNREACHABLE_CODE("compile_node_func_into_fragment_expression");
  }

  return true;
}

bool compile_node_func_into_fragment_shader(String_Builder *sb, Node *f) {
  sb_append_cstr(sb, "#version 330\n");
  sb_append_cstr(sb, "in vec2 fragTexCoord;\n");
  sb_append_cstr(sb, "out vec4 finalColor;\n");

  sb_append_cstr(sb, "vec4 map_color(vec3 rgb) {\n");
  sb_append_cstr(sb, "  return vec4((rgb + 1)/2.0, 1.0);\n");
  sb_append_cstr(sb, "}\n");

  sb_append_cstr(sb, "void main() {\n");
  sb_append_cstr(sb, "  float x = fragTexCoord.x * 2.0 - 1.0;\n");
  sb_append_cstr(sb, "  float y = fragTexCoord.y * 2.0 - 1.0;\n");
  sb_append_cstr(sb, "  finalColor = map_color(");
  if (!compile_node_func_into_fragment_expression(sb, f))
    return false;
  sb_append_cstr(sb, ");\n");
  sb_append_cstr(sb, "}\n");

  return true;
}

int main(int argc, char **argv) {
  srand(time(0));

  const char *program_name = shift(argv, argc);
  if (argc <= 0) {
    nob_log(ERROR, "Usage: %s <command>\n", program_name);
    nob_log(ERROR, "No command is provided\n");
    return 1;
  }

  const char *command_name = shift(argv, argc);
  if (strcmp(command_name, "file") == 0) {
    if (argc <= 0) {
      nob_log(ERROR, "Usage: %s %s <output_path>", program_name, command_name);
      nob_log(ERROR, "No output path is provided");
      return 1;
    }
    const char *output_path = shift(argv, argc);

    // Node *f = gray_gradient_ast();
    // Node* f = cool_gradient_ast();
    Grammar grammar = {0};
    int entry = simple_grammar(&grammar);
    Node *f = gen_rule(grammar, entry, GRAMMAR_DEPTH);
    if (!f) {
      nob_log(ERROR, "Process could not terminate\n");
      exit(69);
    }

    NODE_PRINT_LN(f);

    Image image = GenImageColor(WIDTH, HEIGHT, BLANK);
    if (!render_pixels(image, f))
      return 1;
    if (!ExportImage(image, output_path))
      return 1;

    return 0;
  }

  if (strcmp(command_name, "gui") == 0) {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "RandomArt");

    Grammar grammar = {0};
    int entry = simple_grammar(&grammar);
    Node *f = gen_rule(grammar, entry, GRAMMAR_DEPTH);
    if (!f) {
      nob_log(ERROR, "Process could not terminate\n");
      exit(69);
    }

    String_Builder sb = {0};
    if (!compile_node_func_into_fragment_shader(&sb, f))
      return 1;
    sb_append_null(&sb);
    printf("%s", sb.items);

    Shader shader = LoadShaderFromMemory(NULL, sb.items);
    Texture default_texture = GetDefaultTexture();

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
      BeginDrawing();

      int size = 500;
      BeginShaderMode(shader);
      DrawTexturePro(default_texture, (Rectangle){0, 0, 1, 1},
                     (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
                     (Vector2){0}, 0, WHITE);
      EndShaderMode();

      EndDrawing();
    }

    return 0;
  }

  nob_log(ERROR, "Unknown command: %s", command_name);
  return 0;
}
