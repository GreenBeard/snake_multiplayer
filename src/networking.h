#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/socket.h>

#include "./commons.h"

enum game_packets {
  /* Only sent by client */
  game_packet_join_server,
  /* Only sent by server */
  game_packet_join_response,
  /* Sent by client and relayed by server */
  game_packet_movement,
  /* TODO: implement checksum validation */
  game_packet_checksum,
  /* Only sent by server */
  game_packet_player_died,
  /* Sent by both the server and client */
  game_packet_heartbeat,
  /* Only sent by server after a player has been added */
  game_packet_player_join
};

struct game_packet_player_died {
  unsigned int player_id;
};

struct game_packet_player_join {
  /* TODO add protocol version */
  unsigned int player_id;
  char player_name[PLAYER_NAME_SIZE];
  unsigned int x_position;
  unsigned int y_position;
};

struct game_food_spawn {
  unsigned int x_position;
  unsigned int y_position;
};

struct game_packet_movement {
  unsigned int player_id;
  enum direction directions[PLAYER_QUEUED_MOVEMENTS_SIZE];
  /* Starts at head and ends at the tail */
  unsigned int pos_length;
  unsigned int* pos_x;
  unsigned int* pos_y;
};

struct game_packet_join_server {
  char player_name[PLAYER_NAME_SIZE];
};

struct game_packet_join_response {
  /* player_id may contain anything if accepted is false */
  unsigned int player_id;
  bool accepted;
  unsigned int board_height;
  unsigned int board_width;
};

struct network_state {
  struct timespec last_message_timestamp;
  /* false if client */
  bool is_server;
  int socket_fd;
  /* Length of one if client (and connected to a server) */
  unsigned int addresses_size;
  /* type: struct sockaddr* (note the pointer) */
  struct lazy_list* addresses;
  /* type: socklen_t */
  struct lazy_list* address_lengths;
};

void add_address_to_state(struct network_state* state, struct sockaddr* address, socklen_t length);

void send_game_packet(const struct network_state* net_state, void* packet, enum game_packets packet_type);

int setup_connection(bool server, int* connection_fd, const char* name, const char* service);

#endif
