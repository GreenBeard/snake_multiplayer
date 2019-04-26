#include <stddef.h>
#include <stdbool.h>

/*
 * This list is lazy since it mallocs as conservatively as possible.
 */
struct lazy_list;

void init_lazy_list(struct lazy_list* list, size_t item_size);

/*
 * Returns true on success and false on failure (due to malloc)
 * does not check if the input is NULL
*/
bool add_to_lazy_list(struct lazy_list* list, void* item);

/*
 * Does not validate input boundaries
*/
void remove_from_lazy_list(struct lazy_list* list, size_t index);

/*
 * Returns a pointer to the item in the list
 */
void* find_in_lazy_list(struct lazy_list* list, size_t index);
