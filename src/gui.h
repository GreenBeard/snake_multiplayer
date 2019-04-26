#ifndef GUI_H
#define GUI_H

#include "./commons.h"

void clear_screen_or_reset_cursor(void);

void print_screen(const struct game_state* state);

void menu_print_text_choice(const char* prompt, size_t buffer_size, char* buffer);

unsigned int menu_print_select_choice(const char* title, size_t menu_items_size, const char** menu_items, unsigned int width, unsigned int height);

bool setup_terminal(void);

bool reset_terminal(void);

#endif
