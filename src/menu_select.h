#ifndef MENU_SELECT_H
#define MENU_SELECT_H

#include <stddef.h>

void print_menu(const char* title, size_t menu_items_size, const char** menu_items, unsigned int selection_index, unsigned int width, unsigned int height);

#endif
