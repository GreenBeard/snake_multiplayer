#include "./commons.h"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

const struct board_tile* find_const_game_tile(unsigned int x, unsigned int y, unsigned int width, unsigned int height, const struct board_tile* board) {
  size_t offset = width * y + x;
  return board + offset;
}

struct board_tile* find_game_tile_direction(struct board_tile* tile, enum direction direction, unsigned int width, unsigned int height, const struct board_tile* board) {
  /* TODO return NULL for off the map east and west */
  ptrdiff_t offset;
  switch (direction) {
    case direction_north:
      offset = ((ptrdiff_t) width);
      break;
    case direction_south:
      offset = -((ptrdiff_t) width);
      break;
    case direction_east:
      offset = 1;
      break;
    case direction_west:
      offset = -1;
      break;
    default:
      assert(false);
  }
  if (board > tile + offset || tile + offset - board >= width * height) {
    return NULL;
  } else {
    return tile + offset;
  }
}

unsigned long timespec_us_diff(const struct timespec* start, const struct timespec* end) {
  long second_diff = end->tv_sec - start->tv_sec;
  assert(second_diff >= 0);
  if (second_diff >= (ULONG_MAX / 1000000) - 1) {
    /* May round up by less than a second if it is really close to overflowing, who cares though */
    return ULONG_MAX;
  } else {
    return second_diff / 1000000 + (end->tv_nsec - start->tv_nsec) / 1000;
  }
}

enum direction opposite_direction(enum direction direction) {
  switch (direction) {
    case direction_north:
      return direction_south;
      break;
    case direction_south:
      return direction_north;
      break;
    case direction_west:
      return direction_east;
      break;
    case direction_east:
      return direction_west;
      break;
    default:
      assert(false);
      break;
  }
}

void push_queued_movement(enum direction queued_movements[PLAYER_QUEUED_MOVEMENTS_SIZE], enum direction new_direction) {
  for (int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE; ++i) {
    if (queued_movements[i] == direction_none) {
      queued_movements[i] = new_direction;
      break;
    }
  }
}

enum direction pop_queued_movement(enum direction queued_movements[PLAYER_QUEUED_MOVEMENTS_SIZE]) {
  enum direction direction = queued_movements[0];
  for (int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE - 1; ++i) {
    queued_movements[i] = queued_movements[i + 1];
  }
  queued_movements[PLAYER_QUEUED_MOVEMENTS_SIZE - 1] = direction_none;
  return direction;
}

bool getc_ready(int file_descriptor) {
  int status;

  #ifdef _WIN32
  fd_set stdin_set;
  FD_ZERO(&stdin_set);
  FD_SET(file_descriptor, &stdin_set);

  struct timeval zero_timeval;
  zero_timeval.tv_sec = 0;
  zero_timeval.tv_usec = 0;

  status = select(file_descriptor + 1, &stdin_set, NULL, NULL, &zero_timeval);*/

  #else
  struct pollfd pfd;
  pfd.fd = file_descriptor;
  pfd.events = POLLIN;

  status = poll(&pfd, 1, 0);
  #endif

  if (status == 1) {
    return true;
  } else {
    return false;
  }
}

struct termios original_term;
bool reset_terminal(void) {
  if (tcsetattr(STDIN_FILENO, TCSANOW, &original_term) == 0) {
    while (getc_ready(STDIN_FILENO)) {
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
