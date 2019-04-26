#include <assert.h>
#define __USE_MISC
#include <endian.h>
#undef __USE_MISC
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#include "./commons.h"
#include "./lazy_list.h"
#include "./networking.h"

/* "SNAK" (missing E) if converted using dial pad */
#define GAME_PORT 7625

#define ret_unsigned_number_host_to_network(number) \
  if (sizeof(number) * CHAR_BIT == 16) { \
    return htobe16(number); \
  } else if (sizeof(number) * CHAR_BIT == 32) { \
    return htobe32(number); \
  } else if (sizeof(number) * CHAR_BIT == 64) { \
    return htobe64(number); \
  }

#define ret_unsigned_number_network_to_host(number) \
  if (sizeof(number) * CHAR_BIT == 16) { \
    return be16toh(number); \
  } else if (sizeof(number) * CHAR_BIT == 32) { \
    return be32toh(number); \
  } else if (sizeof(number) * CHAR_BIT == 64) { \
    return be64toh(number); \
  }

char hton_bool(bool number) {
  return (char) number;
}

unsigned int hton_uint(unsigned int number) {
  ret_unsigned_number_host_to_network(number);
}

unsigned short hton_ushort(unsigned short number) {
  ret_unsigned_number_host_to_network(number);
}

unsigned short hton_ulong(unsigned short number) {
  ret_unsigned_number_host_to_network(number);
}

unsigned int ntoh_uint(unsigned int number) {
  ret_unsigned_number_network_to_host(number);
}

unsigned short ntoh_ushort(unsigned short number) {
  ret_unsigned_number_network_to_host(number);
}

unsigned long ntoh_ulong(unsigned long number) {
  ret_unsigned_number_network_to_host(number);
}

bool ntoh_bool(char number) {
  return (bool) number;
}

void add_address_to_state(struct network_state* state, struct sockaddr* address, socklen_t length) {
  /* TODO actually check return values */
  add_to_lazy_list(state->addresses, address);
  add_to_lazy_list(state->addresses, &length);
  ++state->addresses_size;
}

void memcpy_offset_dest(void* restrict unoffset_destination, void* restrict src,
    size_t src_size, size_t* used_buffer_size) {
  *used_buffer_size = *used_buffer_size + src_size;
  memcpy(((char*) unoffset_destination) + *used_buffer_size, src, src_size);
}

void memcpy_offset_src(void* restrict destination, void* restrict unoffset_src,
    size_t src_size, size_t* used_buffer_size) {
  *used_buffer_size = *used_buffer_size + src_size;
  memcpy(destination, ((char*) unoffset_src) + *used_buffer_size, src_size);
}

void deserialize_packet_header(void* data, size_t data_size, unsigned int* packet_id,
    enum game_packets* game_packet_type, size_t* used_size) {
  char* char_data = data;
  *packet_id = ntoh_uint(*(unsigned int*) (char_data + *used_size));
  *used_size += sizeof(*packet_id);
  *game_packet_type = ntoh_uint(*(unsigned int*) (char_data + *used_size));
  *used_size += sizeof(*game_packet_type);
}

size_t serialize_packet_header(void* buffer, size_t packet_max_size, enum game_packets packet_type) {
  static unsigned int packet_id = 0;

  unsigned int network_id;
  unsigned int game_packet_type;
  size_t required_buffer_size = sizeof(game_packet_type) + sizeof(packet_id);
  size_t used_size = 0;
  if (packet_max_size >= required_buffer_size) {
    network_id = hton_uint(packet_id);
    memcpy_offset_dest(buffer, &network_id, sizeof(packet_id), &used_size);

    assert(sizeof(enum game_packets) == sizeof(unsigned int));
    game_packet_type = hton_uint(packet_type);
    memcpy_offset_dest(buffer, &game_packet_type, sizeof(game_packet_type), &used_size);

    ++packet_id;

    return used_size;
  } else {
    return packet_max_size;
  }
}

