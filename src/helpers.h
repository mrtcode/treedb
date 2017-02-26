/* 
 * File:   helpers.h
 * Author: Martynas
 *
 */

#ifndef HELPERS_H
#define	HELPERS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <jansson.h>

uint64_t hex2id64(const char *hex);
char* id2hex64(char *hex, uint64_t id);

uint32_t hex2id32(const char *hex);
char* id2hex32(char *hex, uint32_t id);

static inline int libhl_cmp_keys_uint64x(void *k1, size_t k1size,
                                         void *k2, size_t k2size)
{
    if(*((uint64_t*) k1) == *((uint64_t*)k2))
        return 0;
    else if(*((uint64_t*) k1) > *((uint64_t*)k2))
        return 1;

    return -1;
}

static inline int libhl_cmp_keys_uint32x(void *k1, size_t k1size,
                                         void *k2, size_t k2size)
{
    if(*((uint32_t*) k1) == *((uint32_t*)k2))
        return 0;
    else if(*((uint32_t*) k1) > *((uint32_t*)k2))
        return 1;

    return -1;
}

typedef struct item32 {
    uint32_t id;
    void *value;
} item32_t;

static inline int compare32 (const void *pa, const void *pb, void *param)
{
    if (((item32_t*)pa)->id < ((item32_t*)pb)->id)
        return -1;
    else if (((item32_t*)pa)->id > ((item32_t*)pb)->id)
        return +1;
    else
        return 0;
}

typedef struct item64 {
    uint64_t id;
    void *value;
} item64_t;

static inline int compare64 (const void *pa, const void *pb, void *param)
{
    if (((item64_t*)pa)->id < ((item64_t*)pb)->id)
        return -1;
    else if (((item64_t*)pa)->id > ((item64_t*)pb)->id)
        return +1;
    else
        return 0;
}

//typedef json_t;

size_t utf8len(char *s);
char *utf8index(char *s, size_t pos);
char *utf8slice(char *utf8str, size_t max);
int json_get_id32(json_t *parent, char *name, uint32_t *id);
int json_get_id64(json_t *parent, char *name, uint64_t *id);
int json_get_id64s(json_t *parent, char *name, uint64_t **ids, uint32_t *ids_n);
int json_get_str(json_t *parent, char *name, char **str);
int json_get_strs(json_t *parent, char *name, char ***strs, uint32_t *strs_n);
int json_get_int16(json_t *parent, char *name, uint16_t *integer);
int copyfile(char *from, char *to);

#ifdef	__cplusplus
}
#endif

#endif	/* HELPERS_H */

