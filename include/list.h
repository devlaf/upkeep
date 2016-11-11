#include <stdarg.h>

typedef struct element_t {
    void* data;
    struct element_t* next;
} element_t;

typedef struct list {
    element_t* head;
    element_t* tail;
} list;

struct list* list_init();

// Caller responsible for freeing data fields.
void list_free(struct list* collection);

// Caller responsible for allocation of data.
void list_append(struct list* collection, void *data);

void list_foreach(struct list* collection, void (*fptr)(void*, void*), void* args);
