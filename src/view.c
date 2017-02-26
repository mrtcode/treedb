
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libxml/parser.h>
#include <event.h>
#include <signal.h>
#include <jansson.h>
#include <inttypes.h>

#include "lib/linklist.h"
#include "branch.h"
#include "vbranch.h"
#include "view.h"
#include "helpers.h"
#include "lib/rb.h"


int view_index_init(map_t *map) {
    map->view_index = rb_create(compare32, 0, 0);
}

int view_index_put(map_t *map, view_t *view) {
    item32_t *item = malloc(sizeof(item32_t));
    item->id = view->id;
    item->value = view;
    item32_t *old_item = rb_replace(map->view_index, item);

    if(old_item) {
        free(old_item);
    }

    return 0;
}

view_t *view_index_get(map_t *map, uint32_t id) {
    item32_t item;
    item.id = id;
    item32_t *res_item = rb_find(map->view_index, &item);
    if(res_item) {
        return res_item->value;
    }
    return 0;
}

int view_index_remove(map_t *map, uint32_t id) {
    item32_t item;
    item.id = id;
    rb_delete(map->view_index, &item);
    return 0;
}

view_t *view_create(map_t *map, uint32_t id) {
    view_t *view = (view_t*) calloc(1, sizeof (view_t));

    view->id = id;


    // view_list = view_list_create();
    view_index_put(map, view);

    //list_push_value(view_list->list, view);

    /* view_t *view_test = view_index_get(view_list, view->id);

     if (!view_test) {
         printf("view_test error\n");
     }*/

    vbranch_index_init(view);

    return view;
}

//cpvb - clone parent vbranch
vbranch_t *view_vbranch_clone(vbranch_t *vb, vbranch_t *cpvb, view_t *new_view) {

    vbranch_t *new_vb = vbranch_new();

    new_vb->branch = vb->branch;
    new_vb->parent = cpvb;

    new_vb->view = new_view;

    vbranch_index_put(new_vb->view, new_vb);

    branch_attach_vbranch(new_vb->branch, new_vb);

    new_vb->visible = vb->visible;

    int i, n=0;
    for (i = 0; i < vb->child_n; i++) {
        vbranch_t *vb_i = vb->child_list[i];
        if(vb_i /*&& vb_i->visible*/) {
            n++;
        }
    }

    if(n) {
        new_vb->child_n = n;
        new_vb->child_list = (vbranch_t**)malloc(n*sizeof(vbranch_t*));

        int m = 0;
        for (i = 0; i < vb->child_n && m<n; i++) {
            vbranch_t *vb_i = vb->child_list[i];
            if(vb_i /*&& vb_i->visible*/) {
                new_vb->child_list[m++] = view_vbranch_clone(vb_i, new_vb, new_view);
            }
        }

    }

    return new_vb;
}

view_t *view_create_clone(map_t *map, view_t *view, uint32_t id) {
    view_t *new_view = view_create(map, id);
    new_view->map = map;
    new_view->vbranch = view_vbranch_clone(view->vbranch, 0, new_view);
    return new_view;
}

int view_destroy_vbranch(vbranch_t *vb) {
    int i;
    for (i = 0; i < vb->child_n; i++) {
        view_destroy_vbranch(vb->child_list[i]);
    }

    branch_detach_vbranch(vb);

    if(vb->child_list) {
        free(vb->child_list);
    }
    free(vb);
    return 0;
}

int view_destroy(view_t *view) {
    view_destroy_vbranch(view->vbranch);
    //rbt_destroy(view->vbranch_index);
    view_index_remove(view->map, view->id);
    free(view);
}

int expand(vbranch_t *vb) {
    branch_t *b = vb->branch;

    int i;
    if (b->child_list) {
        vbranch_t **child_list_old = vb->child_list;
        int child_old_n = vb->child_n;
        vb->child_list = (vbranch_t*) malloc(sizeof (vbranch_t*) * (b->child_n));
        for (i = 0; i < b->child_n; i++) {
            branch_t *cur_b = b->child_list[i];

            vbranch_t *existing_vb = 0;
            int j;
            for(j=0;j<child_old_n;j++) {
                vbranch_t *vb = child_list_old[j];
                if(vb->branch==cur_b) {
                    existing_vb = vb;
                    break;
                }
            }

            if(existing_vb) {
                *(vb->child_list + i) = existing_vb;
            } else {
                vbranch_t *new_vb = vbranch_new();

                new_vb->branch = cur_b;
                new_vb->parent = vb;


                new_vb->view = vb->view;

                *(vb->child_list + i) = new_vb;

                vbranch_index_put(new_vb->view, new_vb);

                branch_attach_vbranch(cur_b, new_vb);
            }


        }
        vb->child_n = b->child_n;

        if(child_old_n) {
            free(child_list_old);
        }

        return i;
    }
    return 0;
}

int view_expand(vbranch_t *vb, int level) {


    if (level >= 3) {
        return 0;
    }



    int i;


    if (vb->child_list && vb->child_n == vb->branch->child_n) {
        goto deeper;
    } else {
        int n = expand(vb);
        if (vb->child_list) {
            goto deeper;
        }
    }


deeper:

    level++;


    for (i = 0; i < vb->child_n; i++) {
        view_expand(*(vb->child_list + i), level);
    }

    return 0;
}

int check_vb(branch_t *b, vbranch_t *view) {
    int fn = 0;
    int i;
    for (i = 0; i < b->data->vbranch_n; i++) { //searching for current view vbranch at b
        vbranch_t *vb = b->data->vbranch_list[i];
        if (vb->view == view) {
            //found_vb = vb;
            fn++;
            //break;
        }
    }

    if(fn>1) {
        printf("break\n");
    }
    return 0;
}

