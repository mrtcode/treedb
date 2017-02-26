/* 
 * File:   main.c
 * Author: Martynas
 *
 */

#define THREAD_SAFE

#include <stdio.h>
#include <stdlib.h>
#include <event.h>
#include <signal.h>
#include <event2/thread.h>
#include "event2/listener.h"
#include "map.h"

void event_base_add_virtual(struct event_base *);


static void signal_cb(evutil_socket_t sig, short events, void *user_data) {
    struct event_base *base = (struct event_base *) user_data;
    struct timeval delay = {1, 0};

    if(sig==SIGINT || sig==SIGTERM) {
        event_base_loopexit(base, NULL);
    }
    //event_base_loopexit(base, &delay);
    //
}

int init_signals() {

    struct event_base *base;
    struct event *signal_event, *user_event;

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

    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *) base);

    if (!signal_event || event_add(signal_event, NULL) < 0) {
        fprintf(stderr, "Could not create/add a signal event!\n");
        return 1;
    }

    signal_event = evsignal_new(base, SIGTERM, signal_cb, (void *) base);

    if (!signal_event || event_add(signal_event, NULL) < 0) {
        fprintf(stderr, "Could not create/add a signal event!\n");
        return 1;
    }


    event_base_add_virtual(base);
    //struct timeval timeout = {2, 0};
    //event_add(ev_user, &timeout);
    //return ev_user;

    //rc = event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);
    event_base_dispatch(base);
    event_free(signal_event);
    event_base_free(base);

    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    app_t *app = app_create("../maps/");
    app_load_maps(app);

    app_start(app);

    init_signals();

    return (EXIT_SUCCESS);
}

