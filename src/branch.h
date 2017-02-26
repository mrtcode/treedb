/* 
 * File:   branch.h
 * Author: Martynas
 *
 */

#ifndef BRANCH_H
#define	BRANCH_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct map map_t;

typedef struct vbranch vbranch_t;

    typedef struct data {
        char *text;
        uint64_t note_id;
        uint32_t total_branch_n;
        uint32_t total_archive_n;
        vbranch_t **vbranch_list;
        uint32_t vbranch_n;
        uint8_t collapsed;
        uint8_t compacted;
        uint8_t complevel;
    } data_t;

    typedef struct branch {
        uint32_t findex;
        uint64_t id;
        struct branch *parent;
        struct branch **child_list;
        uint16_t child_n;
        data_t *data;
    } branch_t;

    void* branch_index_init(map_t *map);
    int branch_index_put(map_t *map, branch_t *branch);
    branch_t *branch_index_delete(map_t *map, uint64_t id);
    branch_t *branch_index_get(map_t *map, uint64_t id);

    branch_t *branch_new();
    branch_t *branch_init();
    int branch_add(branch_t *current, branch_t *parent, int pos);
    int branch_detach(branch_t *b);

    int branch_set_text(branch_t *branch, char *new_text);
    int branch_get_pos(branch_t *b);

    int branch_attach_vbranch(branch_t *b, vbranch_t *vb);
    int branch_detach_vbranch(vbranch_t *vb);

    int branch_increase(branch_t *b, int n);
    int branch_calculate(branch_t *b);
    int branch_print_id(branch_t *b, int level);

#ifdef	__cplusplus
}
#endif

#endif	/* BRANCH_H */

