
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "lib/rb.h"
#include "map.h"
#include "vbranch.h"
#include "helpers.h"

uint64_t b_id = 1000; //TODO: add to app struct

void* branch_index_init(map_t *map) {
    map->branch_index = rb_create(compare64, 0, 0);
    return map->branch_index;
}

int branch_index_put(map_t *map, branch_t *branch) {
    item64_t *item = malloc(sizeof(item64_t));
    item->id = branch->id;
    item->value = branch;
    item64_t *old_item = rb_replace(map->branch_index, item);

    if(old_item) {
        free(old_item);
    }

    return 1;
}

branch_t *branch_index_delete(map_t *map, uint64_t id) {
    item64_t item;
    item.id = id;
    item64_t *res_item = rb_delete(map->branch_index, &item);
    if(res_item) {
        return res_item->value;
    }
    return NULL;
}

branch_t *branch_index_get(map_t *map, uint64_t id) {
    item64_t item;
    item.id = id;
    item64_t *res_item = rb_find(map->branch_index, &item);
    if(res_item) {
        return res_item->value;
    }
    return NULL;
}

branch_t *branch_new() {
    branch_t *t = (branch_t*) malloc(sizeof (branch_t));
    memset(t, 0, sizeof (branch_t));

    t->id = b_id;
    b_id++;
    
    t->data = malloc(sizeof(data_t));
    memset(t->data, 0, sizeof(data_t));

    return t;
}

int branch_add(branch_t *current, branch_t *parent, int pos) {

    branch_t **childs;

    current->parent = parent;

    if (!parent->child_n) {
        childs = (branch_t**) malloc(sizeof (branch_t*));
        *childs = current;
    } else {

        if (pos < 0 || pos>parent->child_n)
            pos = parent->child_n;

        childs = (branch_t**) malloc(sizeof (branch_t*) * (parent->child_n + 1));

        unsigned short n = 0;
        branch_t **a = childs;
        branch_t **b = parent->child_list;
        while (n < parent->child_n + 1) {
            if (n != pos)
                *a++ = *b++;
            else
                *a++ = current;
            n++;
        }
    }
    branch_t *t = (branch_t*)parent->child_list;
    parent->child_list = childs;
    parent->child_n++;

    free(t);

    branch_increase(current, current->data->total_branch_n+1);
    return 1;
}

int branch_detach(branch_t *b) {

    branch_t **child_list_new;

    branch_t *pb = b->parent;

    branch_increase(b, -b->data->total_branch_n-1);

    if (pb) {

        child_list_new = (branch_t**) malloc(sizeof (branch_t*) * (pb->child_n - 1));

        unsigned short n = 0;
        branch_t **cln = child_list_new;
        branch_t **cl = pb->child_list;
        while (n < pb->child_n) {
            if (*cl!=b) {
                *cln++ = *cl;
            }
            cl++;
            n++;
        }

        branch_t *t = (branch_t*)pb->child_list;
        pb->child_list = child_list_new;
        pb->child_n--;

        free(t);

        b->parent = 0;

    }
    return 1;
}

int branch_set_text(branch_t *branch, char *text) {
    if (!branch->data)
        return 0;

    char *text_tmp = branch->data->text;
    branch->data->text = malloc(strlen(text) + 1);
    strcpy(branch->data->text, text);

    free(text_tmp);
    return 1;
}

int branch_get_pos(branch_t *b) {
    branch_t *pb = b->parent;

    int n = 0;

    branch_t **cl = pb->child_list;

    while (n < pb->child_n) {

        if (*cl == b)
            return n;

        cl++;
        n++;
    }
    return -1;
}
//WARNING: do not add vbranch to branch more than one time
int branch_attach_vbranch(branch_t *b, vbranch_t *vb) {

    vbranch_t **vbranch_list;

    if (!b->data->vbranch_n) {
        vbranch_list = (vbranch_t**) malloc(sizeof (vbranch_t*));
        *vbranch_list = vb;
    } else {

        vbranch_list = (vbranch_t**) malloc((b->data->vbranch_n + 1) * sizeof (vbranch_t*));

        memcpy(vbranch_list, b->data->vbranch_list, b->data->vbranch_n * sizeof (vbranch_t*));

        vbranch_list[b->data->vbranch_n] = vb;

        free(b->data->vbranch_list);
    }

    b->data->vbranch_list = vbranch_list;
    b->data->vbranch_n++;

    vb->branch = b;

    return 0;
}

// it makes sense to remove all child vbranches when parent vbranch is detached, no need to exist
int branch_detach_vbranch(vbranch_t *vb) {

    int n;
    vbranch_t **vbranch_list_new;

    branch_t *b = vb->branch;

        vbranch_list_new = (vbranch_t**) malloc((b->data->vbranch_n-1) * sizeof (vbranch_t*));

        vbranch_t **vbln = vbranch_list_new;
        vbranch_t **vbl = b->data->vbranch_list;

        n = 0;
        while (n < b->data->vbranch_n) {
            if (*vbl != vb) { //WARNING: possible bug: if there is no vb in the list, vbranch_list_new will be overflowed
                *vbln++ = *vbl;
            }
            vbl++;
            n++;
        }

        free(b->data->vbranch_list);
        b->data->vbranch_list = vbranch_list_new;

    b->data->vbranch_n--;

    vb->branch = 0;

    return 0;
}

int branch_increase(branch_t *b, int n) {
    int parents_affected=0;
    while((b=b->parent)) {
        b->data->total_branch_n+=n;
        parents_affected++;
    }
    return parents_affected;
}

int branch_calculate(branch_t *b) {
    int i;
    branch_increase(b, 1);
    
    if (b->child_list) {
        for (i = 0; i < b->child_n; i++) {
            branch_calculate(*(b->child_list + i));
        }
    }
    return 0;
}

int branch_print(branch_t *b, int level) {
    int i;
    for (i = 0; i < level; i++) {
        printf(" ");
    }
    printf("%" PRIu64 " (%d): %s\n", b->id, b->child_n, b->data->text);

    if (b->child_list) {
        level++;

        for (i = 0; i < b->child_n; i++) {
            branch_print(*(b->child_list + i), level);
        }
    }
    return 0;
}

int branch_print_id(branch_t *b, int level) {
    int i;
    for (i = 0; i < level; i++) {
        printf(" ");
    }
    printf("%" PRIu64 " (%d) ", b->id, b->child_n);

    if (b->data) {
        printf("%s", b->data->text);
    }
    printf("\n");

    if (b->child_list) {
        level++;

        for (i = 0; i < b->child_n; i++) {
            branch_print_id(*(b->child_list + i), level);
        }
    }
    return 0;
}
