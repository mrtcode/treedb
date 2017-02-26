/*
 * File:   app.h
 * Author: Martynas
 */

#ifndef MINDDB_APP_H
#define MINDDB_APP_H

typedef struct channel channel_t;

typedef struct app {
    channel_t *channel;
    void *map_index;
    char *path;
    struct event *out_event;
} app_t;

app_t* app_create(char *path);
int app_load_maps(app_t *app);
int app_start(app_t *app);
char *app_get_map_path(app_t *app, char *hexid);

#endif //MINDDB_APP_H
