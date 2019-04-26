#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*enum menu_type {
  menu_type_main,
  menu_type_pause
};*/

enum menu_item_type {
  menu_item_type_checked,
  menu_item_type_unchecked,
  menu_item_type_plain
};

void print_boxed_line(const char* string, unsigned int width, enum menu_item_type item_type) {
  size_t size = strlen(string);
  unsigned int prefix_size;
  switch (item_type) {
    case menu_item_type_checked:
      {
        char str[] = "| [X] ";
        printf("%s", str);
        prefix_size = strlen(str);
        break;
      }
    case menu_item_type_unchecked:
      {
        char str[] = "| [ ] ";
        printf("%s", str);
        prefix_size = strlen(str);
        break;
      }
    case menu_item_type_plain:
      {
        char str[] = "| ";
        printf("%s", str);
        prefix_size = strlen(str);
        break;
      }
    default:
      assert(false);
      break;
  }
  unsigned int max_size = width - prefix_size - 2;
  if (size <= max_size) {
    printf("%s", string);
    for (unsigned int i = size + prefix_size; i < width - 1; ++i) {
      printf(" ");
    }
  } else {
    /* Subtract 3 for the three dots */
    printf("%.*s... ", max_size - 3, string);
  }
  printf("|\n");
}

void print_divider(unsigned int width) {
  printf("+");
  for (unsigned int i = 0; i < width - 2; ++i) {
    printf("-");
  }
  printf("+\n");
}

void print_menu(const char* title, size_t menu_items_size, const char** menu_items, unsigned int selection_index, unsigned int width, unsigned int height) {
  print_divider(width);
  print_boxed_line(title, width, menu_item_type_plain);
  for (size_t i = 0; i < menu_items_size; ++i) {
    char item[80];
    snprintf(item, sizeof(item), "%zu. %s", i, menu_items[i]);
    print_boxed_line(item, width, menu_item_type_plain);
    /*if (i == selection_index) {
      print_boxed_line(menu_items[i], width, menu_item_type_checked);
    } else {
      print_boxed_line(menu_items[i], width, menu_item_type_unchecked);
    }*/
  }
  print_divider(width);
}

/*void test(void) {
  const char* items[] = {"This is an example of a too long line of text to hopefully fit inside of the terminal comfortably", "Option 2", "Option 3"};
  print_menu("Example Title", 3, items, 0, 80, 24);
}*/