/*
 * If expansion was needed, returns top parent that doesn't needed expansion. If no expansion needed returns 0
 */
vbranch_t *view_expand_to(view_t *view, branch_t *branch, int with_children) {
    int i;

    //b - is branch we want to expand to
    branch_t *b = branch;
    branch_t *pb = b->parent; //parent exists?

    vbranch_t *found_vb = 0;

    if(pb) {  //this can happen only in one case - when branch = root branch


        vbranch_t **prev_child_list = 0;
        int prev_child_n = 0;

        vbranch_t *v_end = 0;

        do {

            int fn = 0;
            for (i = 0; i < b->data->vbranch_n; i++) { //searching for current view vbranch at b
                vbranch_t *vb = b->data->vbranch_list[i];
                if (vb->view == view) {
                    found_vb = vb;
                    break;
                }
            }

            if (!found_vb) { //if not found, creating vbranches for b and all it's siblings

                vbranch_t **child_list = malloc(pb->child_n * sizeof(vbranch_t *));
                int child_n = pb->child_n;

                for (i = 0; i < pb->child_n; i++) {
                    branch_t *cur_b = pb->child_list[i];
                    vbranch_t *new_vb = vbranch_new();

                    new_vb->branch = cur_b;
                    new_vb->parent = 0; //no vb parent at the time

                    new_vb->view = view;

                    if (prev_child_list && (cur_b ==
                                            b)) { //if previously vbranches were created in prev_child_list, and current branch is parent of their, all prev_child_list branches now get their parent
                        new_vb->child_list = prev_child_list;
                        new_vb->child_n = prev_child_n;

                        int j;
                        for (j = 0; j < new_vb->child_n; j++) {
                            vbranch_t *vb = new_vb->child_list[j];
                            vb->parent = new_vb;
                        }

                        prev_child_list = 0;
                        prev_child_n = 0;
                    }

                    child_list[i] = new_vb;
                    //child_list[i] = vbranch_new();

                    vbranch_index_put(new_vb->view, new_vb);

                    branch_attach_vbranch(cur_b, new_vb);
                }

                prev_child_list = child_list;
                prev_child_n = child_n;

            } else {
                if (prev_child_list) {
                    found_vb->child_list = prev_child_list;
                    found_vb->child_n = prev_child_n;
                    for (i = 0; i < prev_child_n; i++) {
                        vbranch_t *vb = found_vb->child_list[i];
                        vb->parent = found_vb;
                    }
                } else { //found_vb was found without expansion, return 0
                    //return 0;
                    goto done;
                }

            }

            if (!pb) break;
            b = pb;
            pb = b->parent;


        } while (!found_vb);


        b = branch;

    }
    done:
    if(with_children && b->child_n) {

        vbranch_t *vb=0;

        for (i = 0; i < b->data->vbranch_n; i++) { //searching for current view vbranch at b
            vbranch_t *vb_i = b->data->vbranch_list[i];
            if (vb_i->view == view) {
                vb = vb_i;
                break;
            }
        }

        if(vb) {

            vbranch_t **child_list = malloc(b->child_n * sizeof(vbranch_t *));

            for (i = 0; i < b->child_n; i++) {
                branch_t *b_child = b->child_list[i];
                vbranch_t *vb_new = vbranch_new();

                vb_new->branch = b_child;
                vb_new->parent = vb;

                vb_new->view = view;

                child_list[i] = vb_new;

                vbranch_index_put(vb_new->view, vb_new);

                branch_attach_vbranch(b_child, vb_new);
            }

            vb->child_list = child_list;
            vb->child_n = b->child_n;
        }

    }

    return found_vb;
}



//expand exact collapsed branch without siblings
vbranch_t *view_expand_tox(view_t *view, branch_t *branch) {
    //b - is branch we want to expand to
    branch_t *b = branch;
    branch_t *pb = b->parent; //parent exists?

    int i;


    //b->data exists?

    vbranch_t *found_vb = 0;
    vbranch_t *child_vb = 0;

    do {

        int test_n=0;
        for (i = 0; i < b->data->vbranch_n; i++) { //searching for current view vbranch at b
            vbranch_t *vb = b->data->vbranch_list[i];
            if (vb->view == view) {
                found_vb = vb;
                break;
                test_n++;
                if(test_n>1) {
                    printf("test_n: %d\n", test_n); //test_n = 2
                }
            }
        }




        if (!found_vb) { //if not found, creating vbranches for b and all it's siblings

            vbranch_t *new_vb = vbranch_new();

            new_vb->branch = b;
            new_vb->parent = 0; //no vb parent at the time

            new_vb->view = view;


            if(child_vb) {
                vbranch_add(child_vb, new_vb);
            }

            vbranch_index_put(new_vb->view, new_vb);

            branch_attach_vbranch(b, new_vb);

            child_vb = new_vb;

        } else {
            if (child_vb) {
                vbranch_add(child_vb, found_vb);
            }

        }


        if(!pb) break;
        b = pb;
        pb = b->parent;




    } while (!found_vb);


    if(!found_vb) {
        printf("break\n");
    }

    return found_vb;
}



vbranch_t *view_get_vbranch(view_t *view, branch_t *branch) {
    int i;

    for (i = 0; i < branch->data->vbranch_n; i++) { //searching for current view vbranch at b
        vbranch_t *vb_i = branch->data->vbranch_list[i];
        if (vb_i->view == view) {
            return vb_i;
        }
    }

    return 0;
}










