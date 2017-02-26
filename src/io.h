/* 
 * File:   io.h
 * Author: Martynas
 *
 */

#ifndef IO_H
#define	IO_H

#include "branch.h"

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct map map_t;

    typedef struct bdata {
        uint64_t mark_id;
        uint64_t mark_document_id;
        uint8_t mark_document_name[64];
        uint8_t text[1024];
    } __attribute__((packed)) bdata_t;

    typedef struct bhead {
        uint64_t id;
        uint64_t parent_id;
        uint16_t pos;
        uint16_t child_n;
    } __attribute__((packed)) bhead_t;

    int io_resave_map(map_t *map);
    int io_branch_new(map_t *map, branch_t *branch);
    int io_branch_moved(map_t *map, branch_t *branch, branch_t *old_parent, uint16_t old_pos);
    int io_branch_deleted(map_t *map, branch_t *branch, branch_t *old_parent, uint16_t old_pos);
    int io_load_map(map_t *map);
    int io_save_branch_data(map_t *map, branch_t *branch);

#ifdef	__cplusplus
}
#endif

#endif	/* IO_H */

