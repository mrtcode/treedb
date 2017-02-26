/* 
 * File:   view.h
 * Author: Martynas
 *
 */

#ifndef VIEW_H
#define    VIEW_H

#ifdef    __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lib/linklist.h"
#include "lib/rb.h"
#include "map.h"
#include "vbranch.h"

typedef struct view {
    uint32_t id;

    map_t *map;

    vbranch_t *vbranch;
    void *vbranch_index;

    time_t time_updated;

} view_t;

typedef struct view_list {
    linked_list_t *list;
} view_list_t;

int view_index_init(map_t *map);

view_t *view_index_get(map_t *map, uint32_t id);

view_t *view_create(map_t *map, uint32_t id);

view_t *view_create_clone(map_t *map, view_t *view, uint32_t id);

int view_destroy(view_t *view);

int view_expand(vbranch_t *vb, int level);

vbranch_t *view_expand_to(view_t *view, branch_t *branch, int with_children);

vbranch_t *view_expand_tox(view_t *view, branch_t *branch);

vbranch_t *view_get_vbranch(view_t *view, branch_t *branch);

        int check_vb(branch_t *b, vbranch_t *view);

#ifdef    __cplusplus
}
#endif

#endif	/* VIEW_H */

