#ifndef COMMONS_H
#define COMMONS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define TICK_RATE 5
#define TICK_DURATION_US 1000*1000/TICK_RATE

struct player_state;

enum board_tile_type {
  board_tile_type_player,
  board_tile_type_empty,
  board_tile_type_food
};

enum direction {
  direction_north,
  direction_south,
  direction_east,
  direction_west,
  direction_none
};

struct player_tile {
  struct player_state* player;
  /* points in the direction of the head
     (following the path consecutively
     direction_none for the head. */
  enum direction heading;
};

union board_tile_data {
  /* I may add other types? */
  struct player_tile p;
};

/* (0,0) is the bottom left */
struct board_tile {
  enum board_tile_type type;
  union board_tile_data data;
};

#define PLAYER_QUEUED_MOVEMENTS_SIZE 8

struct player_state {
  unsigned int id;
  unsigned int score;
  bool fed;
  bool is_alive;
  enum direction queued_movements[PLAYER_QUEUED_MOVEMENTS_SIZE];
  /**/
  struct board_tile* head;
  struct board_tile* tail;
};

struct game_state {
  unsigned int player_count;
  struct player_state* states;
  unsigned int board_width;
  unsigned int board_height;
  struct board_tile* board;
};

#define find_game_tile(x, y, width, height, board) ((struct board_tile*) find_const_game_tile((x), (y), (width), (height), (board)))

/* returns a pointer to the tile *inside* the board (not a copy) */
const struct board_tile* find_const_game_tile(unsigned int x, unsigned int y, unsigned int width, unsigned int height, const struct board_tile* board);

struct board_tile* find_game_tile_direction(struct board_tile* tile, enum direction direction, unsigned int width, unsigned int height, const struct board_tile* board);

/* Difference in microseconds rounded arbitrarily */
unsigned long timespec_us_diff(const struct timespec* start, const struct timespec* end);

enum direction opposite_direction(enum direction direction);

/* Ignores the value if the array is already full */
void push_queued_movement(enum direction queued_movements[PLAYER_QUEUED_MOVEMENTS_SIZE], enum direction new_direction);

enum direction pop_queued_movement(enum direction queued_movements[PLAYER_QUEUED_MOVEMENTS_SIZE]);

bool getc_ready(int file_descriptor);

bool setup_terminal(void);

bool reset_terminal(void);

#endif
