#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <jansson.h>

#include <event2/thread.h>
#include "event2/listener.h"

#include "lib/queue.h"
#include "map.h"
#include "helpers.h"
#include "channel.h"

void event_base_add_virtual(struct event_base *);

void onMessage(redisAsyncContext *c, void *reply, channel_t *channel) {

    redisReply *r = reply;
    if (reply == NULL) return;

    if (r->type == REDIS_REPLY_ARRAY && r->elements == 3 && r->element[2]->str) {

        json_t *root;
        json_error_t error;

        root = json_loads(r->element[2]->str, 0, &error);

        if (root) {
            json_t *obj = json_object_get(root, "mapId");
            if (json_is_string(obj)) {
                const char *map_hexid = json_string_value(obj);
                map_t *map = map_index_get(channel->app, hex2id32(map_hexid));

                if (!map) {
                    map = map_create_clone(channel->app, map_hexid, "1cc0d587");
                }

                if (map) {
                    //duplicating str because it will be freed in this event loop
                    queue_push_right(map->in_q, strdup(r->element[2]->str));
                    if (map->in_event)
                        event_active(map->in_event, EV_READ | EV_WRITE, 1);
                }
            }
            json_decref(root);
        } else {
            printf("json error on line %d: %s\n", error.line, error.text);
        }

    }
}

//https://github.com/redis/hiredis/wiki
static void *thread_sub(channel_t *channel) {
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();

    channel->redis_sub = redisAsyncConnect("redis", 6379);

    if (channel->redis_sub->err) {
        printf("error: %s\n", channel->redis_sub->errstr);
        return 0;
    }
    redisLibeventAttach(channel->redis_sub, base);
    redisAsyncCommand(channel->redis_sub, onMessage, channel, "SUBSCRIBE server1");

    event_base_dispatch(base);
    return 0;
}

int map_each(map_t *map, channel_t *channel) {
    const char publish[] = "PUBLISH";
    char *argv[3];
    char channel_id[64];

    char map_hexid[9];
    id2hex32(map_hexid, map->id);
    sprintf(channel_id, "user.%s", map_hexid);

    argv[0] = (char *) publish;
    argv[1] = channel_id;

    char *str;
    while ((str = queue_pop_left(map->out_q))) {
        argv[2] = str;
        redisAsyncCommandArgv(channel->redis_pub, NULL, NULL, 3, (const char **) argv, NULL);
        free(str);
    }

    return 0;
}

static void user_event_cb(evutil_socket_t event, short events, channel_t *channel) {
    map_index_traverse(channel->app, map_each, channel);
}

static void *thread_pub(channel_t *channel) {
    struct event_base *base;
    struct evconnlistener *listener;
    struct event *signal_event, *user_event;
    pthread_t th;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 0;
    }

    evthread_use_pthreads();
    if (evthread_make_base_notifiable(base) < 0) {
        printf("Couldn't make base notifiable!");
        return 0;
    }

    channel->app->out_event = event_new(base, -1, EV_TIMEOUT | EV_READ, user_event_cb, channel);

    channel->redis_pub = redisAsyncConnect("redis", 6379);

    if (channel->redis_pub->err) {
        printf("error: %s\n", channel->redis_pub->errstr);
        return 0;
    }

    redisLibeventAttach(channel->redis_pub, base);

    event_base_add_virtual(base);
    event_base_dispatch(base);

    event_free(channel->app->out_event);
    event_base_free(base);
    return 0;
}

void channel_init(app_t *app) {
    channel_t *channel;
    channel = (channel_t *) calloc(1, sizeof(channel_t));

    channel->app = app;
    app->channel = channel;

    pthread_create(&channel->thread_pub, NULL, thread_sub, channel);
    pthread_create(&channel->thread_sub, NULL, thread_pub, channel);
}