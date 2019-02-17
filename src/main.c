#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "./commons.h"
#include "./gui.h"

/*struct menu_select {
  bool quit;
  enum game_type type;
  
};

void menu_select_prompt(struct menu_select** selection);*/

enum direction get_player_direction(struct player_state* player, bool pop) {
  enum direction heading;
  if (player->queued_movements[0] == direction_none) {
    heading = player->head->data.p.heading;
    return heading;
  } else {
    if (pop) {
      heading = pop_queued_movement(player->queued_movements);
    } else {
      heading = player->queued_movements[0];
    }
    if (opposite_direction(heading) != player->head->data.p.heading) {
      return heading;
    } else {
      return player->head->data.p.heading;
    }
  }
}

void player_remove(struct game_state* state, struct player_state* player) {
  player->is_alive = false;
  struct board_tile* tile = player->tail;
  while (tile != NULL && tile->type == board_tile_type_player && tile->data.p.player == player) {
    tile->type = board_tile_type_empty;
    tile = find_game_tile_direction(player->tail, player->tail->data.p.heading, state->board_width, state->board_height, state->board);
  }
}

void player_update_tail(struct game_state* state, unsigned int player_id) {
  struct player_state* player = state->states + player_id;

  /* Shorten tail */
  if (!player->fed) {
    assert(player->tail->data.p.player == player);
    struct board_tile* new_tail;
    /* length of one, special case */
    if (player->tail == player->head) {
      new_tail = find_game_tile_direction(player->tail, get_player_direction(player, false), state->board_width, state->board_height, state->board);
    } else {
      new_tail = find_game_tile_direction(player->tail, player->tail->data.p.heading, state->board_width, state->board_height, state->board);
      assert(new_tail != NULL);
    }
    if (new_tail != NULL) {
      player->tail->type = board_tile_type_empty;
      player->tail = new_tail;
    } else {
      player_remove(state, player);
    }
  } else {
    player->fed = false;
  }
}

void handle_user_input(struct player_state* player) {
  while (getc_ready(STDIN_FILENO)) {
    char c = getc(stdin);
    if (c == '\033') {
      c = getc(stdin);
      if (c == '[') {
        c = getc(stdin);
        enum direction direction;
        switch (c) {
          case 'A':
            direction = direction_north;
            break;
          case 'B':
            direction = direction_south;
            break;
          case 'C':
            direction = direction_east;
            break;
          case 'D':
            direction = direction_west;
            break;
          default:
            direction = direction_none;
            break;
        }
        push_queued_movement(player->queued_movements, direction);
      } else {
        continue;
      }
    }
  }
}

void player_update_head(struct game_state* state, unsigned int player_id) {
  struct player_state* player = state->states + player_id;

  /* Lengthen head */
  enum direction heading = get_player_direction(player, true);

  struct board_tile* new_head = find_game_tile_direction(player->head, heading, state->board_width, state->board_height, state->board);
  /* Check for collisions, etc. */
  if (new_head == NULL) {
    /* We fell off the map */
    player_remove(state, player);
  } else if (new_head->type == board_tile_type_player) {
    /* TODO: Allow for non-incrementing player update cycles */
    bool other_player_moved = player > new_head->data.p.player;
    if (new_head->data.p.player->head == new_head && other_player_moved) {
      /* Ties are boring, they only happen if two *heads* would occupy the same tile *after both* have moved */
      /* Both moved, therefore it is a tie */
      player_remove(state, new_head->data.p.player);
    }
    player_remove(state, player);
  } else if (new_head->type == board_tile_type_food) {
    player->fed = true;
  }
  if (player->is_alive) {
    new_head->type = board_tile_type_player;
    new_head->data.p.heading = heading;
    new_head->data.p.player = player;

    /* Update the old head to point to the new head (not necessary if not change in direction) */
    player->head->data.p.heading = heading;

    player->head = new_head;
  }
}

void update_game_state(struct game_state* state) {
  for (unsigned int i = 0; i < state->player_count; ++i) {
    if (state->states[i].is_alive) {
      player_update_tail(state, i);
    }
  }
  for (unsigned int i = 0; i < state->player_count; ++i) {
    if (state->states[i].is_alive) {
      player_update_head(state, i);
    }
  }
}

int count_living_players(struct game_state* state) {
  unsigned int count = 0;
  for (unsigned int i = 0; i < state->player_count; ++i) {
    if (state->states[i].is_alive) {
      ++count;
    }
  }
  return count;
}

int main(int argc, char** argv) {

  struct game_state state;
  /* Top row of the terminal is for the scoreboard */
  state.board_height = (24 - 1) * 2;
  state.board_width = 80;
  state.player_count = 0;
  state.board = malloc(state.board_width * state.board_height * sizeof(struct board_tile));

  for (int i = 0; i < state.board_width * state.board_height; ++i) {
    state.board[i].type = board_tile_type_empty;
  }

  /* TODO allow multiplayer */

  {
    state.player_count = 1;
    state.states = malloc(sizeof(struct player_state));
    assert(state.states != NULL);
    state.states[0].id = 0;
    state.states[0].score = 0;
    state.states[0].fed = false;
    state.states[0].is_alive = true;


    struct board_tile* head_tile = find_game_tile(10, 10, state.board_width, state.board_height, state.board);
    head_tile->type = board_tile_type_player;
    head_tile->data.p.heading = direction_north;
    head_tile->data.p.player = state.states + 0;

    state.states[0].head = head_tile;
    state.states[0].tail = head_tile;

    for (unsigned int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE; ++i) {
      state.states[0].queued_movements[i] = direction_none;
    }

    find_game_tile(40, 25, state.board_width, state.board_height, state.board)->type = board_tile_type_food;
    find_game_tile(0, 1, state.board_width, state.board_height, state.board)->type = board_tile_type_food;
    find_game_tile(4, 0, state.board_width, state.board_height, state.board)->type = board_tile_type_food;
    find_game_tile(79, 0, state.board_width, state.board_height, state.board)->type = board_tile_type_food;
    find_game_tile(0, 4, state.board_width, state.board_height, state.board)->type = board_tile_type_food;
    find_game_tile(0, 5, state.board_width, state.board_height, state.board)->type = board_tile_type_food;
  }

  if (!setup_terminal()) {
    printf("Error preparing terminal\n");
    return 1;
  }

  unsigned int tick_count = 0;

  struct timespec time_start;
  struct timespec time_end;
  while (true) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &time_start);

    /* Update game state */
    update_game_state(&state);
    /* TODO: Check which players are still alive */
    if (false /* TODO check if multiplayer*/) {
      if (count_living_players(&state) <= 1) {
        break;
      }
    } else if (count_living_players(&state) == 0) {
      print_screen(&state);
      break;
    }
    /* Render game */
    clear_screen_or_reset_cursor();
    /* TODO print scores */
    printf("Tick: %d\n", tick_count % 256);
    print_screen(&state);
    /* Wait until a tick has passed from start of loop */
    clock_gettime(CLOCK_MONOTONIC_RAW, &time_end);
    unsigned long diff = timespec_us_diff(&time_start, &time_end);
    if (TICK_DURATION_US > diff) {
      usleep(TICK_DURATION_US - diff);
    }
    /* Check for user input and handle it */
    handle_user_input(state.states + 0);

    ++tick_count;
  }

  reset_terminal();

  return 0;
}
