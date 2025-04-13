#include <string.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

/* ---------- */
#ifdef __linux__
#define RAYLIB_PATH "./lib/raylib/raylib-5.5_linux_amd64"
#define RAYLIB_LINKER "-l:libraylib.a"
#elif defined(_WIN32) || defined(_WIN64)
#define RAYLIB_PATH "./lib/raylib/raylib-5.5_windows"
#define RAYLIB_LINKER "./lib/raylib/raylib-5.5_windows/lib/libraylib.a"
#elif defined(__APPLE__)
#define RAYLIB_PATH "./lib/raylib/raylib-5.5_macos"
#define RAYLIB_LINKER "./lib/raylib/raylib-5.5_macos/lib/libraylib.a"
#else
#error "Unsupported platform"
#endif

#ifdef __APPLE__
#define builder_macos_frameworks(cmd)                                          \
  cmd_append(cmd, "-framework", "Cocoa", "-framework", "IOKit", "-framework",  \
             "CoreFoundation", "-framework", "CoreGraphics")
#else
#define builder_macos_frameworks(cmd)
#endif
/* ---------- */

#define builder_cc(cmd) cmd_append(cmd, "cc")
#define builder_output(cmd, output_path) cmd_append(cmd, "-o", output_path)
#define builder_inputs(cmd, ...) cmd_append(cmd, __VA_ARGS__)
#define builder_libs(cmd) cmd_append(cmd, "-lm")
#define builder_flags(cmd)                                                     \
  cmd_append(cmd, "-Wall", "-Wextra", "-Wswitch-enum", "-ggdb")
#define builder_include_path(cmd, include_path)                                \
  cmd_append(cmd, temp_sprintf("-I%s", include_path))
#define builder_raylib_include_path(cmd)                                       \
  cmd_append(cmd, temp_sprintf("-I%s/include/", RAYLIB_PATH))
#define builder_raylib_library_path(cmd)                                       \
  cmd_append(cmd, temp_sprintf("-L%s/lib/", RAYLIB_PATH), RAYLIB_LINKER)

void cmd_append_optional_depth(Nob_Cmd *cmd, char **argv, int argc_) {
  if (argc_ >= 1 && strcmp(argv[0], "-depth") == 0) {
    cmd_append(cmd, shift(argv, argc_));
    if (argc_ >= 1) {
      cmd_append(cmd, shift(argv, argc_));
    }
  }
}

void cmd_append_optional_grammar_path(Nob_Cmd *cmd, char **argv, int argc_) {
  const char *grammar_path = "./grammars/grammar.bnf";

  for (int i = 0; i < argc_ - 1; ++i) {
    if (strcmp(argv[i], "-grammar") == 0) {
      grammar_path = argv[i + 1];
      for (int j = i; j < argc_ - 2; ++j) {
        argv[j] = argv[j + 2];
      }
      argc_ -= 2;
      break;
    }
  }
  cmd_append(cmd, grammar_path);
}

int main(int argc, char *argv[]) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  Nob_Cmd cmd = {0};

  builder_cc(&cmd);
  builder_output(&cmd, "main");
  builder_inputs(&cmd, "main.c", "node.c");
  builder_libs(&cmd);
  builder_flags(&cmd);
  builder_raylib_include_path(&cmd);
  builder_raylib_library_path(&cmd);
  builder_macos_frameworks(&cmd);

  if (!cmd_run_sync_and_reset(&cmd))
    return 1;

  const char *program_name = shift(argv, argc);

  if (argc > 0) {
    const char *subcommand = shift(argv, argc);

    if (strcmp(subcommand, "run") == 0) {
      cmd_append(&cmd, "./main", "file", "output/random_image.png");
      cmd_append_optional_depth(&cmd, argv, argc);
      if (!cmd_run_sync_and_reset(&cmd))
        return 1;

      cmd_append(&cmd, "kitty", "icat", "output/random_image.png");
      if (!cmd_run_sync_and_reset(&cmd))
        return 1;
    } else if (strcmp(subcommand, "gui") == 0) {
      cmd_append(&cmd, "./main", "gui");
      cmd_append_optional_grammar_path(&cmd, argv, argc);
      cmd_append_optional_depth(&cmd, argv, argc);
      if (!cmd_run_sync_and_reset(&cmd))
        return 1;
    } else {
      nob_log(ERROR, "Unknown command: %s", subcommand);
      return 1;
    }
  }

  return 0;
}
