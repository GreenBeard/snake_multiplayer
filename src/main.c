#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./commons.h"
#include "./gui.h"
#include "./networking.h"

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

void player_remove(struct game_state* state, const struct network_state* net_state, struct player_state* player) {
  player->is_alive = false;
  struct board_tile* tile = player->tail;
  while (tile != NULL && tile->type == board_tile_type_player && tile->data.p.player == player) {
    tile->type = board_tile_type_empty;
    tile = find_game_tile_direction(player->tail, player->tail->data.p.heading, state->board_width, state->board_height, state->board);
  }

  struct game_packet_player_died packet;
  packet.player_id = player->id;
  if (net_state->is_server) {
    send_game_packet(net_state, &packet, game_packet_player_died);
  }
}

void player_update_tail(struct game_state* state, const struct network_state* net_state, unsigned int player_id) {
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
      player_remove(state, net_state, player);
    }
  } else {
    player->fed = false;
  }
}

void handle_user_input(const struct network_state* net_state, struct player_state* player_one, struct player_state* player_two) {
  while (is_fd_ready(STDIN_FILENO)) {
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
        push_queued_movement(player_one->queued_movements, direction);
        struct game_packet_movement packet;
        memcpy(packet.directions, player_one->queued_movements, sizeof(packet.directions));
        send_game_packet(net_state, &packet, game_packet_movement);
      } else {
        continue;
      }
    } else {
      if (player_two != NULL) {
        enum direction direction;
        switch (c) {
          case 'w':
            direction = direction_north;
            break;
          case 's':
            direction = direction_south;
            break;
          case 'd':
            direction = direction_east;
            break;
          case 'a':
            direction = direction_west;
            break;
          default:
            direction = direction_none;
            break;
        }
        push_queued_movement(player_two->queued_movements, direction);
        /* TODO send game packet for player two */
      }
    }
  }
}