bool init_game_packet_movement(const struct game_state* game_state, struct player_state* player, struct game_packet_movement* packet) {
  packet->player_id = player->id;
  memcpy(packet->directions, player->queued_movements, sizeof(enum direction) * PLAYER_QUEUED_MOVEMENTS_SIZE);
  packet->pos_length = 0;
  struct board_tile* tile = player->tail;
  while (tile != player->head) {
    ++packet->pos_length;
    tile = find_game_tile_direction(tile, tile->data.p.heading, game_state->board_width, game_state->board_height, game_state->board);
  }

  packet->pos_x = malloc(sizeof(unsigned int) * packet->pos_length);
  packet->pos_y = malloc(sizeof(unsigned int) * packet->pos_length);

  if (packet->pos_x != NULL && packet->pos_y != NULL) {
    for (unsigned int i = 0; i < packet->pos_length; ++i) {
      find_game_tile_pos(tile, packet->pos_x + i, packet->pos_y + i, game_state->board_width, game_state->board_height, game_state->board);
      tile = find_game_tile_direction(tile, tile->data.p.heading, game_state->board_width, game_state->board_height, game_state->board);
    }
    return true;
  } else {
    return false;
  }

}

void* deserialize_packet(void* data, size_t data_size, enum game_packets* game_packet_type) {
  char* char_data = data;

  size_t used_size = 0;
  unsigned int packet_id;
  /* TODO check packet_id if it is a duplicate */
  /* TODO check the mallocs in this function */
  /* TODO check for underflow!!! */
  deserialize_packet_header(char_data, data_size, &packet_id, game_packet_type, &used_size);

  switch (*game_packet_type) {
    case game_packet_join_server:
      {
        struct game_packet_join_server* packet;
        size_t required_buffer_size = PLAYER_NAME_SIZE;
        if (data_size >= required_buffer_size) {
          packet = malloc(sizeof(struct game_packet_join_server));

          memcpy_offset_src(packet->player_name, char_data, PLAYER_NAME_SIZE, &used_size);

          assert(required_buffer_size == used_size);

          return packet;
        } else {
          return NULL;
        }
      }
    case game_packet_join_response:
      {
        struct game_packet_join_response* packet;
        size_t required_buffer_size = sizeof(char) + sizeof(packet->player_id);
        if (data_size >= required_buffer_size) {
          packet = malloc(sizeof(struct game_packet_join_response));

          packet->accepted = ntoh_bool(char_data[used_size]);
          used_size += sizeof(char);

          packet->player_id = ntoh_uint(*(unsigned int*) (char_data + used_size));
          used_size += sizeof(packet->player_id);

          assert(required_buffer_size == used_size);

          return packet;
        } else {
          return NULL;
        }
      }
    case game_packet_player_join:
      {
        struct game_packet_player_join* packet;
        size_t required_buffer_size = sizeof(packet->player_id) + sizeof(packet->player_name);
        if (data_size >= required_buffer_size) {
          packet = malloc(sizeof(struct game_packet_player_join));

          packet->player_id = ntoh_uint(*(unsigned int*) (char_data + used_size));
          used_size += sizeof(packet->player_id);

          memcpy_offset_src(packet->player_name, char_data, PLAYER_NAME_SIZE, &used_size);

          assert(required_buffer_size == used_size);

          return packet;
        } else {
          return NULL;
        }
      }
    case game_packet_movement:
      {
        struct game_packet_movement* packet;
        size_t required_buffer_size = sizeof(packet->player_id) + sizeof(packet->directions) + sizeof(packet->pos_length);
        if (data_size >= required_buffer_size) {
          packet = malloc(sizeof(struct game_packet_movement));

          packet->player_id = ntoh_uint(*(unsigned int*) (char_data + used_size));
          used_size += sizeof(packet->player_id);

          assert(sizeof(packet->directions[0]) == sizeof(int));
          for (unsigned int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE; ++i) {
            packet->directions[i] = ntoh_uint(*(unsigned int*) (char_data + used_size));
            used_size += sizeof(packet->directions[0]);
          }

          packet->pos_length = ntoh_uint(*(unsigned int*) (char_data + used_size));
          used_size += sizeof(packet->pos_length);

          assert(required_buffer_size == used_size);

          size_t new_length_required = packet->pos_length * (sizeof(packet->pos_x[0]) + sizeof(packet->pos_y[0]));
          /* TODO safer checks */
          assert(packet->pos_length <= 500);
          packet->pos_x = malloc(packet->pos_length * sizeof(packet->pos_x[0]));
          packet->pos_y = malloc(packet->pos_length * sizeof(packet->pos_y[0]));
          if (data_size >= required_buffer_size + new_length_required && packet->pos_x != NULL && packet->pos_y != NULL) {
            for (unsigned int i = 0; i < packet->pos_length; ++i) {
            packet->pos_x[i] = ntoh_uint(*(unsigned int*) (char_data + used_size));
            used_size += sizeof(packet->pos_x[0]);
            packet->pos_y[i] = ntoh_uint(*(unsigned int*) (char_data + used_size));
            used_size += sizeof(packet->pos_y[0]);
            }
            return packet;
          } else {
            return NULL;
          }
        } else {
          return NULL;
        }
      }
    case game_packet_player_died:
      {
        struct game_packet_player_died* packet;
        size_t required_buffer_size = sizeof(packet->player_id);
        if (data_size >= required_buffer_size) {
          packet = malloc(sizeof(struct game_packet_player_died));

          packet->player_id = ntoh_uint(*(unsigned int*) (char_data + used_size));
          used_size += sizeof(packet->player_id);

          assert(required_buffer_size == used_size);

          return packet;
        } else {
          return NULL;
        }
      }
    default:
      assert(false);
      break;
  }
}

