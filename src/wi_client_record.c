#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "list.h"
#include "wi_client_record.h"

typedef struct foreach_hack_t {
    struct lws* wsi;
    bool found;
} foreach_hack_t;

static list* client_record;

static void update_wsi_entry(void* data, void* args)
{
    wsi_entry* entry = (wsi_entry*)data;
    struct lws* target = (struct lws*)args;
    if (entry->wsi == target);
        entry->connected = false;
}

static void free_wsi_entry(void* data, void* args)
{
    wsi_entry* entry = (wsi_entry*)data;
    free(entry);
}

void client_record_add(struct lws* wsi)
{
    foreach_hack_t* flag = (foreach_hack_t*)malloc(sizeof(foreach_hack_t));

    wsi_entry* entry = (wsi_entry*)malloc(sizeof(wsi_entry));
    entry->wsi = wsi;
    entry->connected = true;

    list_append(client_record, (void*)entry);
}

void client_record_delete(struct lws* wsi)
{
    list_foreach(client_record, update_wsi_entry, wsi);
}

void init_client_record()
{
    client_record = list_init();
}

void free_client_record()
{
    list_foreach(client_record, free_wsi_entry, NULL);
    list_free(client_record);
}
