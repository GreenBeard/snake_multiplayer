#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "./lazy_list.h"

struct lazy_list {
  size_t size;
  size_t item_size;
  char* list;
};

void init_lazy_list(struct lazy_list* list, size_t item_size) {
  list->size = 0;
  list->item_size = item_size;
  list->list = NULL;
}

void* find_in_lazy_list(struct lazy_list* list, size_t index) {
  assert(index < list->size);
  return list + index * list->item_size;
}

void remove_from_lazy_list(struct lazy_list* list, size_t index) {
  assert(index < list->size);
  memmove(list + index * list->item_size, list + (index + 1) * list->item_size, (list->size - 1 - index) * list->item_size);
  --list->size;
  void* tmp = realloc(list->list, list->size * sizeof(list->item_size));
  if (tmp != NULL) {
    list->list = tmp;
  } else {
    /* do nothing */
  }
}

bool add_to_lazy_list(struct lazy_list* list, void* item) {
  assert(list != NULL && item != NULL);
  ++list->size;
  void* tmp = realloc(list->list, list->size * sizeof(list->item_size));
  if (tmp != NULL) {
    list->list = tmp;
    memcpy(list + (list->size - 1) * list->item_size, item, list->item_size);
    return true;
  } else {
    return false;
  }
}