void player_update_head(struct game_state* state, const struct network_state* net_state, unsigned int player_id) {
  struct player_state* player = state->states + player_id;

  /* Lengthen head */
  enum direction heading = get_player_direction(player, true);

  struct board_tile* new_head = find_game_tile_direction(player->head, heading, state->board_width, state->board_height, state->board);
  /* Check for collisions, etc. */
  if (new_head == NULL) {
    /* The player fell/walked off the map */
    player_remove(state, net_state, player);
    /* To make clang's static analyzer happy */
    assert(!player->is_alive);
  } else if (new_head->type == board_tile_type_player) {
    /* TODO: Allow for non-incrementing player update cycles */
    bool other_player_moved = player > new_head->data.p.player;
    if (new_head->data.p.player->head == new_head && other_player_moved) {
      /* Ties are boring, they only happen if two *heads* would occupy the same tile *after both* have moved */
      /* Both moved, therefore it is a tie */
      player_remove(state, net_state, new_head->data.p.player);
    }
    player_remove(state, net_state, player);
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

void update_game_state(struct game_state* state, const struct network_state* net_state) {
  for (unsigned int i = 0; i < state->player_count; ++i) {
    if (state->states[i].is_alive) {
      player_update_tail(state, net_state, i);
    }
  }
  for (unsigned int i = 0; i < state->player_count; ++i) {
    if (state->states[i].is_alive) {
      player_update_head(state, net_state, i);
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

bool init_game_state(struct game_state* state) {
  /* Top row of the terminal is for the scoreboard */
  state->board_height = (24 - 1) * 2;
  state->board_width = 80;
  state->player_count = 0;
  state->board = malloc(state->board_width * state->board_height * sizeof(struct board_tile));
  if (state->board != NULL) {
    for (int i = 0; i < state->board_width * state->board_height; ++i) {
      state->board[i].type = board_tile_type_empty;
    }
    return true;
  } else {
    return false;
  }
}

/* Addes a one long player into the game with id at start_tile */
bool init_player(struct player_state* state, unsigned int id, struct board_tile* start_tile) {
  state->id = id;
  state->score = 0;
  state->fed = false;
  state->is_alive = true;


  struct board_tile* head_tile = start_tile;
  head_tile->type = board_tile_type_player;
  head_tile->data.p.heading = direction_north;
  head_tile->data.p.player = state;

  state->head = head_tile;
  state->tail = head_tile;

  for (unsigned int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE; ++i) {
    state->queued_movements[i] = direction_none;
  }
  return true;
}

/* Attempts to a food tile (sometimes fails if the randomly selected tile is already full) */
void add_food_tile(struct board_tile* board, unsigned int width, unsigned int height) {
  long num = random() % (width * height);
  unsigned int x = num % width;
  unsigned int y = num / width;
  struct board_tile* tile = find_game_tile(x, y, width, height, board);
  if (tile->type == board_tile_type_empty) {
    tile->type = board_tile_type_food;
  }
}

unsigned int count_food_tiles(struct board_tile* board, unsigned int width, unsigned int height) {
  unsigned int count = 0;
  for (unsigned int i = 0; i < width * height; ++i) {
    if (board[i].type == board_tile_type_food) {
      ++count;
    }
  }
  return count;
}

int main(int argc, char** argv) {
  /*printf("Server or client? [s,c]\n");
  bool server;
  bool set = false;
  while (!set) {
    char c;
    scanf("%c", &c);
    switch (c) {
      case 's':
        server = true;
        set = true;
        break;
      case 'c':
        server = false;
        set = true;
        break;
      default:
        break;
    }
  }
  if (server) {
    server_socket_setup();
  } else {
    client_socket_setup("127.0.0.1");
  }
  return 0;*/

  unsigned int local_player_index = 0;

  struct game_state state;
  struct network_state net_state;

  if (!init_game_state(&state)) {
    return 1;
  }

  if (!setup_terminal()) {
    printf("Error preparing terminal\n");
    return 1;
  }

  const char* options[] = { "Multiplayer", "Single player", "Settings (TODO)" };
  unsigned int choice = menu_print_select_choice("Main Menu", 3, options, state.board_width, state.board_height);
  bool multiplayer;
  if (choice == 0) {
    const size_t buffer_size = 40;
    char buffer[buffer_size];
    while (true) {
      menu_print_text_choice("Please enter the remote IP4 address", buffer_size, buffer);
      if (setup_connection(false, &net_state.socket_fd, buffer, "-1") == 0) {
        printf("Connecting to server...\n");
        /* Actually try to connect */
        exit(1);
        break;
      } else {
        printf("Error invalid IP4 address\n");
      }
    }
    multiplayer = true;
    assert(false);
  } else if (choice == 1) {
    net_state.is_server = false;
    net_state.socket_fd = -1;
    net_state.addresses_size = 0;
    net_state.addresses = NULL;
    net_state.address_lengths = NULL;
    net_state.last_message_timestamp.tv_nsec = 0;
    net_state.last_message_timestamp.tv_sec = 0;
    multiplayer = false;
  } else {
    return 1;
  }

  /* TODO allow multiplayer */

  unsigned int food_size;
  {
    state.player_count = 1;

    food_size = 1 + 2 * state.player_count;

    state.states = malloc(MAX_PLAYER_COUNT * sizeof(struct player_state));
    assert(state.states != NULL);

    /* TODO add proper id */
    init_player(state.states + local_player_index, 0, find_game_tile(10, 10, state.board_width, state.board_height, state.board));
    //init_player(state.states + 1, 1, find_game_tile(70, 10, state.board_width, state.board_height, state.board));

    for (unsigned int i = 0; i < food_size; ++i) {
      add_food_tile(state.board, state.board_width, state.board_height);
    }
  }

  unsigned int tick_count = 0;

  struct timespec time_start;
  struct timespec time_end;
  while (true) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &time_start);

    /* Update game state */
    update_game_state(&state, &net_state);
    /* TODO: Check which players are still alive */
    unsigned int living_player_count = count_living_players(&state);
    if (multiplayer) {
      if (living_player_count == 1) {
        struct player_state* alive_player = NULL;
        for (unsigned int i = 0; i < state.player_count; ++i) {
          if (state.states[i].is_alive) {
            alive_player = state.states + i;
            break;
          }
        }
        assert(alive_player != NULL);
        /* Humans prefer one-based indices */
        printf("Player %d won\n", alive_player->id + 1);
        break;
      } else if (living_player_count == 0) {
        printf("The game was a tie\n");
        break;
      }
    } else if (living_player_count == 0) {
      print_screen(&state);
      printf("Game over\n");
      break;
    }
    unsigned int food_count = count_food_tiles(state.board, state.board_width, state.board_height);
    for (int i = 0; i + food_count < food_size; ++i) {
      add_food_tile(state.board, state.board_width, state.board_height);
    }
    /* Render game */
    clear_screen_or_reset_cursor();
    print_screen(&state);
    /* Wait until a tick has passed from start of loop */
    clock_gettime(CLOCK_MONOTONIC_RAW, &time_end);
    unsigned long diff = timespec_us_diff(&time_start, &time_end);
    if (TICK_DURATION_US > diff) {
      usleep(TICK_DURATION_US - diff);
    }
    /* Check for user input and handle it */
    handle_user_input(&net_state, state.states + local_player_index, state.states + 1);

    ++tick_count;
  }

  reset_terminal();

  free(state.states);
  free(state.board);

  return 0;
}
