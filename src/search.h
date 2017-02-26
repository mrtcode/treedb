/* 
 * File:   search.h
 * Author: Martynas
 *
 */

#ifndef SEARCH_H
#define	SEARCH_H

#ifdef	__cplusplus
extern "C" {
#endif

    int init1(map_t *map);
    int search_init(map_t *map);
    int search_find(map_t *map, uint64_t from_id, char *tags, char *text, branch_t **branch_list, int *branch_n);
    int search_find2(map_t *map, uint64_t note_id, branch_t **branch_list, int *branch_n);

#ifdef	__cplusplus
}
#endif

#endif	/* SEARCH_H */

