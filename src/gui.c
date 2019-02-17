#include <assert.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "./commons.h"

void clear_screen_or_reset_cursor(void) {
  /* Unix only */
  fputs("\033[J\033[1J\033[1;1H", stdout);
}

/* Maybe make the game slightly better eventually, but whatever */
void print_screen(const struct game_state* state) {
  //find_const_game_tile
  assert(state->board_width % 2 == 0);
  assert(state->board_height >= 1);
  unsigned int y = state->board_height;
  while (y > 0) {
    y -= 2;
    for (unsigned int x = 0; x < state->board_width; ++x) {
      const struct board_tile* bottom_tile = find_const_game_tile(x, y, state->board_width, state->board_height, state->board);
      const struct board_tile* top_tile = find_const_game_tile(x, y + 1, state->board_width, state->board_height, state->board);
      bool bottom_filled = (bottom_tile->type == board_tile_type_player || bottom_tile->type == board_tile_type_food);
      bool top_filled = (top_tile->type == board_tile_type_player || top_tile->type == board_tile_type_food);

      int type = (top_filled ? 2 : 0) + (bottom_filled ? 1 : 0);
      /* Probably the compiler hates me for this, but I think it is easier to read. */
      char* c;
      switch (type) {
        case 0:
          c = " ";
          break;
        case 1:
          c = "▄";
          break;
        case 2:
          c = "▀";
          break;
        case 3:
          c = "█";
          break;
      }
      printf("%s", c);
    }
  };
  fflush(stdout);
}
