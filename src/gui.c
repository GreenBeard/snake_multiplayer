#include <assert.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "./commons.h"
#include "./menu_select.h"

void clear_screen_or_reset_cursor(void) {
  /* Unix only */
  fputs("\033[J\033[1J\033[1;1H", stdout);
}

/* Maybe make the game slightly better eventually, but whatever */
void print_screen(const struct game_state* state) {
  /* TODO print scores instead of tick count */
  static unsigned int tick_count = 0;
  ++tick_count;
  printf("Tick: %d\n", tick_count % 256);

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
    /* TODO not print new line for the last line*/
    if (y != 0) {
      printf("\n");
    }
  };
  fflush(stdout);
}

void menu_print_text_choice(const char* prompt, size_t buffer_size, char* buffer) {
  assert(buffer_size > 0);
  printf("%s\n> ", prompt);
  for (size_t i = 0; i < buffer_size - 1; ++i) {
    buffer[i] = getc(stdin);
    printf("%c", buffer[i]);
    if (buffer[i] == '\n') {
      buffer[i] = '\0';
      return;
    }
  }
  /* Read as many characters as we could without a buffer overflow */
  buffer[buffer_size - 1] = '\0';
}

unsigned int menu_print_select_choice(const char* title, size_t menu_items_size, const char** menu_items, unsigned int width, unsigned int height) {
  print_menu(title, menu_items_size, menu_items, 0, width, height);
  printf("> ");
  fflush(stdout);
  while (true) {
    char c = getc(stdin);
    if (c >= '0' && c <= '9') {
      unsigned int num = c - '0';
      if (num < menu_items_size) {
        printf("%c\n", c);
        return num;
      }
    }
  }
}

struct termios original_term;
bool reset_terminal(void) {
  if (tcsetattr(STDIN_FILENO, TCSANOW, &original_term) == 0) {
    while (is_fd_ready(STDIN_FILENO)) {
      getc(stdin);
    }
    return true;
  } else {
    return false;
  }
}

bool setup_terminal(void) {
  /* disable input buffering */
  setvbuf(stdin, NULL, _IONBF, 0);

  /* reset the terminal screen (cursor, scrolling, etc.) */
  printf("\033c");
  struct termios term;

  if (tcgetattr(STDIN_FILENO, &term) == 0) {
    original_term = term;
    term.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) == 0) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}