size_t serialize_packet_body(char* buffer, size_t buffer_size, enum game_packets game_packet_type, void* game_packet) {
  size_t used_buffer_size = 0;
  switch (game_packet_type) {
    case game_packet_join_server:
      {
        size_t required_buffer_size = PLAYER_NAME_SIZE;
        if (buffer_size >= required_buffer_size) {
          struct game_packet_join_server* packet = game_packet;

          memcpy_offset_dest(buffer, packet->player_name, PLAYER_NAME_SIZE, &used_buffer_size);

          assert(required_buffer_size == used_buffer_size);
        }
        break;
      }
    case game_packet_join_response:
      {
        char network_accepted;
        unsigned int network_id;
        size_t required_buffer_size = sizeof(network_accepted) + sizeof(network_id);
        if (buffer_size >= required_buffer_size) {
          struct game_packet_join_response* packet = game_packet;
          network_accepted = hton_bool(packet->accepted);
          memcpy_offset_dest(buffer, &network_accepted, sizeof(network_accepted), &used_buffer_size);

          network_id = hton_uint(packet->player_id);
          memcpy_offset_dest(buffer, &network_id, sizeof(network_id), &used_buffer_size);

          assert(required_buffer_size == used_buffer_size);
        }
        break;
      }
    case game_packet_player_join:
      {
        unsigned int network_id;
        size_t required_buffer_size = sizeof(network_id) + PLAYER_NAME_SIZE;
        if (buffer_size >= required_buffer_size) {
          struct game_packet_player_join* packet = game_packet;

          network_id = hton_uint(packet->player_id);
          memcpy_offset_dest(buffer, &network_id, sizeof(network_id), &used_buffer_size);

          memcpy_offset_dest(buffer, packet->player_name, PLAYER_NAME_SIZE, &used_buffer_size);

          assert(required_buffer_size == used_buffer_size);
        }

        break;
      }
    case game_packet_movement:
      {
        struct game_packet_movement* packet = game_packet;
        assert(sizeof(packet->directions[0]) == sizeof(int));

        size_t required_buffer_size = sizeof(packet->directions)
            + sizeof(packet->player_id)
            + packet->pos_length * (sizeof(packet->pos_x[0]) + sizeof(packet->pos_y[0]));
        if (buffer_size >= required_buffer_size) {
          unsigned int network_id = hton_uint(packet->player_id);
          memcpy_offset_dest(buffer, &network_id, sizeof(network_id), &used_buffer_size);

          for (unsigned int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE; ++i) {
            unsigned int network_dir = hton_uint(packet->directions[i]);
            memcpy_offset_dest(buffer, &network_dir, sizeof(packet->directions[0]), &used_buffer_size);
          }

          unsigned int network_pos_length = hton_uint(packet->pos_length);
          memcpy_offset_dest(buffer, &network_pos_length, sizeof(network_pos_length), &used_buffer_size);

          for (unsigned int i = 0; i < packet->pos_length; ++i) {
            unsigned int network_x = hton_uint(packet->pos_x[i]);
            memcpy_offset_dest(buffer, &network_x, sizeof(packet->pos_x[0]), &used_buffer_size);
            unsigned int network_y = hton_uint(packet->pos_y[i]);
            memcpy_offset_dest(buffer, &network_y, sizeof(packet->pos_y[0]), &used_buffer_size);
          }

          assert(required_buffer_size == used_buffer_size);
        }
        
        break;
      }
    case game_packet_player_died:
      {
        unsigned int network_id;
        size_t required_buffer_size = sizeof(network_id) + PLAYER_NAME_SIZE;
        if (buffer_size >= required_buffer_size) {
          struct game_packet_player_died* packet = game_packet;
          network_id = hton_uint(packet->player_id);

          memcpy_offset_dest(buffer, &network_id, sizeof(network_id), &used_buffer_size);

          assert(required_buffer_size == used_buffer_size);
        }
        break;
      }
    default:
      assert(false);
      break;
  }
  return used_buffer_size;
}

