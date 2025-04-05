#include "node.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

#define ARENA_IMPLEMENTATION
#include "lib/arena.h"

#define ALEXER_IMPLEMENTATION
#include "lib/alexer.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define IMAGE_WIDTH 400
#define IMAGE_HEIGHT 400
#define GEN_RULE_MAX_ATTEMPTS 10
#define GRAMMAR_DEPTH 20

static Arena node_arena = {0};

typedef enum {
  PUNCT_BAR,
  PUNCT_OPAREN,
  PUNCT_CPAREN,
  PUNCT_COMMA,
  PUNCT_SEMICOLON,
  COUNT_PUNCTS,
} Punct_Index;

const char *puncts[COUNT_PUNCTS] = {
  [PUNCT_BAR]       = "|", 
  [PUNCT_OPAREN]    = "(", 
  [PUNCT_CPAREN]    = ")", 
  [PUNCT_COMMA]     = ",", 
  [PUNCT_SEMICOLON] = ";"
};

static_assert(ALEXER_COUNT_KINDS == 7, "Amount of kinds have changed");
const char *alexer_kind_names[ALEXER_COUNT_KINDS] = {
    [ALEXER_INVALID] = "INVALID",
    [ALEXER_END] = "END",
    [ALEXER_INT] = "INT",
    [ALEXER_SYMBOL] = "SYMBOL",
    [ALEXER_KEYWORD] = "KEYWORD",
    [ALEXER_PUNCT] = "PUNCT",
    [ALEXER_STRING] = "STRING",
};

const char *comments[] = {
  "#",
};

Alexer_Token symbol_impl(const char *file, int line, const char *name_cstr) {
  UNUSED(file); UNUSED(line);

  return (Alexer_Token) { 
    .kind = ALEXER_SYMBOL,
    .loc = {
      .file_path = __FILE__,
      .row = __LINE__,
      .col = 0,
    },
    .begin = name_cstr,
    .end = name_cstr + strlen(name_cstr)
  };
}

typedef struct {
  Node *node;
  size_t weight;
} Grammar_Branch;

typedef struct {
  Grammar_Branch *items;
  size_t capacity;
  size_t count;
  size_t weight_sum;
  Alexer_Token name;
} Grammar_Branches;

typedef struct {
  Grammar_Branches *items;
  size_t capacity;
  size_t count;
} Grammar;

#define GRAMMAR_PRINT_LN(grammar) (grammar_print(grammar), printf("\n"))
void grammar_print(Grammar grammar) {
  for (size_t i = 0; i < grammar.count; ++i) {
    printf("%zu ::= ", i);
    Grammar_Branches *branches = &grammar.items[i];
    for (size_t j = 0; j < branches->count; ++j) {
      if (j > 0)
        printf(" | ");
      node_print(branches->items[j].node);
      printf(" [%zu]", branches->items[i].weight);
    }
    printf("\n");
  }
}

// Arena allocator for 'Node' in node_arena
Node *node_loc(const char *file, int line, Node_Kind kind) {
  Node *node = arena_alloc(&node_arena, sizeof(Node));
  node->kind = kind;
  node->file = file;
  node->line = line;

  return node;
}

// Dynamic Arena allocator for 'Grammar' in node_arena
void append_branch(Grammar_Branches *branches, Node *node, size_t weight) {
  arena_da_append(&node_arena, branches, ((Grammar_Branch){.node = node, .weight = weight}));
}

void grammar_append_branches(Grammar *grammar, Grammar_Branches *branches, const char *name) {
  branches->name = SYMBOL(name);
  for (size_t i = 0; i < branches->count; ++i) {
    branches->weight_sum += branches->items[i].weight;
  }

  arena_da_append(&node_arena, grammar, *branches);
  memset(branches, 0, sizeof(*branches));
}

