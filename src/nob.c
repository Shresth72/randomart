#include <string.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

#define BUILD_FOLDER "build/"

#define builder_cc(cmd) cmd_append(cmd, "cc")
#define builder_output(cmd, output_path) cmd_append(cmd, "-o", output_path)
#define builder_inputs(cmd, ...) cmd_append(cmd, __VA_ARGS__)
#define builder_libs(cmd) cmd_append(cmd, "-lm")
#define builder_flags(cmd)                                                     \
  cmd_append(cmd, "-Wall", "-Wextra", "-Wswitch-enum", "-ggdb")
#define builder_include_path(cmd, include_path)                                \
  cmd_append(cmd, temp_sprintf("-I%s", include_path))

#define builder_raylib_include_path(cmd)                                       \
  cmd_append(cmd, "-I./lib/raylib/raylib-5.5_linux_amd64/include")
#define builder_rayblib_library_path(cmd)                                      \
  cmd_append(cmd, "-L./lib/raylib/raylib-5.5_linux_amd64/lib", "-l:libraylib.a")

int main(int argc, char *argv[]) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  Nob_Cmd cmd = {0};

  builder_cc(&cmd);
  builder_output(&cmd, "main");
  builder_inputs(&cmd, "main.c", "node.c", "grammar.c");
  builder_libs(&cmd);
  builder_flags(&cmd);
  builder_raylib_include_path(&cmd);
  builder_rayblib_library_path(&cmd);

  if (!cmd_run_sync_and_reset(&cmd))
    return 1;

  if (argc > 1) {
    if (strcmp(argv[1], "run") == 0) {
      cmd_append(&cmd, "./main", "file", "output/random_image.png");
      if (!cmd_run_sync_and_reset(&cmd))
        return 1;

      cmd_append(&cmd, "kitty", "icat", "output/random_image.png");
      if (!cmd_run_sync_and_reset(&cmd))
        return 1;
    } else if (strcmp(argv[1], "gui") == 0) {
      cmd_append(&cmd, "./main", "gui");
      if (!cmd_run_sync_and_reset(&cmd))
        return 1;
    }
  }

  return 0;
}
