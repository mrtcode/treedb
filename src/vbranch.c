#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "lib/rb.h"
#include "branch.h"
#include "view.h"
#include "helpers.h"

int vbranch_index_init(view_t *view) {
    view->vbranch_index = rb_create(compare64, 0, 0);
}

int vbranch_index_put(view_t *view, vbranch_t *vbranch) {
    item64_t *item = malloc(sizeof(item64_t));
    item->id = vbranch->branch->id;
    item->value = vbranch;
    item64_t *old_item = rb_replace(view->vbranch_index, item);

    if (old_item) {
        free(old_item);
    }

    return 0;
}

vbranch_t *vbranch_index_get(view_t *view, uint64_t id) {
    item64_t item;
    item.id = id;
    item64_t *res_item = rb_find(view->vbranch_index, &item);
    if (res_item) {
        return res_item->value;
    }
    return 0;
}

int vbranch_index_remove(view_t *view, uint64_t id) {
    item64_t item;
    item.id = id;
    rb_delete(view->vbranch_index, &item);
    return 0;
}

void vbranch_index_destroy_cb(void *data, void *param) {
    free(data);
}

int vbranch_index_destroy(view_t *view) {
    rb_destroy(view->vbranch_index, vbranch_index_destroy_cb);
}


vbranch_t *vbranch_new() {
    vbranch_t *t = (vbranch_t *) calloc(1, sizeof(vbranch_t));
    return t;
}

vbranch_t *vbranch_add(vbranch_t *current, vbranch_t *parent) {

    vbranch_t **childs;

    current->parent = parent;

    if (!parent->child_n) {
        childs = (vbranch_t **) malloc(sizeof(vbranch_t *));
        *childs = current;
    } else {
        int pos = branch_get_pos(current->branch);
        if (pos < 0 || pos > parent->child_n)
            pos = parent->child_n;


        childs = (vbranch_t **) malloc((parent->child_n + 1)*sizeof(vbranch_t *));

        unsigned short n = 0;
        vbranch_t **a = childs;
        vbranch_t **b = parent->child_list;
        while (n < parent->child_n + 1) {
            if (n != pos)
                *a++ = *b++;
            else
                *a++ = current;
            n++;
        }
        free(parent->child_list);
    }
    //vbranch_t **t = parent->child_list;
    parent->child_list = childs;
    parent->child_n++;

    /*memcpy(childs, parent->childs, sizeof (vbranch_t*) * pos);
    memcpy(childs + sizeof (vbranch_t*) * pos, &current, sizeof (vbranch_t*));
    memcpy(childs + sizeof (vbranch_t*) * (pos + 1), parent->childs, sizeof (vbranch_t*) * pos);
     */
}

vbranch_t *vbranch_detach(vbranch_t *b) {

    vbranch_t **child_list_new;

    vbranch_t *pb = b->parent;

    if (pb) {

        child_list_new = (vbranch_t **) malloc(sizeof(vbranch_t *) * (pb->child_n - 1));

        unsigned short n = 0;
        vbranch_t **cln = child_list_new;
        vbranch_t **cl = pb->child_list;
        while (n++ < pb->child_n) {
            if (*cl != b) {
                *cln++ = *cl;
            }
            cl++;
        }

        vbranch_t **t = pb->child_list;
        pb->child_list = child_list_new;
        pb->child_n--;

        free(t);

        b->parent = 0;

    }
}

int vbranch_get_pos(vbranch_t *b) {
    vbranch_t *pb = b->parent;

    int n = 0;

    vbranch_t **cl = pb->child_list;

    while (n < pb->child_n) {

        if (*cl == b)
            return n;

        cl++;
        n++;
    }
    return -1;
}

int vbranch_print(vbranch_t *b, int level) {
    int i;
    for (i = 0; i < level; i++) {
        printf(" ");
    }
    printf("%" PRIu64 " (%d): %s\n", b->branch->id, b->child_n, b->branch->data->text);

    if (b->child_list) {
        level++;

        for (i = 0; i < b->child_n; i++) {
            vbranch_print(*(b->child_list + i), level);
        }
    }
    return 0;
}
