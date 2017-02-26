/* 
 * File:   map.h
 * Author: Martynas
 *
 */

#ifndef MAP_H
#define MAP_H

#ifdef    __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sqlite3.h>

#include "branch.h"
#include "lib/queue.h"
#include "app.h"


typedef struct map {
    app_t *app;
    uint32_t id;
    branch_t *branch;
    void *branch_index;
    void *d2b_index;
    void *view_index;

    pthread_t thread_map;

    uint32_t branch_n;

    int test;

    char *bhead_path;
    char *bdata_path;
    char *sqlite_path;

    FILE *bhead_fp;
    FILE *bdata_fp;
    sqlite3 *sqlite;

    struct event *in_event;
    queue_t *in_q;
    queue_t *out_q;
} map_t;


void *map_index_init(app_t *app);

map_t *map_index_get(app_t *app, uint32_t id);

int map_index_traverse(app_t *app, int (*cb)(map_t *, void *), void *data);

map_t *map_load(app_t *app, char *hexid);

map_t *map_create(app_t *app, const char *path, char *map_hexid);

map_t *map_create_clone(app_t *app, char *hexid, char *hexid_from);

map_t *map_create_empty(app_t *app, char *hexid);

map_t *map_create_test(app_t *app, char *map_hexid);

int map_test_start();

#ifdef    __cplusplus
}
#endif

#endif	/* MAP_H */

