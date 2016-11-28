typedef struct wsi_entry {
    struct lws* wsi;
    bool connected;
} wsi_entry;

void client_record_add(struct lws* wsi);

void client_record_delete(struct lws* wsi);

void init_client_record();

void free_client_record();