/// Evaluate Node Expressions (AST)
Node *eval(Node *expr, float x, float y, float t) {
  switch (expr->kind) {
  case NK_X:
    return node_number_loc(expr->file, expr->line, x);
  case NK_Y:
    return node_number_loc(expr->file, expr->line, y);
  case NK_T:
    return node_number_loc(expr->file, expr->line, t);

  case NK_NUMBER:
  case NK_BOOLEAN:
    return expr;

  case NK_ADD:
  case NK_MULT:
  case NK_MOD:
  case NK_GT:
    return eval_binop(expr, x, y, t, NK_NUMBER);

  case NK_SQRT:
    return eval_unop(expr, x, y, t, NK_NUMBER);

  case NK_TRIPLE: {
    Node *first = eval(expr->as.triple.first, x, y, t);
    Node *second = eval(expr->as.triple.second, x, y, t);
    Node *third = eval(expr->as.triple.third, x, y, t);
    return (first && second && third)
               ? node_triple_loc(expr->file, expr->line, first, second, third)
               : NULL;
  }

  case NK_IF: {
    Node *cond = eval(expr->as.iff.cond, x, y, t);
    if (!cond || !expect_kind(cond, NK_BOOLEAN)) return NULL;

    Node *then = eval(expr->as.iff.then, x, y, t);
    if (!then || !expect_kind(then, NK_TRIPLE)) return NULL;

    Node *elze = eval(expr->as.iff.elze, x, y, t);
    if (!elze || !expect_kind(elze, NK_TRIPLE)) return NULL;

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
bool eval_func(Node *f, float x, float y, float t, Vector3 *c) {
  Node *result = eval(f, x, y, t);

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

  for (size_t y = 0; y < IMAGE_HEIGHT; ++y) {
    // 0..<IMAGE_HEIGHT => 0..1 => 0..2 => -1..1
    float ny = (float)y / IMAGE_HEIGHT * 2.0f - 1;
    for (size_t x = 0; x < IMAGE_WIDTH; ++x) {
      float nx = (float)x / IMAGE_WIDTH * 2.0f - 1;

      Vector3 c;
      if (!eval_func(f, nx, ny, 0.0f, &c))
        return false;

      size_t index = y * IMAGE_WIDTH + x;
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
Node *gen_rule(Grammar grammar, Alexer_Token rule, int depth);

Node *gen_node(Grammar grammar, Node *node, int depth) {
  switch (node->kind) {
  case NK_X:
  case NK_Y:
  case NK_T:
  case NK_NUMBER:
  case NK_BOOLEAN:
    return node;

  case NK_ADD:
  case NK_MULT:
  case NK_MOD:
  case NK_GT: {
    Node *lhs = gen_node(grammar, node->as.binop.lhs, depth);
    if (!lhs) return NULL;
    Node *rhs = gen_node(grammar, node->as.binop.rhs, depth);
    if (!rhs) return NULL;

    return node_binop_loc(node->file, node->line, node->kind, lhs, rhs);
  }

  case NK_SQRT: {
    Node *value = gen_node(grammar, node->as.unop, depth);
    if (!value) return NULL;

    return node_unop_loc(node->file, node->line, value);
  }

  case NK_TRIPLE: {
    Node *first = gen_node(grammar, node->as.triple.first, depth);
    if (!first) return NULL;
    Node *second = gen_node(grammar, node->as.triple.second, depth);
    if (!second) return NULL;
    Node *third = gen_node(grammar, node->as.triple.third, depth);
    if (!third) return NULL;

    return node_triple_loc(node->file, node->line, first, second, third);
  }

  case NK_IF: {
    Node *cond = gen_node(grammar, node->as.iff.cond, depth);
    if (!cond) return NULL;
    Node *then = gen_node(grammar, node->as.iff.then, depth);
    if (!then) return NULL;
    Node *elze = gen_node(grammar, node->as.iff.elze, depth);
    if (!elze) return NULL;

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

Grammar_Branches *branches_by_name(Grammar *grammar, Alexer_Token rule) {
  for (size_t i = 0; i < grammar->count; ++i) {
    if (alexer_token_text_equal(grammar->items[i].name, rule)) {
      return &grammar->items[i];
    }
  }
  return NULL;
}

Node *gen_rule(Grammar grammar, Alexer_Token rule, int depth) {
  if (depth <= 0)
    return NULL;

  Grammar_Branches *branches = branches_by_name(&grammar, rule);
  assert(branches->count > 0);

  Node *node = NULL;
  for (size_t attempts = 0; node == NULL && attempts < GEN_RULE_MAX_ATTEMPTS;
       ++attempts) {
    float p = rand_float();
    float t = 0.0f;
    for (size_t i = 0; i < branches->count; ++i) {
      t += (float)branches->items[i].weight / branches->weight_sum;
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
Alexer_Token simple_grammar(Grammar *grammar) {
  Grammar_Branches branches = {0};

  // E
  append_branch(&branches, node_triple(node_rule("C"), node_rule("C"), node_rule("C")), 1);
  grammar_append_branches(grammar, &branches, "E");

  // A
  append_branch(&branches, node_random(), 1);
  append_branch(&branches, node_x(), 1);
  append_branch(&branches, node_y(), 1);
  append_branch(&branches, node_t(), 1);
  append_branch(&branches,
                node_sqrt(
                    node_add(
                        node_add(
                            node_mult(node_x(), node_x()),
                            node_mult(node_y(), node_y())),
                        node_mult(node_t(), node_t()))),
                1);
  grammar_append_branches(grammar, &branches, "A");

  // C
  append_branch(&branches, node_rule("A"), 2);
  append_branch(&branches, node_add(node_rule("C"), node_rule("C")), 3);
  append_branch(&branches, node_mult(node_rule("C"), node_rule("C")), 3);
  grammar_append_branches(grammar, &branches, "C");

  GRAMMAR_PRINT_LN(*grammar);
  return SYMBOL("E");
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

bool compile_node_func_into_fragment_expression(String_Builder *sb, Node *expr) {
  switch (expr->kind) {
  case NK_X:
  case NK_Y:
  case NK_T:
    sb_append_cstr(sb, node_kind_string(expr->kind));
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
  case NK_GT: {
    sb_append_cstr(sb, "(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.lhs))
      return false;
    sb_append_cstr(sb, node_kind_operation(expr->kind));
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.rhs))
      return false;
    sb_append_cstr(sb, ")");
  } break;

  case NK_MOD: {
    sb_append_cstr(sb, "mod(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.lhs))
      return false;
    sb_append_cstr(sb, ",");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.binop.rhs))
      return false;
    sb_append_cstr(sb, ")");
  } break;

  case NK_SQRT: {
    sb_append_cstr(sb, "sqrt(");
    if (!compile_node_func_into_fragment_expression(sb, expr->as.unop))
      return false;
    sb_append_cstr(sb, ")");
  } break;

  case NK_TRIPLE: {
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
  } break;

  case NK_IF: {
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
  } break;

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
  sb_append_cstr(sb, "uniform float time;\n");

  sb_append_cstr(sb, "vec4 map_color(vec3 rgb) {\n");
  sb_append_cstr(sb, "  return vec4((rgb + 1)/2.0, 1.0);\n");
  sb_append_cstr(sb, "}\n");

  sb_append_cstr(sb, "void main() {\n");
  sb_append_cstr(sb, "  float x = fragTexCoord.x * 2.0 - 1.0;\n");
  sb_append_cstr(sb, "  float y = fragTexCoord.y * 2.0 - 1.0;\n");
  sb_append_cstr(sb, "  float t = sin(time);\n");
  sb_append_cstr(sb, "  finalColor = map_color(");
  if (!compile_node_func_into_fragment_expression(sb, f)) return false;
  sb_append_cstr(sb, ");\n");
  sb_append_cstr(sb, "}\n");

  return true;
}

int parse_optional_depth(char **argv, int argc_) {
    if (argc_ >= 1 && strcmp(argv[0], "-depth") == 0) {
      shift(argv, argc_);
      if (argc_ >= 1) {
        const char *depth_str = shift(argv, argc_);
        if (depth_str) {
          return atoi(depth_str);
        } 
      } else {
        nob_log(WARNING, "Expected value after -depth flag, using default depth: %d", GRAMMAR_DEPTH);
        return GRAMMAR_DEPTH;
      }
    } 
    nob_log(INFO, "No depth provided, using default depth: %d", GRAMMAR_DEPTH);
    return GRAMMAR_DEPTH;
}

bool parse_grammar_branch(Alexer *l, Grammar_Branch *branch) {
  // alexer_get_token(l, &t);
  // while (t.kind == ALEXER_PUNCT && t.punct_index == PUNCT_BAR) {
  //   weight += 1;
  //   alexer_get_token(&l, &t);
  // }
  // return false;
}

bool parse_grammar_branches(Alexer *l, Alexer_Token name, Grammar_Branches *branches) {
  branches->name = name;
  
  Alexer_Token t = {0};
  alexer_get_token(l, &t);

  size_t branch_start[] = {PUNCT_BAR, PUNCT_SEMICOLON};
  bool quit = false;
  
  while (!quit) {
    if (!alexer_expect_one_of_puncts(l, t, branch_start, ARRAY_LEN(branch_start))) return false;
    switch (t.punct_index) {
    case PUNCT_BAR: {
        Grammar_Branch branch = {.weight = 1};
        if (!parse_grammar_branch(l, &branch)) return false;
        arena_da_append(&node_arena, branches, branch);
      } break;

    case PUNCT_SEMICOLON: quit = true; break;
    default: UNREACHABLE_CODE("parse_grammar_branches");
    }
  }

  branches->weight_sum = 0;
  for (size_t i = 0; i < branches->count; ++i) {
    branches->weight_sum += branches->items[i].weight;
  }

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
      nob_log(ERROR, "Usage: %s %s <output_path> -depth <depth>", program_name, command_name);
      nob_log(ERROR, "No output path is provided");
      return 1;
    }

    const char *output_path = shift(argv, argc);

    // Node *f = gray_gradient_ast();
    // Node* f = cool_gradient_ast();
    Grammar grammar = {0};
    Alexer_Token entry = simple_grammar(&grammar);
    Node *f = gen_rule(grammar, entry, parse_optional_depth(argv, argc));
    if (!f) {
      nob_log(ERROR, "Process could not terminate\n");
      exit(69);
    }

    NODE_PRINT_LN(f);

    Image image = GenImageColor(IMAGE_WIDTH, IMAGE_HEIGHT, BLANK);
    if (!render_pixels(image, f)) return 1;
    if (!ExportImage(image, output_path)) return 1;

    return 0;
  }

  if (strcmp(command_name, "gui") == 0) {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "RandomArt");

    Grammar grammar = {0};
    Alexer_Token entry = simple_grammar(&grammar);
    Node *f = gen_rule(grammar, entry, parse_optional_depth(argv, argc));
    if (!f) {
      nob_log(ERROR, "Process could not terminate\n");
      exit(69);
    }
    // NODE_PRINT_LN(f);

    String_Builder sb = {0};
    if (!compile_node_func_into_fragment_shader(&sb, f))
      return 1;
    sb_append_null(&sb);
    printf("%s", sb.items);

    Shader shader = LoadShaderFromMemory(NULL, sb.items);

    int time_loc = GetShaderLocation(shader, "time");
    Texture default_texture = GetDefaultTexture();

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
      BeginDrawing();

      float time = GetTime();
      SetShaderValue(shader, time_loc, &time, SHADER_UNIFORM_FLOAT);

      BeginShaderMode(shader);
      DrawTexturePro(
        default_texture, 
        (Rectangle){0, 0, 1, 1},
        (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
        (Vector2){0},
        0, 
        WHITE
      );
      EndShaderMode();

      EndDrawing();
    }

    return 0;
  }

  if (strcmp(command_name, "parse") == 0) {
    if (argc <= 0) {
      nob_log(ERROR, "Usage: %s %s <input>", program_name, command_name);
      nob_log(ERROR, "No input is provided");
      return 1;
    }

    const char *input_path = shift(argv, argc);

    String_Builder sb = {0};
    if (!read_entire_file(input_path, &sb)) return 1;

    Alexer l = alexer_create(input_path, sb.items, sb.count);
    l.puncts = puncts;
    l.puncts_count = COUNT_PUNCTS;
    l.sl_comments = comments;
    l.sl_comments_count = ARRAY_LEN(comments);

    Alexer_Token t = {0};
    Alexer_Kind top_level_start[] = {ALEXER_SYMBOL, ALEXER_END};
    bool quit = false;

    Grammar grammar = {0};

    while (!quit) {
      alexer_get_token(&l, &t);
      if (!alexer_expect_one_of_kinds(&l, t, top_level_start, ARRAY_LEN(top_level_start))) return 1;

      switch (t.kind) {
      case ALEXER_SYMBOL: {
        Grammar_Branches branches = {0};
        if (!parse_grammar_branches(&l, t, &branches)) return 1;
        arena_da_append(&node_arena, &grammar, branches);
        return 0;
      } break;

      case ALEXER_END: quit = true; break;
      default:
        UNREACHABLE_CODE("top_level");
      }
    }
  }

  nob_log(ERROR, "Unknown command: %s", command_name);
  return 0;
}
