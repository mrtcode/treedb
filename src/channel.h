/* 
 * File:   channel.h
 * Author: Martynas
 */

#ifndef CHANNEL_H
#define    CHANNEL_H


#include <hiredis/async.h>
#include "app.h"

#ifdef    __cplusplus
extern "C" {
#endif

typedef struct channel {
    app_t *app;
    pthread_t thread_pub;
    pthread_t thread_sub;
    redisAsyncContext *redis_pub;
    redisAsyncContext *redis_sub;
} channel_t;

void channel_init(app_t *app);

#ifdef    __cplusplus
}
#endif

#endif	/* CHANNEL_H */

