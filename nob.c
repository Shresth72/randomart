#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

int main(int argc, char *argv[]) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  Nob_Cmd cmd = {0};
  cmd_append(&cmd, "cc", "-Wall", "-Wextra");
  cmd_append(&cmd, "-o", "randomart", "randomart.c");
  cmd_append(&cmd, "-lm");

  if (!cmd_run_sync_and_reset(&cmd))
    return 1;

  // cmd_append(&cmd, "./randomart");
  // if (!cmd_run_sync_and_reset(&cmd))
  //   return 1;
  //
  // cmd_append(&cmd, "kitty", "icat", "output_path.png");
  // if (!cmd_run_sync_and_reset(&cmd))
  //   return 1;

  return 0;
}
