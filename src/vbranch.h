/* 
 * File:   view.h
 * Author: martynas
 *
 * Created on August 29, 2015, 5:11 PM
 */

#ifndef VBRANCH_H
#define	VBRANCH_H

#ifdef	__cplusplus
extern "C" {
#endif

    //different branches for reader and for users can be made..
    //batch operations batch processing?

#include <stdint.h>

    typedef struct view view_t;

    typedef struct vbranch {
        struct branch *branch;

        char visible;

        struct vbranch *parent;
        struct vbranch **child_list; //pointer to dynamic pointers array that point to first child of child list
        int child_n;

        view_t *view;

    } vbranch_t;

    int vbranch_index_init(view_t *view);
    int vbranch_index_put(view_t *view, vbranch_t *vbranch);
    vbranch_t *vbranch_index_get(view_t *view, uint64_t id);
    vbranch_t *vbranch_new();
    vbranch_t *vbranch_add(vbranch_t *current, vbranch_t *parent);
    vbranch_t *vbranch_detach(vbranch_t *b);
    int vbranch_print(vbranch_t *b, int level);

#ifdef	__cplusplus
}
#endif

#endif	/* VBRANCH_H */