/* 
 * packet - an unserialized game packet to be sent of type packet_type
 * index - number in net_state addresses to send to (or -1 for all)
 */
void send_game_packet(const struct network_state* net_state, void* packet, enum game_packets packet_type) {
  const size_t packet_max_size = 300;
  char packet_buffer[packet_max_size];
  size_t size = serialize_packet_header(packet_buffer, packet_max_size, packet_type);
  size += serialize_packet_body(packet_buffer + size, packet_max_size - size, packet_type, packet);

  if (likely(size < packet_max_size)) {
    for (unsigned int i = 0; i < net_state->addresses_size; ++i) {
      sendto(net_state->socket_fd, packet_buffer, size, 0, find_in_lazy_list(net_state->addresses, i), * (socklen_t*) find_in_lazy_list(net_state->address_lengths, i));
    }
  }
}

void send_direct_game_packet(const struct network_state* net_state, void* packet, enum game_packets packet_type,
    const struct sockaddr* address, const socklen_t address_length) {
  const size_t packet_max_size = 300;
  char packet_buffer[packet_max_size];
  size_t size = serialize_packet_header(packet_buffer, packet_max_size, packet_type);
  size = size + serialize_packet_body(packet_buffer + size, packet_max_size - size, packet_type, packet);

  if (likely(size < packet_max_size)) {
    sendto(net_state->socket_fd, packet_buffer, size, 0, address, address_length);
  }
}

/*
 * returns 0 on success
 */
int setup_connection(bool server, int* connection_fd, const char* name, const char* service) {
  int status = 1;

  assert(name != NULL || service != NULL);
  struct addrinfo hints;
  /* TODO allow ipv6 */
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  struct addrinfo* address_results;
  if (getaddrinfo(name, service, &hints, &address_results) == 0) {
    for (struct addrinfo* result = address_results; result != NULL; result = result->ai_next) {
      *connection_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
      if (*connection_fd != -1) {
        if (server) {
          if (bind(*connection_fd, result->ai_addr, result->ai_addrlen) == 0) {
            status = 0;
            break;
          }
        } else {
          if (connect(*connection_fd, result->ai_addr, result->ai_addrlen) == 0) {
            status = 0;
            break;
          }
        }
      }
    }
  }
  freeaddrinfo(address_results);
  return status;
}

void server_socket_setup(void) {
  int socket_fd;

  char port_str[8];
  sprintf(port_str, "%d", GAME_PORT);

  if (setup_connection(true, &socket_fd, NULL, port_str) == 0) {
    printf("Successfully bound to port %d\n", GAME_PORT);
  }
}

void client_socket_setup(const char* remote_ip) {
  int socket_fd;

  char port_str[8];
  sprintf(port_str, "%d", GAME_PORT);

  if (setup_connection(false, &socket_fd, remote_ip, port_str) == 0) {
    printf("Successfully setup connection to server\n");
  }
}

