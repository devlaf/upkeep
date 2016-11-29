#include<stdio.h>
#include<stdlib.h>
#include "list.h"
#include "string.h"

struct list* list_init() 
{
    return (struct list*)calloc(1, sizeof(struct list));
}

// Caller responsible for allocation of data.
void list_append(struct list* collection, void *data)
{
	if (NULL == collection)
		return;

    struct element_t* to_append = (struct element_t*)malloc(sizeof(struct element_t));
    to_append->data = data;
    to_append->next = NULL;

    if (NULL == collection->tail) {
    	collection->tail = to_append;
    	collection->head = to_append;
    } else {
        collection->tail->next = to_append;
    	collection->tail = to_append;
    }
}

// Caller responsible for freeing data fields.
void list_free(struct list* collection) {
	element_t* current = collection->head;
	while (current != NULL) {
		element_t* to_free = current;
		current = current->next;
		free(to_free);
		to_free = NULL;
	}

	free(collection);
	collection = NULL;
}

void list_foreach(struct list* collection, void (*fptr)(void*, void*), void* args)
{
	if(NULL == collection)
		return;

    element_t* node = collection->head;
    while (node != NULL)
    {
        (*fptr)(node->data, args);
        node = node->next;
    }
}

bool list_contains(struct list* collection, bool (*fptr)(void*, void*), void* args)
{
    if(NULL == collection)
        return false;

    element_t* node = collection->head;
    while (node != NULL)
    {
        if ((*fptr)(node->data, args))
            return true;
        node = node->next;
    }

    return false;
}