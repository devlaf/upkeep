#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "list.h"
#include "wi_client_record.h"

static list* client_record;

static void disable_wsi_entry(void* data, void* args)
{
    wsi_entry* entry = (wsi_entry*)data;
    struct lws* target = (struct lws*)args;
    if (entry->wsi == target)
        entry->connected = false;
}

static void enable_wsi_entry(void* data, void* args)
{
    wsi_entry* entry = (wsi_entry*)data;
    struct lws* target = (struct lws*)args;
    if (entry->wsi == target)
        entry->connected = true;
}

static void free_wsi_entry(void* data, void* args)
{
    wsi_entry* entry = (wsi_entry*)data;
    free(entry);
}

static bool wsi_entry_comparison(void* data, void* expected)
{
    wsi_entry* entry = (wsi_entry*)data;
    struct lws* target = (struct lws*)expected;
    return (entry->wsi == target);
}

void client_record_add(struct lws* wsi)
{
    if (list_contains(client_record, wsi_entry_comparison, (void*)wsi)) {
        list_foreach(client_record, enable_wsi_entry, wsi);
    } else {
        wsi_entry* entry = (wsi_entry*)malloc(sizeof(wsi_entry));
        entry->wsi = wsi;
        entry->connected = true;
        list_append(client_record, (void*)entry);
    }
}

void client_record_delete(struct lws* wsi)
{
    list_foreach(client_record, disable_wsi_entry, wsi);
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
