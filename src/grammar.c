#include "grammar.h"

void grammar_print(Grammar grammar) {
  for (size_t i = 0; i < grammar.count; ++i) {
    printf("%zu ::= ", i);
    Grammar_Branches *branches = &grammar.items[i];
    for (size_t j = 0; j < branches->count; ++j) {
      if (j > 0)
        printf(" | ");
      node_print(branches->items[j].node);
      printf(" [%.02f]", branches->items[i].probability);
    }
    printf("\n");
  }
}