void update_game_ticks(struct game_state* game_state, struct network_state* net_state) {
  /* Handle game packets */
  while (true) {
    if (is_fd_ready(net_state->socket_fd)) {
      char buffer[600];
      struct sockaddr recv_address;
      socklen_t recv_address_size;
      ssize_t size = recvfrom(net_state->socket_fd, &buffer, sizeof(buffer), 0, &recv_address, &recv_address_size);
      if (size != -1) {
        enum game_packets packet_type;
        void* uncast_packet = deserialize_packet(buffer, size, &packet_type);
        if (uncast_packet != NULL) {
          switch (packet_type) {
            case game_packet_join_server:
              if (net_state->is_server) {
                struct game_packet_join_server* packet = uncast_packet;
                struct game_packet_join_response out_packet;
                if (game_state->player_count <= MAX_PLAYER_COUNT && strnlen(packet->player_name, PLAYER_NAME_SIZE) < PLAYER_NAME_SIZE) {
                  out_packet.accepted = true;
                  ++(game_state->player_count);

                  unsigned int player_id = game_state->player_count;
                  /* TODO better selection */
                  init_player(game_state->states + game_state->player_count,
                    game_state->player_count,
                    /* TODO not hardcode location */
                    find_game_tile(70, 10, game_state->board_width, game_state->board_height, game_state->board));

                  out_packet.player_id = player_id;

                  struct game_packet_player_join out_packet_two;
                  out_packet_two.player_id = player_id;
                  memcpy(out_packet_two.player_name, packet->player_name, PLAYER_NAME_SIZE);
                  send_game_packet(net_state, &out_packet_two, game_packet_player_join);

                  add_address_to_state(net_state, &recv_address, recv_address_size);
                } else {
                  out_packet.accepted = false;
                  /* Arbitrary number (not part of protocol) */
                  out_packet.player_id = UINT_MAX;
                }
                send_direct_game_packet(net_state, &out_packet, game_packet_join_response, &recv_address, recv_address_size);
              }
              break;
            case game_packet_join_response:
              if (!net_state->is_server) {
                /* recast packet for ease of use */
                struct game_packet_join_response* packet = uncast_packet;
                if (packet->accepted) {
                  add_address_to_state(net_state, &recv_address, recv_address_size);
                  game_state->states[0].id = packet->player_id;
                } else {
                  close(net_state->socket_fd);
                  net_state->socket_fd = -1;
                }
              }
              break;
            case game_packet_movement:
              {
                struct game_packet_movement* packet = uncast_packet;
                struct player_state* player = find_player_by_id(game_state->states, game_state->player_count, packet->player_id);
                if (player != NULL && packet->pos_length > 0) {
                  /* Check if player head doesn't match and resend data (otherwise assume they have the same data) */
                  if (net_state->is_server && player->head != find_const_game_tile(packet->pos_x[0], packet->pos_y[0],
                      game_state->board_width, game_state->board_height, game_state->board)) {
                    /* Assume that they were lagging and reset their queued movements */
                    for (unsigned int i = 0; i < PLAYER_QUEUED_MOVEMENTS_SIZE; ++i) {
                      player->queued_movements[i] = direction_none;
                    }
                    struct game_packet_movement new_packet;
                    init_game_packet_movement(game_state, player, &new_packet);
                    send_game_packet(net_state, uncast_packet, game_packet_movement);
                  } else {
                    if (net_state->is_server) {
                      send_game_packet(net_state, uncast_packet, game_packet_movement);
                    }
                    /* TODO Check if invalid enum values were received */
                    /* ^^ Maybe that should be done in the deserialization process */
                    memcpy(player->queued_movements, packet->directions, sizeof(enum direction) * PLAYER_QUEUED_MOVEMENTS_SIZE);
                    if (!net_state->is_server) {
                      for (unsigned int i = 0; i < packet->pos_length; ++i) {
                        const struct board_tile* tile = find_const_game_tile(packet->pos_x[i], packet->pos_y[i], game_state->board_width, game_state->board_height, game_state->board);
                        if (tile->type != board_tile_type_player || tile->data.p.player->id != packet->player_id) {
                          /* TODO */
                          printf("Misaligned positions. FIXME ;_;\n");
                          exit(1);
                        }
                      }
                    }
                  }
                }
              }
              break;
            case game_packet_checksum:
              /* TODO */
              break;
            /* Only sent by server */
            case game_packet_player_died:
              if (!net_state->is_server) {
                /* recast packet for ease of use */
                struct game_packet_player_died* packet = uncast_packet;
                player_remove(game_state, net_state, find_player_by_id(game_state->states, game_state->player_count, packet->player_id));
              }
              break;
            /* Sent by both the server and client */
            case game_packet_heartbeat:
            /* Unix time is dangerous, utilise Temps Atomique Internationale */
              clock_gettime(CLOCK_TAI, &(net_state->last_message_timestamp));
              break;
            /* Only sent by server after a player has been added */
            case game_packet_player_join:
            if (!net_state->is_server) {
              struct game_packet_player_join* packet = uncast_packet;
              /* TODO disconnect from server if misbehaving */
              assert(strnlen(packet->player_name, PLAYER_NAME_SIZE) < PLAYER_NAME_SIZE);
              init_player(game_state->player_count, packet->player_id, find_game_tile(packet->x_position, packet->y_position, game_state->board_width, game_state->board_height, game_state->board));
            }
              break;
            default:
              break;
          }
        }
        printf("Received a packet of size %zd\n", size);
      }
    }
  }
}
