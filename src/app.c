#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include "app.h"
#include "map.h"
#include "channel.h"

app_t* app_create(char *path) {
    app_t *app = calloc(1, sizeof(app_t));

    map_index_init(app);
    app->path = strdup(path);

    return app;
}

int app_load_maps(app_t *app) {
    DIR *d;
    struct dirent *entry;
    d = opendir(app->path);
    if (d) {
        while ((entry = readdir(d)) != NULL) {
            if ((entry->d_type & DT_DIR) && strlen(entry->d_name) == 8) {
                char *dbpath = malloc(strlen(app->path) + strlen(entry->d_name) + 2);
                printf("Loading %s\n", entry->d_name);
                map_load(app, entry->d_name);
            }
        }

        closedir(d);
    }
    return 0;
}

int app_start(app_t *app) {
    channel_init(app);
    return 1;
}

//must have '/'
char *app_get_map_path(app_t *app, char *hexid) {
    size_t len = strlen(app->path)+strlen(hexid)+1;
    char *path = calloc(1, len);
    strcpy(path, app->path);

    strcat(path, hexid);

    return path;
}