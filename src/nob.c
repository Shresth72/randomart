#include <string.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "lib/nob.h"

int main(int argc, char *argv[]) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  Nob_Cmd cmd = {0};
  cmd_append(&cmd, "cc");
  cmd_append(&cmd, "-o", "main");
  cmd_append(&cmd, "main.c", "node.c");
  cmd_append(&cmd, "-lm");
  cmd_append(&cmd, "-Wall", "-Wextra", "-Wswitch-enum");

  if (!cmd_run_sync_and_reset(&cmd))
    return 1;

  if (argc > 1 && strcmp(argv[1], "run") == 0) {
    cmd_append(&cmd, "./main");
    if (!cmd_run_sync_and_reset(&cmd))
      return 1;

    cmd_append(&cmd, "kitty", "icat", "output/output_path.png");
    if (!cmd_run_sync_and_reset(&cmd))
      return 1;
  }

  return 0;
}
