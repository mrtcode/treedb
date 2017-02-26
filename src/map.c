
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libxml/parser.h>
#include <event.h>
#include <jansson.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>

#include <event2/thread.h>
#include "event2/listener.h"

#include "branch.h"
#include "map.h"
#include "view.h"
#include "io.h"
#include "helpers.h"
#include "search.h"

void event_base_add_virtual(struct event_base *);

void *map_index_init(app_t *app) {
    app->map_index = rb_create(compare32, 0, 0);
    return app->map_index;
}

int map_index_put(app_t *app, map_t *map) {
    item32_t *item = malloc(sizeof(item32_t));
    item->id = map->id;
    item->value = map;
    item32_t *old_item = rb_replace(app->map_index, item);

    if (old_item) {
        free(old_item);
    }

    return 0;
}

map_t *map_index_get(app_t *app, uint32_t id) {
    item32_t item;
    item.id = id;
    item32_t *res_item = rb_find(app->map_index, &item);
    if (res_item) {
        return res_item->value;
    }
    return 0;
}

int map_index_traverse(app_t *app, int (*cb)(map_t *, void *), void *data) {
    struct rb_traverser traverser;
    rb_t_init(&traverser, app->map_index);

    item32_t *item;

    while ((item = rb_t_next(&traverser))) {
        (*cb)(item->value, data);
    }
    return 0;
}

view_t *map_create_view(map_t *map, uint32_t view_id) {
    view_t *view = view_create(map, view_id);
    view->map = map;
    view->vbranch = vbranch_new();
    view->vbranch->branch = map->branch;
    view->vbranch->view = view;

    vbranch_index_put(view, view->vbranch);

    branch_attach_vbranch(map->branch, view->vbranch);

    return view;
}

json_t *vbranch_to_json(vbranch_t *b) {

    json_t *obj = json_object();

    char branch_hexid[17];
    id2hex64(branch_hexid, b->branch->id);

    json_object_set_new(obj, "id", json_string(branch_hexid));
    json_object_set_new(obj, "text", json_string(b->branch->data->text));
    json_object_set_new(obj, "branch_n", json_integer(b->branch->data->total_branch_n));
    json_object_set_new(obj, "child_n", json_integer(b->branch->child_n));
    json_object_set_new(obj, "visible", json_integer(b->visible));

    if (b->branch->data->note_id) {
        char mark_hexid[17], mark_document_hexid[17];
        id2hex64(mark_document_hexid, b->branch->data->note_id);

        json_t *js_mark = json_object();
        json_object_set_new(js_mark, "document_id", json_string(mark_document_hexid));
        json_object_set_new(obj, "mark", js_mark);
    }


    int i;

    if (b->child_n) {
        json_t *array = json_array();

        for (i = 0; i < b->child_n; i++) {
            json_t *res = vbranch_to_json(b->child_list[i]);
            json_array_append_new(array, res);
        }
        json_object_set_new(obj, "childs", array);
    }

    return obj;
}

branch_t *insert(map_t *map, view_t *view,
                 uint64_t parent_branch_id, uint64_t branch_id,
                 uint32_t branch_pos, char *branch_text,
                 uint64_t branch_mark_id, uint64_t branch_document_id) {


    if (view) view->time_updated = time(0);

    branch_t *nb = branch_index_get(map, branch_id);
    if (nb) return 0; //stop if branch exists

    branch_t *pb = branch_index_get(map, parent_branch_id);
    if (!pb) return 0; //stop if parent doesn't exists

    int i;
    if (view) {
        //expansion is only required for initiator view. It's made to expand only one level (when inserting in folded, notloaded branch). But in exceptional cases
        vbranch_t *found_pvb = 0;

        //tries to find current view in parent branch
        for (i = 0; i < pb->data->vbranch_n; i++) { //TODO: seg fault, nb=nil, pb=nil
            vbranch_t *pvb = ((vbranch_t *) (pb->data->vbranch_list)[i]);
            if (pvb->view == view) {
                found_pvb = pvb;
                break;
            }
        }

        if (!found_pvb/* || found_pvb->branch->child_n && !found_pvb->child_n*/) {

            vbranch_t *expanded_vb = view_expand_to(view, pb, 1); //need to expand new parent children also

            if (expanded_vb) {
                json_t *root = json_object();
                json_object_set_new(root, "name", json_string("mapInsert"));
                char view_hexid[9];
                id2hex32(view_hexid, expanded_vb->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                json_t *data = json_object();
                char parent_branch_hexid[17];
                id2hex64(parent_branch_hexid, expanded_vb->branch->id);
                json_object_set_new(data, "parentBranchId", json_string(parent_branch_hexid));
                json_object_set_new(data, "branchPos", json_integer(branch_pos));
                json_t *branch_json = vbranch_to_json(expanded_vb);
                json_object_set_new(data, "branch", branch_json);
                json_object_set_new(root, "data", data);
                char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                //printf(str);
                json_decref(root);
                queue_push_right(expanded_vb->view->map->out_q, str);
            }
        }
    }

    branch_t *new_b = (branch_t *) calloc(1, sizeof(branch_t));

    new_b->id = branch_id;
    new_b->findex = map->branch_n++;
    new_b->data = (data_t *) calloc(1, sizeof(data_t));

    new_b->data->text = utf8slice(branch_text, 5000);

    if (branch_document_id && branch_mark_id) {
        new_b->data->note_id = branch_document_id;
    }

    branch_add(new_b, pb, branch_pos);
    branch_index_put(map, new_b);
    io_branch_new(map, new_b);
    //search_insert(map, new_b->id, 0, new_b->data->text);

    //Spread info about the new branch
    //INSERT - if parent is visible
    //COUNT - if parent isn't visible

    for (i = 0; i < pb->data->vbranch_n; i++) {
        //printf("creating vbranch: %d\n", i);
        vbranch_t *pvb = ((vbranch_t *) (pb->data->vbranch_list)[i]);

        if (pvb->view != view &&
            !pvb->child_n) { //current view will be expanded so no endpoint;for all others endpoint meens not expanded; empty branches for all views will get new vbranch
            continue;
        }

        vbranch_t *new_vb = vbranch_new();
        new_vb->branch = new_b;
        new_vb->parent = pvb;
        new_vb->view = pvb->view;
        vbranch_index_put(new_vb->view, new_vb);
        vbranch_add(new_vb, pvb);
        branch_attach_vbranch(new_b, new_vb);

        if (pvb->view != view) {
            json_t *root = json_object();
            json_object_set_new(root, "name", json_string("mapInsert"));
            char view_hexid[9];
            id2hex32(view_hexid, new_vb->view->id);
            json_object_set_new(root, "viewId", json_string(view_hexid));
            json_t *data = json_object();
            char parent_branch_hexid[17];
            id2hex64(parent_branch_hexid, parent_branch_id);
            json_object_set_new(data, "parentBranchId", json_string(parent_branch_hexid));
            json_object_set_new(data, "branchPos", json_integer(branch_pos));
            json_t *branch_json = vbranch_to_json(new_vb);
            json_object_set_new(data, "branch", branch_json);
            json_object_set_new(root, "data", data);
            char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
            //printf(str);
            json_decref(root);
            queue_push_right(new_vb->view->map->out_q, str);
        }
    }

    //looping root views to get those not expanded to pb
    branch_t *br = map->branch;
    for (i = 0; i < br->data->vbranch_n; i++) {
        //printf("vbr: %d\n", i);
        vbranch_t *vbr = br->data->vbranch_list[i];

        if (vbr->view == view)
            continue; //before it was sending mapCount to a view that inserted the branch. Not sure if this hasn't broke something..

        int j;
        int found_vbr = 0;
        int notexpanded = 0;
        for (j = 0; j < pb->data->vbranch_n; j++) {
            vbranch_t *vb = pb->data->vbranch_list[j];
            if (vbr->view == vb->view) {

                if (vb->branch->child_n && !vb->child_n)
                    notexpanded = 1;

                found_vbr = 1; //todo: ??? it must be at root
                break;
            }
        }

        //if current root vbranch is not expanded till the required branch, find its endpoint
        if (!found_vbr || notexpanded) {
            int found_ep = 0;
            branch_t *b_ep;

            if (notexpanded)
                b_ep = pb;
            else
                b_ep = pb->parent;

            //starting from parent and climbing up while
            //looping through all views and expecting to find current root vbranch (vbr)
            do {
                for (j = 0; j < b_ep->data->vbranch_n; j++) {
                    //printf("creating vbranch: %d\n", j);
                    vbranch_t *vb = ((vbranch_t *) (b_ep->data->vbranch_list)[j]);
                    if (vbr->view == vb->view) {
                        found_ep = 1;
                        break;
                    }
                }
                if (found_ep) {
                    break;
                }
            } while ((b_ep = b_ep->parent));

            //and inform client about count change
            if (found_ep) {
                json_t *root = json_object();
                json_object_set_new(root, "name", json_string("mapCount"));
                char view_hexid[9];
                id2hex32(view_hexid, vbr->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                json_t *data = json_object();
                char branch_hexid[17];
                id2hex64(branch_hexid, b_ep->id);
                json_object_set_new(data, "branchId", json_string(branch_hexid));
                json_object_set_new(data, "n", json_integer(1));
                json_object_set_new(root, "data", data);
                char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                //printf(str);
                json_decref(root);
                queue_push_right(vbr->view->map->out_q, str);
            }
        }
    }

    return new_b;
}

uint64_t *generate_branch_id(map_t *map) {
    //gets last generated id from map->last_generate_branch_id
    //increments it
    //check index if it realy doesn't exists ( in case there was failure saving generated id before)
    //if err, repeats from beginning, if ok saves and returns new id
}

branch_t *insert_path(map_t *map, uint64_t parent_id, char **names, uint32_t names_n) {
    branch_t *parent = 0;
    if (parent_id) {
        parent = branch_index_get(map, parent_id);
    } else {
        parent = map->branch;
    }

    if (!parent)
        return 0;

    int i;
    for (i = 0; i < names_n && parent; i++) {
        char *name = names[i];
        branch_t *branch = 0;
        int j;
        for (j = 0; j < parent->child_n; j++) {
            branch_t *child = parent->child_list[j];
            if (!strcasecmp(name, child->data->text)) {
                branch = child;
                break;
            }
        }
        if (!branch) {
            char hexid[17];
            sprintf(hexid, "%08X%08X", 1/*time(0)*/, map->branch_n);
            uint64_t id = hex2id64(hexid);
            branch = insert(map, 0, parent->id, id, 999999, name, 0, 0);
        }

        parent = branch;
    }

    return parent;
}

branch_t *insert_in_path(map_t *map, uint64_t parent_id,
                         char **names, uint32_t names_n,
                         uint64_t branch_id, char *branch_text,
                         uint64_t branch_mark_id, uint64_t branch_document_id) {
    branch_t *parent = insert_path(map, parent_id, names, names_n);

    if (parent) {
        return insert(map, 0, parent->id, branch_id, 999999,
                      branch_text, branch_mark_id, branch_document_id);
    }
    return 0;
}

int update(map_t *map, view_t *view,
           uint64_t branch_id, char *branch_text,
           uint64_t branch_mark_id, uint64_t branch_document_id) {

    //if(!branch_text && !(branch_mark_id && branch_document_id)) return 0;

    if (view) view->time_updated = time(0);

    branch_t *b = branch_index_get(map, branch_id);

    if (!b) {
        return 0; //maybe send back to client that branch was not found
    }

    if (branch_text) {
        free(b->data->text);

        //TODO: \n \r are not allowed, must be replaced to ' '
        b->data->text = utf8slice(branch_text, 5000);

    }

    b->data->note_id = branch_document_id;

    int i;
    for (i = 0; i < b->data->vbranch_n; i++) {
        vbranch_t *vb = ((vbranch_t *) (b->data->vbranch_list)[i]);

        if (vb->view != view) {
            json_t *root = json_object();
            json_object_set_new(root, "name", json_string("mapUpdate"));

            char view_hexid[9];
            id2hex32(view_hexid, vb->view->id);
            json_object_set_new(root, "viewId", json_string(view_hexid));

            json_t *data = json_object();

            char branch_hexid[17];
            id2hex64(branch_hexid, branch_id);
            json_object_set_new(data, "branchId", json_string(branch_hexid));

            json_object_set_new(data, "branchText", json_string(b->data->text));

            if (b->data->note_id) {
                char str_branch_doc_id[17];
                id2hex64(str_branch_doc_id, b->data->note_id);
                json_object_set_new(data, "document_id", json_string(str_branch_doc_id));
            }

            json_object_set_new(root, "data", data);

            char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);

            json_decref(root);

            queue_push_right(vb->view->map->out_q, str);
        }

    }

    io_save_branch_data(map, b);
    return 1;
}

void vbranch_back_print(vbranch_t *vb) {

    if (!vb) {
        printf("\n");
        return;
    }

    printf("%" PRIu64 " > ", vb->branch->id);

    vbranch_back_print(vb->parent);
}

int move(map_t *map, view_t *view,
         uint64_t new_parent_id, uint64_t old_parent_id,
         uint64_t branch_id, uint16_t branch_pos) {

    if (view) view->time_updated = time(0);

    branch_t *npb = branch_index_get(map, new_parent_id);
    branch_t *b = branch_index_get(map, branch_id);

    if (!b) return 0;

    if (!b->parent) return 0;

    if (!npb) return 0;

    //branch_t *opb = npb->parent;
    branch_t *opb = b->parent;

    //if(!opb) return 0;

    //if(b->parent->id==new_parent_id)
    //    return 0;

    //if new parent branch is not before to branch which will be moved, then operation must be stopped
    int npb_before = 1;

    branch_t *bt = npb;

    do {
        if (bt == b) {
            npb_before = 0;
        }
    } while ((bt = bt->parent));

    if (b->parent->id != old_parent_id || !npb_before) {
        //update client to new position
        //return
        if (view) {
            json_t *root = json_object();
            json_object_set_new(root, "name", json_string("mapMove"));
            char view_hexid[9];
            id2hex32(view_hexid, view->id);
            json_object_set_new(root, "viewId", json_string(view_hexid));
            json_t *data = json_object();
            char new_parent_hexid[17];
            id2hex64(new_parent_hexid, b->parent->id);
            json_object_set_new(data, "newParentId", json_string(new_parent_hexid));
            char branch_hexid[17];
            id2hex64(branch_hexid, b->id);
            json_object_set_new(data, "branchId", json_string(branch_hexid));
            json_object_set_new(data, "branchPos", json_integer(branch_get_pos(b)));
            json_object_set_new(data, "retry", json_integer(1));
            json_object_set_new(root, "data", data);
            char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
            json_decref(root);
            queue_push_right(view->map->out_q, str);
        }
        return 0;
    }

    int i;

    branch_t *br = map->branch;
    for (i = 0; i < br->data->vbranch_n; i++) { //checking all views in the map
        vbranch_t *vbr = br->data->vbranch_list[i];

        int j;

        int src_visible = 0;
        vbranch_t *vb = 0;
        for (j = 0; j < b->data->vbranch_n; j++) {
            vbranch_t *vb_j = ((vbranch_t *) (b->data->vbranch_list)[j]);
            if (vbr->view == vb_j->view) {
                vb = vb_j;
                src_visible = 1;
                break;
            }
        } //vb - source vbranch (the branch which is moved)

        int found_vr = 0;
        int notexpanded = 0;
        vbranch_t *vnpb = 0;
        for (j = 0; j < npb->data->vbranch_n; j++) {
            vbranch_t *vnpb_j = ((vbranch_t *) (npb->data->vbranch_list)[j]);
            if (vbr->view == vnpb_j->view) {
                vnpb = vnpb_j;//!!!! vnpb->folded buvo uzkomentuotas
                if (vnpb->branch->child_n && !vnpb->child_n)
                    notexpanded = 1;

                found_vr = 1;
                break;
            }
        } //vnpb - destination vbranch (new parent branch for vb)

        int dest_visible = 1;
        if (!found_vr || notexpanded) { //destination parent must be expanded to be visible
            dest_visible = 0;
        }

        printf("src_visible: %d, dest_visible: %d\n", src_visible, dest_visible);
        if (src_visible && dest_visible/* || vbr->view == view*/) {

            vbranch_detach(vb); //Signal: SIGABRT (Aborted)
            vbranch_add(vb, vnpb);

            if (vbr->view != view) {
                json_t *root = json_object();
                json_object_set_new(root, "name", json_string("mapMove"));
                char view_hexid[9];
                id2hex32(view_hexid, vbr->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                json_t *data = json_object();
                char new_parent_hexid[17];
                id2hex64(new_parent_hexid, npb->id);
                json_object_set_new(data, "newParentId", json_string(new_parent_hexid));
                char branch_hexid[17];
                id2hex64(branch_hexid, b->id);
                json_object_set_new(data, "branchId", json_string(branch_hexid));
                json_object_set_new(data, "branchPos", json_integer(
                        branch_pos));
                json_object_set_new(root, "data", data);
                char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                //printf(str);
                json_decref(root);
                queue_push_right(vbr->view->map->out_q, str);

            }

        } else if (!src_visible && dest_visible && vbr->view != view) {
            //src parent count - 1
            //dest insert

            //TODO: opb is NOT CHECKED!
            int found_ep = 0;
            branch_t *opb_ep;

            opb_ep = opb;

            //starting from parent and climbing up while
            //looping through all views and expecting to find current root vbranch (vbr) view
            do {
                for (j = 0; j < opb_ep->data->vbranch_n; j++) {
                    vbranch_t *vb_j = ((vbranch_t *) (opb_ep->data->vbranch_list)[j]);
                    if (vbr->view == vb_j->view) {
                        found_ep = 1;
                        break;
                    }
                }
                if (found_ep) {
                    break;
                }
            } while ((opb_ep = opb_ep->parent));

            //and inform client about count change
            if (found_ep) { //this 'if' isn't necessary
                json_t *root = json_object();
                json_object_set_new(root, "name", json_string("mapCount"));
                char view_hexid[9];
                id2hex32(view_hexid, vbr->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                json_t *data = json_object();
                char branch_hexid[17];
                id2hex64(branch_hexid, opb_ep->id);
                json_object_set_new(data, "branchId", json_string(branch_hexid));
                json_object_set_new(data, "n", json_integer(-((json_int_t) b->data->total_branch_n + 1)));
                json_object_set_new(root, "data", data);
                char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                //printf(str);
                json_decref(root);
                queue_push_right(vbr->view->map->out_q, str);
            }

            vbranch_t *new_vb = vbranch_new();
            new_vb->branch = b;
            new_vb->parent = vnpb;
            new_vb->view = vnpb->view;
            vbranch_index_put(new_vb->view, new_vb);
            //vbranch is added before adding real branch, which is done at the end of this code
            vbranch_add(new_vb, vnpb);
            branch_attach_vbranch(b, new_vb);

            json_t *root = json_object();
            json_object_set_new(root, "name", json_string("mapInsert"));
            char view_hexid[9];
            id2hex32(view_hexid, new_vb->view->id);
            json_object_set_new(root, "viewId", json_string(view_hexid));
            json_t *data = json_object();
            char parent_branch_hexid[17];
            id2hex64(parent_branch_hexid, new_parent_id);
            json_object_set_new(data, "parentBranchId", json_string(parent_branch_hexid));
            json_object_set_new(data, "branchPos", json_integer(branch_pos));
            json_t *branch_json = vbranch_to_json(new_vb);
            json_object_set_new(data, "branch", branch_json);
            json_object_set_new(root, "data", data);
            char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
            json_decref(root);
            queue_push_right(new_vb->view->map->out_q, str);

        } else if (src_visible && !dest_visible) {
            //dest expand to
            //move

            vbranch_t *expanded_vb = view_expand_to(vbr->view, npb, 1); //failure. Need to expand new parent childs too

            if (expanded_vb) {
                json_t *root = json_object();
                json_object_set_new(root, "name", json_string("mapInsert"));
                char view_hexid[9];
                id2hex32(view_hexid, expanded_vb->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                json_t *data = json_object();
                char parent_branch_hexid[17];
                id2hex64(parent_branch_hexid, expanded_vb->branch->id);
                json_object_set_new(data, "parentBranchId", json_string(parent_branch_hexid));
                json_object_set_new(data, "branchPos", json_integer(branch_pos));
                json_t *branch_json = vbranch_to_json(expanded_vb);
                json_object_set_new(data, "branch", branch_json);
                json_object_set_new(root, "data", data);
                char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                json_decref(root);
                queue_push_right(expanded_vb->view->map->out_q, str);

                vbranch_t *found_vb2 = 0;
                int k;
                for (k = 0; k < npb->data->vbranch_n; k++) {
                    vbranch_t *vnpb_k = npb->data->vbranch_list[k];
                    if (vbr->view == vnpb_k->view) {
                        found_vb2 = vnpb_k;
                        break;
                    }
                } //vnpb

                if (!found_vb2) {
                    printf("err\n");
                }

                vbranch_detach(vb);
                vbranch_add(vb, found_vb2);

                if (vbr->view != view) {
                    root = json_object();
                    json_object_set_new(root, "name", json_string("mapMove"));
                    //char view_hexid[9];
                    id2hex32(view_hexid, vbr->view->id);
                    json_object_set_new(root, "viewId", json_string(view_hexid));
                    data = json_object();
                    char new_parent_hexid[17];
                    id2hex64(new_parent_hexid, npb->id);
                    json_object_set_new(data, "newParentId", json_string(new_parent_hexid));
                    char branch_hexid[17];
                    id2hex64(branch_hexid, b->id);
                    json_object_set_new(data, "branchId", json_string(branch_hexid));
                    json_object_set_new(data, "branchPos", json_integer(branch_pos));
                    json_object_set_new(root, "data", data);
                    str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                    json_decref(root);
                    queue_push_right(vbr->view->map->out_q, str);
                }
            }
        } else if (!src_visible && !dest_visible && vbr->view != view) {
            //if src and dest are behind different endpoints
            //src count - 1
            //dest count + 1

            branch_t *opb_ep;

            opb_ep = opb;

            vbranch_t *vopb_ep = 0;

            //starting from parent and climbing up while
            //looping through all views and expecting to find current root vbranch (vbr) view
            do {
                for (j = 0; j < opb_ep->data->vbranch_n; j++) {
                    vbranch_t *vopb_j = ((vbranch_t *) (opb_ep->data->vbranch_list)[j]);
                    if (vbr->view == vopb_j->view) {
                        vopb_ep = vopb_j;
                        break;
                    }
                }
                if (vopb_ep) {
                    break;
                }
            } while ((opb_ep = opb_ep->parent));


            branch_t *npb_ep;

            npb_ep = npb;

            vbranch_t *vnpb_ep = 0;

            //starting from parent and climbing up while
            //looping through all views and expecting to find current root vbranch (vbr) view
            do {
                for (j = 0; j < npb_ep->data->vbranch_n; j++) {
                    vbranch_t *vnpb_j = ((vbranch_t *) (npb_ep->data->vbranch_list)[j]);
                    if (vbr->view == vnpb_j->view) {
                        vnpb_ep = vnpb_j;
                        break;
                    }
                }
                if (vnpb_ep) {
                    break;
                }
            } while ((npb_ep = npb_ep->parent));


            if (vopb_ep != vnpb_ep) {

                json_t *root = json_object();
                json_object_set_new(root, "name", json_string("mapCount"));
                char view_hexid[9];
                id2hex32(view_hexid, vbr->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                json_t *data = json_object();
                char branch_hexid[17];
                id2hex64(branch_hexid, vopb_ep->branch->id);
                json_object_set_new(data, "branchId", json_string(branch_hexid));
                json_object_set_new(data, "n", json_integer(-((json_int_t) b->data->total_branch_n + 1)));
                json_object_set_new(root, "data", data);
                char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                //printf(str);
                json_decref(root);
                queue_push_right(vbr->view->map->out_q, str);


                root = json_object();
                json_object_set_new(root, "name", json_string("mapCount"));
                //char view_hexid[9];
                id2hex32(view_hexid, vbr->view->id);
                json_object_set_new(root, "viewId", json_string(view_hexid));
                data = json_object();
                //char branch_hexid[17];
                id2hex64(branch_hexid, vnpb_ep->branch->id);
                json_object_set_new(data, "branchId", json_string(branch_hexid));
                json_object_set_new(data, "n", json_integer(((json_int_t) b->data->total_branch_n + 1)));
                json_object_set_new(root, "data", data);
                str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                //printf(str);
                json_decref(root);
                queue_push_right(vbr->view->map->out_q, str);
            }
        }
    }

    uint16_t old_pos = (uint16_t) branch_get_pos(b);

    branch_detach(b);

    branch_add(b, npb, branch_pos);

    io_branch_moved(map, b, opb, old_pos);

    return 1;
}

int delete(map_t *map, view_t *view, uint64_t branch_id) {
    branch_t *b = branch_index_get(map, branch_id);

    if (!b) return 0;

    if (!b->parent) return 0;

    if (b) {
        if (b->child_n) { //todo: forbid deleting branch with children
            int i;
            for (i = 0; i < b->child_n; i++) {
                branch_t *child = b->child_list[i];
                move(map, 0, map->branch->id, child->parent->id, child->id, 9999);
            }
        }

        /*if (b->data->mark) {
            d2b_list_index_remove(map, b);
        }*/

        int i;

        branch_t *br = map->branch; //Branch Root
        for (i = 0; i < br->data->vbranch_n; i++) { //checking all views in the map

            //printf("vbr: %d\n", i);
            vbranch_t *vbr = br->data->vbranch_list[i];

            int j;

            int src_visible = 0;
            vbranch_t *vb = 0;
            for (j = 0; j < b->data->vbranch_n; j++) {
                vbranch_t *vb_j = ((vbranch_t *) (b->data->vbranch_list)[j]);
                if (vbr->view == vb_j->view) {
                    vb = vb_j;
                    src_visible = 1;
                    break;
                }
            } //vb - source vbranch (the branch which is moved)

            if (src_visible) {
                vbranch_detach(vb);
                if (vb->view != view) {
                    json_t *root = json_object();
                    json_object_set_new(root, "name", json_string("mapDelete"));
                    char view_hexid[9];
                    id2hex32(view_hexid, vb->view->id);
                    json_object_set_new(root, "viewId", json_string(view_hexid));
                    json_t *data = json_object();
                    char branch_hexid[17];
                    id2hex64(branch_hexid, b->id);
                    json_object_set_new(data, "branchId", json_string(branch_hexid));
                    json_object_set_new(root, "data", data);
                    char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                    json_decref(root);
                    queue_push_right(vb->view->map->out_q, str);
                }

            } else if (vbr->view != view) {
                branch_t *pb_ep = b->parent;

                vbranch_t *ep = 0;

                //starting from parent and climbing up while
                //looping through all views and expecting to find current root vbranch (vbr) view
                do {
                    for (j = 0; j < pb_ep->data->vbranch_n; j++) {
                        vbranch_t *vnpb_j = ((vbranch_t *) (pb_ep->data->vbranch_list)[j]);
                        if (vbr->view == vnpb_j->view) {
                            ep = vnpb_j;
                            break;
                        }
                    }
                    if (ep) {
                        break;
                    }
                } while ((pb_ep = pb_ep->parent));

                if (ep) {
                    json_t *root = json_object();
                    json_object_set_new(root, "name", json_string("mapCount"));
                    char view_hexid[9];
                    id2hex32(view_hexid, vbr->view->id);
                    json_object_set_new(root, "viewId", json_string(view_hexid));
                    json_t *data = json_object();
                    char branch_hexid[17];
                    id2hex64(branch_hexid, ep->branch->id);
                    json_object_set_new(data, "branchId", json_string(branch_hexid));
                    json_object_set_new(data, "n", json_integer(-((json_int_t) b->data->total_branch_n + 1)));
                    json_object_set_new(root, "data", data);
                    char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
                    json_decref(root);
                    queue_push_right(vbr->view->map->out_q, str);
                }
            }
        }

        if (b->data->note_id) {
            json_t *root = json_object();
            json_object_set_new(root, "name", json_string("mapRemoveMark"));
            char map_hexid[9];
            id2hex32(map_hexid, map->id);
            json_object_set_new(root, "mapId", json_string(map_hexid));
            json_t *data = json_object();
            char note_hexid[17];
            id2hex64(note_hexid, b->data->note_id);
            json_object_set_new(data, "noteId", json_string(note_hexid));

            char branch_hexid[17];
            id2hex64(branch_hexid, b->id);
            json_object_set_new(data, "branchId", json_string(branch_hexid));


            json_object_set_new(root, "data", data);
            char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
            //printf(str);
            json_decref(root);
            queue_push_right(map->out_q, str);

        }

        branch_t *old_parent_branch = b->parent;
        uint16_t old_pos = (uint8_t)branch_get_pos(b);

        branch_detach(b);

        branch_index_delete(map, b->id);

        b->id = 0;

        io_branch_deleted(map, b, old_parent_branch, old_pos);
    }

    return 0;
}

branch_t *move_to_path(map_t *map, uint64_t parent_id,
                       char **names, uint32_t names_n,
                       uint64_t branch_id) {
    branch_t *parent = insert_path(map, parent_id, names, names_n);

    if (parent) {
        branch_t *b = branch_index_get(map, branch_id);
        if (b && b->parent) {
            move(map, 0, parent->id, b->parent->id, branch_id, 9999);
        }
    }
    return 0;
}

view_t *map_get_updated_view(map_t *map) {
    time_t time_last = 0;
    view_t *last = 0;
    int i;
    for (i = 0; i < map->branch->data->vbranch_n; i++) {
        vbranch_t *vb = map->branch->data->vbranch_list[i];
        if (vb->view->time_updated > time_last) {
            last = vb->view;
            time_last = vb->view->time_updated;
        }
    }
    return last;
}

int vbranch_make_visible_path(vbranch_t *vb) {
    do {
        vb->visible = 1;
    } while ((vb = vb->parent));

    return 0;
}

view_t *map_init_view(map_t *map, uint32_t view_id, uint64_t req_id, uint64_t branch_id,
                      char *query, uint64_t doc_id, uint32_t clone_view_id,
                      uint64_t *endpoint_ids, uint32_t endpoint_ids_n) {

    view_t *view;

    view = view_index_get(map, view_id);
    if (view) {
        view_destroy(view);
        view = 0;
    }

    if (branch_id) {

    }

    if (strlen(query) || doc_id || endpoint_ids_n || branch_id) {

        view = view_create(map, view_id);
        view->map = map;

        view->vbranch = vbranch_new();
        view->vbranch->branch = map->branch;
        view->vbranch->view = view;
        vbranch_index_put(view, view->vbranch);
        branch_attach_vbranch(map->branch, view->vbranch);

        branch_t *b = branch_index_get(map, branch_id);

        if (b) {
            view_expand_to(view, b, 0);
            vbranch_t *vb = view_get_vbranch(view, b);
            vbranch_make_visible_path(vb);
        }

        //view_inconsistency_detector2(map->branch);

        if (strlen(query)) {

            if (strstr(query, "note:current")) {

                /*if (doc_id) {
                    d2b_list_t *d2b_list = d2b_list_index_get(map, doc_id);

                    if (d2b_list) {
                        branch_t **branches = d2b_list->list;
                        size_t branch_n = d2b_list->n;

                        int i;
                        branch_t *b;
                        for (i = 0; i < branch_n; i++) {

                            b = branches[i];

                            view_expand_to(view, b, 0);
                            vbranch_t *vb = view_get_vbranch(view, b);
                            vbranch_make_visible_path(vb);
                        }
                    }
                }*/

                if (doc_id) {
                    view_expand_to(view, map->branch, 0);
                    vbranch_t *vb = view_get_vbranch(view, map->branch);
                    vbranch_make_visible_path(vb);


                    int branch_n = 30;
                    branch_t **branches = malloc(branch_n * sizeof(branch_t *));
                    search_find2(map, doc_id, branches, &branch_n);


                    int i;
                    branch_t *b;
                    for (i = 0; i < branch_n; i++) {

                        b = branches[i];

                        view_expand_to(view, b, 0);
                        vbranch_t *vb = view_get_vbranch(view, b);
                        vbranch_make_visible_path(vb);
                    }
                    free(branches);
                }

            } else {

                branch_t *filter_branch = branch_index_get(map, branch_id);
                if (!filter_branch) {
                    filter_branch = map->branch;
                }

                view_expand_to(view, filter_branch, 0);
                vbranch_t *vb = view_get_vbranch(view, filter_branch);
                vbranch_make_visible_path(vb);


                int branch_n = 30;
                branch_t **branches = malloc(branch_n * sizeof(branch_t *));
                search_find(map, filter_branch->id, "", query, branches, &branch_n);


                int i;
                branch_t *b;
                for (i = 0; i < branch_n; i++) {

                    b = branches[i];

                    view_expand_to(view, b, 0);
                    vbranch_t *vb = view_get_vbranch(view, b);
                    vbranch_make_visible_path(vb);
                }
                free(branches);
            }
        }

        uint32_t i;

        //printf("endpoint n %d\n", endpoint_ids_n);
        for (i = 0; i < endpoint_ids_n; i++) {
            uint64_t id = endpoint_ids[i];
            branch_t *b = branch_index_get(map, id);
            if (b) {
                view_expand_to(view, b, 0);
                vbranch_t *vb = view_get_vbranch(view, b);
                vbranch_make_visible_path(vb);
            }
        }
    }

    if (!view) {
        /*view_t *view_original = 0;

        if (!clone_view_id && map->branch->data->vbranch_n >= 1) {
            view_t *last = map_get_updated_view(map);
            if (last) {
                clone_view_id = last->id;
            }
        }

        if (clone_view_id) {
            view_original = view_index_get(map, clone_view_id);
        }

        if (view_original) {
            view = view_create_clone(map, view_original, view_id);
        } else {*/

        view = view_create(map, view_id);
        view->map = map;

        view->vbranch = vbranch_new();
        view->vbranch->visible = 1;
        view->vbranch->branch = map->branch;
        view->vbranch->view = view;
        vbranch_index_put(view, view->vbranch);
        branch_attach_vbranch(map->branch, view->vbranch);
        view_expand_to(view, map->branch, 1);

        int i;
        for (i = 0; i < view->vbranch->child_n; i++) {
            vbranch_t *child = view->vbranch->child_list[i];
            child->visible = 1;
        }

        //}
    }

    //view_inconsistency_detector2(map->branch);

    json_t *view_json = vbranch_to_json(view->vbranch);

    json_t *root = json_object();
    json_object_set_new(root, "name", json_string("mapInit"));
    char view_hexid[9];
    id2hex32(view_hexid, view->id);
    json_object_set_new(root, "viewId", json_string(view_hexid));
    json_t *data = json_object();
    char req_hexid[17];
    id2hex64(req_hexid, req_id);
    json_object_set_new(data, "reqId", json_string(req_hexid));
    json_object_set_new(data, "map", view_json);
    json_object_set_new(root, "data", data);

    char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);

    json_decref(root);

    queue_push_right(map->out_q, str);

    return view;
}

int map_destroy_view(map_t *map, view_t *view) {
    view_destroy(view);
    return 0;
}


int map_visible(map_t *map, view_t *view,
                uint64_t *branch_ids, uint32_t branch_ids_n) {

    if (view) view->time_updated = time(0);

    uint32_t i;

    for (i = 0; i < branch_ids_n; i++) {
        uint64_t id = branch_ids[i];
        vbranch_t *vb = vbranch_index_get(view, id);
        if (vb) {
            vb->visible = 1;
        }
    }

    return 0;
}

int map_invisible(map_t *map, view_t *view,
                  uint64_t *branch_ids, uint32_t branch_ids_n) {

    if (view) view->time_updated = time(0);

    uint32_t i;

    for (i = 0; i < branch_ids_n; i++) {
        uint64_t id = branch_ids[i];
        vbranch_t *vb = vbranch_index_get(view, id);
        if (vb) {
            vb->visible = 0;
        }
    }

    return 0;
}


int map_expand_to(map_t *map, view_t *view, uint64_t branch_id) {

    if (view) view->time_updated = time(0);

    branch_t *b = branch_index_get(map, branch_id);

    if (!b)
        return 0;

    vbranch_t *vb = view_expand_to(view, b, 1);

    if (vb) { //child_n nes gali buti saka expandinta jau
        json_t *root = json_object();
        json_object_set_new(root, "name", json_string("mapInsert"));
        char view_hexid[9];
        id2hex32(view_hexid, view->id);
        json_object_set_new(root, "viewId", json_string(view_hexid));
        json_t *data = json_object();
        char parent_branch_hexid[17];
        id2hex64(parent_branch_hexid, vb->branch->id);
        json_object_set_new(data, "parentBranchId", json_string(parent_branch_hexid));
        json_object_set_new(data, "branchPos", json_integer(0));
        json_t *branch_json = vbranch_to_json(vb);
        json_object_set_new(data, "branch", branch_json);
        json_object_set_new(root, "data", data);
        char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
        json_decref(root);
        queue_push_right(view->map->out_q, str);
    }
}

int map_branch_unfold(map_t *map, view_t *view, uint64_t branch_id) {

    if (view) view->time_updated = time(0);

    vbranch_t *vb = vbranch_index_get(view, branch_id);

    if (vb && vb->child_n != vb->branch->child_n) { //better check if not loaded
        view_expand(vb, 2);

        json_t *root = json_object();
        json_object_set_new(root, "name", json_string("mapInsert"));
        char view_hexid[9];
        id2hex32(view_hexid, view->id);
        json_object_set_new(root, "viewId", json_string(view_hexid));
        json_t *data = json_object();
        char parent_branch_hexid[17];
        id2hex64(parent_branch_hexid, vb->branch->id);
        json_object_set_new(data, "parentBranchId", json_string(parent_branch_hexid));
        json_object_set_new(data, "branchPos", json_integer(0));
        json_t *branch_json = vbranch_to_json(vb);
        json_object_set_new(data, "branch", branch_json);
        json_object_set_new(root, "data", data);
        char *str = json_dumps(root, JSON_INDENT(1) | JSON_PRESERVE_ORDER);
        json_decref(root);
        queue_push_right(view->map->out_q, str);
    }
}

int map_proc(map_t *map, char *op) {
    json_t *root;
    json_error_t error;

    printf("%s\n", op);

    root = json_loads(op, 0, &error);

    if (!root) {
        return 0;
    }

    uint32_t view_id = 0;
    if (!json_get_id32(root, "viewId", &view_id)) {
        goto error;
    }
    view_t *view = view_index_get(map, view_id);

    char *name = 0;
    if (!json_get_str(root, "name", &name)) {
        goto error;
    }

    json_t *data = json_object_get(root, "data");
    if (!json_is_object(data)) {
        goto error;
    }

    if (!strcmp(name, "mapInitView")) {

        //printf("Initialising view\n");

        char *query;
        uint64_t req_id;
        uint64_t branch_id = 0;
        uint64_t doc_id;
        uint32_t clone_view_id = 0;
        uint32_t endpoint_ids_n;
        uint64_t *endpoint_ids = 0;

        if (!(json_get_str(data, "query", &query) &&
              json_get_id64(data, "reqId", &req_id) &&
              json_get_id64(data, "docId", &doc_id) &&
              json_get_id64s(data, "endpoints", &endpoint_ids, &endpoint_ids_n))) {
            goto error;
        }

        json_get_id64(data, "branchId", &branch_id);
        json_get_id32(data, "cloneViewId", &clone_view_id);
        map_init_view(map, view_id, req_id, branch_id, query, doc_id, clone_view_id, endpoint_ids, endpoint_ids_n);
        free(endpoint_ids);
    } else if (!strcmp(name, "mapInsertInPath")) {
        uint64_t parent_branch_id;
        char **names = 0;
        uint32_t names_n;
        uint64_t branch_id;
        char *branch_text;
        uint64_t new_branch_document_id;
        uint64_t new_branch_mark_id = 1;

        if (!(json_get_id64(data, "parentBranchId", &parent_branch_id) &&
              json_get_strs(data, "names", &names, &names_n) &&
              json_get_id64(data, "branchId", &branch_id) &&
              json_get_str(data, "branchText", &branch_text) &&
              //json_get_id64(data, "branchMarkId", &new_branch_mark_id) &&
              json_get_id64(data, "branchDocumentId", &new_branch_document_id))) {
            goto error;
        }

        insert_in_path(map, parent_branch_id,
                       names, names_n,
                       branch_id, branch_text,
                       new_branch_mark_id, new_branch_document_id);

        free(names);
    } else if (!strcmp(name, "mapMoveToPath")) {
        uint64_t parent_branch_id;
        char **names = 0;
        uint32_t names_n;
        uint64_t branch_id;

        if (!(json_get_id64(data, "parentBranchId", &parent_branch_id) &&
              json_get_strs(data, "names", &names, &names_n) &&
              json_get_id64(data, "branchId", &branch_id))) {
            goto error;
        }

        move_to_path(map, parent_branch_id,
                     names, names_n,
                     branch_id);

        free(names);
    } else if (!strcmp(name, "mapDelete")) {
        uint64_t branch_id;

        if (!(json_get_id64(data, "branchId", &branch_id))) {
            goto error;
        }

        delete(map, view, branch_id);

    } else if (!strcmp(name, "mapInsert")) {
        uint64_t parent_branch_id;
        uint64_t branch_id;
        uint16_t branch_pos;
        char *branch_text;
        uint64_t new_branch_document_id;
        uint64_t new_branch_mark_id = 1;

        if (!(json_get_id64(data, "parentBranchId", &parent_branch_id) &&
              json_get_id64(data, "branchId", &branch_id) &&
              json_get_int16(data, "branchPos", &branch_pos) &&
              json_get_str(data, "branchText", &branch_text) &&
              //json_get_id64(data, "branchMarkId", &new_branch_mark_id) &&
              json_get_id64(data, "branchDocumentId", &new_branch_document_id))) {
            goto error;
        }

        insert(map, view,
               parent_branch_id, branch_id,
               branch_pos, branch_text,
               new_branch_mark_id, new_branch_document_id);

    } else if (!strcmp(name, "mapUpdate")) {
        uint64_t branch_id;
        char *branch_text = 0;
        uint64_t new_branch_document_id = 0;
        uint64_t new_branch_mark_id = 0;

        if (!(json_get_id64(data, "branchId", &branch_id) /*&&
                  json_get_str(data, "branchText", &branch_text)*/)) {
            goto error;
        }

        json_get_str(data, "branchText", &branch_text);
        json_get_id64(data, "branchMarkId", &new_branch_mark_id);
        json_get_id64(data, "branchDocumentId", &new_branch_document_id);

        update(map, view,
               branch_id, branch_text,
               new_branch_mark_id, new_branch_document_id);

    } else if (view) {
        if (!strcmp(name, "mapVisible")) {

            uint32_t branch_ids_n;
            uint64_t *branch_ids = 0;

            if (!(json_get_id64s(data, "branchIds", &branch_ids, &branch_ids_n))) {
                goto error;
            }

            map_visible(map, view, branch_ids, branch_ids_n);
            free(branch_ids);
        } else if (!strcmp(name, "mapInvisible")) {

            uint32_t branch_ids_n;
            uint64_t *branch_ids = 0;

            if (!(json_get_id64s(data, "branchIds", &branch_ids, &branch_ids_n))) {
                goto error;
            }

            map_invisible(map, view, branch_ids, branch_ids_n);
            free(branch_ids);
        } else if (!strcmp(name, "mapDestroyView")) {
            map_destroy_view(map, view);
        } else if (!strcmp(name, "mapBranchUnfold")) {
            uint64_t branch_id;

            if (!json_get_id64(data, "branchId", &branch_id)) {
                goto error;
            }

            map_branch_unfold(map, view, branch_id);
        } else if (!strcmp(name, "mapExpandTo")) {
            uint64_t branch_id;
            char *query;

            if (!(json_get_id64(data, "branchId", &branch_id))) {
                goto error;
            }

            map_expand_to(map, view, branch_id);
        } else if (!strcmp(name, "mapMove")) {
            uint64_t new_parent_id;
            uint64_t old_parent_id;
            uint64_t branch_id;
            uint16_t branch_pos;

            if (!(json_get_id64(data, "newParentId", &new_parent_id) &&
                  json_get_id64(data, "oldParentId", &old_parent_id) &&
                  json_get_id64(data, "branchId", &branch_id) &&
                  json_get_int16(data, "branchPos", &branch_pos))) {
                goto error;
            }

            move(map, view,
                 new_parent_id, old_parent_id,
                 branch_id, branch_pos);
        }
    }

    ok:
    goto endop;

    error:
    printf("Error: %s\n", op);

    endop:
    queue_push_right(map->out_q, strdup("{\"ack\":\"1\"}"));
    event_active(map->app->out_event, EV_READ | EV_WRITE, 1);
    json_decref(root);
    return 0;
}

static void map_event(evutil_socket_t event, short events, void *data) {
    map_t *map = (map_t *) data;

    char *op;

    while ((op = queue_pop_left(map->in_q))) {
        map_proc(map, op);
        free(op);
    }
}

static void *thread_map(map_t *map) {

    struct event_base *base;
    struct event *user_event;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 0;
    }

    evthread_use_pthreads();
    if (evthread_make_base_notifiable(base) < 0) {
        printf("Couldn't make base notifiable!");
        return 0;
    }

    map->in_event = event_new(base, -1, EV_TIMEOUT | EV_READ, map_event, map);

    event_active(map->in_event, EV_READ | EV_WRITE, 1);

    event_base_add_virtual(base);

    event_base_dispatch(base);
    event_free(map->in_event);
    event_base_free(base);
}

//to read FreeMind .mm file
int read_map(map_t *map, xmlNode *child, branch_t *parent) {
    xmlNode *node;
    for (node = child; node; node = node->next) {

        if (!strcmp((char *) node->name, "node")) {
            xmlAttr *attribute = node->properties;
            branch_t *b = branch_new();
            b->findex = map->branch_n++;
            branch_add(b, parent, -1);
            branch_index_put(map, b);
            while (attribute && attribute->name && attribute->children) {
                xmlChar *value = xmlNodeListGetString(node->doc, attribute->children, 1);
                if (!strcmp((char *) attribute->name, "TEXT")) {
                    b->data->text = utf8slice(value, 250);
                } else {
                    b->data->text = strdup("");
                }
                xmlFree(value);
                attribute = attribute->next;
            }
            read_map(map, node->children, b);
        }

    }
    return 0;
}

branch_t *import_mm(map_t *map, char *mm_file) {
    xmlDoc *document;
    xmlNode *root, *first_child, *node;
    char *filename;

    filename = mm_file;

    document = xmlReadFile(filename, NULL, 0);
    root = xmlDocGetRootElement(document);
    fprintf(stdout, "Root is <%s> (%i)\n", root->name, root->type);
    first_child = root->children;

    //branch_t *r = branch_init(); //TODO: FIX
    branch_t *r = 0;

    r->findex = map->branch_n++;

    r->data->text = strdup("Map1");

    branch_index_put(map, r);

    read_map(map, first_child, r);

    return r;
}

int branch_put_search(map_t *map, branch_t *b) {
    int i;
    if (b->child_list) {

        for (i = 0; i < b->child_n; i++) {
            branch_put_search(map, *(b->child_list + i));
        }
    }

    return 0;
}

int map_init_paths(map_t *map, const char *path) {
    const char bhead[] = "bhead";
    const char bdata[] = "bdata";
    const char sqlite[] = "sqlite";

    map->bhead_path = malloc(strlen(path) + strlen(bhead) + 2);
    sprintf(map->bhead_path, "%s/%s", path, bhead);

    map->bdata_path = malloc(strlen(path) + strlen(bdata) + 2);
    sprintf(map->bdata_path, "%s/%s", path, bdata);

    map->sqlite_path = malloc(strlen(path) + strlen(sqlite) + 2);
    sprintf(map->sqlite_path, "%s/%s", path, sqlite);
}

int map_copy(app_t *app, char *hexid_from, char *hexid_to) {

    char *path_from = app_get_map_path(app, hexid_from);

    char *path_to = app_get_map_path(app, hexid_to);

    struct stat st = {0};

    if (stat(path_to, &st) == -1) {
        mkdir(path_to, 0777);
    }

    const char bhead[] = "bhead";
    //const char bdata[] = "bdata";
    const char sqlite[] = "sqlite";

    char *file_from;
    char *file_to;

    //

    file_from = malloc(strlen(path_from) + strlen(bhead) + 2);
    sprintf(file_from, "%s/%s", path_from, bhead);

    file_to = malloc(strlen(path_to) + strlen(bhead) + 2);
    sprintf(file_to, "%s/%s", path_to, bhead);

    copyfile(file_from, file_to);

    free(file_from);
    free(file_to);

    //

    /*
    file_from = malloc(strlen(path_from) + strlen(bdata) + 2);
    sprintf(file_from, "%s/%s", path_from, bdata);

    file_to = malloc(strlen(path_to) + strlen(bdata) + 2);
    sprintf(file_to, "%s/%s", path_to, bdata);

    copyfile(file_from, file_to);

    free(file_from);
    free(file_to);
    */
    //

    file_from = malloc(strlen(path_from) + strlen(sqlite) + 2);
    sprintf(file_from, "%s/%s", path_from, sqlite);

    file_to = malloc(strlen(path_to) + strlen(sqlite) + 2);
    sprintf(file_to, "%s/%s", path_to, sqlite);

    copyfile(file_from, file_to);

    free(file_from);
    free(file_to);
}

map_t *map_load(app_t *app, char *hexid) {

    char *path = app_get_map_path(app, hexid);

    map_t *map = (map_t *) calloc(1, sizeof(map_t));

    map->app = app;

    map->id = hex2id32(hexid);

    map_init_paths(map, path);

    search_init(map);

    branch_index_init(map);

    //d2b_index_init(map);

    io_load_map(map);

    //init1(map);

    //branch_put_search(map, map->branch);

    map->in_q = queue_create();
    map->out_q = queue_create();

    map->in_event = NULL;

    view_index_init(map);

    map_index_put(app, map);

    branch_calculate(map->branch);

    pthread_create(&map->thread_map, NULL, thread_map, map);

    free(path);

    return map;
}

map_t *map_create_clone(app_t *app, char *hexid, char *hexid_from) {
    map_copy(app, hexid_from, hexid);
    return map_load(app, hexid);
}

map_t *map_create_empty(app_t *app, char *hexid) {

    char *path = app_get_map_path(app, hexid);

    struct stat st = {0};

    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
    }

    map_t *map = (map_t *) calloc(1, sizeof(map_t));

    map->app = app;

    map->id = hex2id32(hexid);

    map_init_paths(map, path);

    FILE *fp = fopen(map->bhead_path, "wb");
    fclose(fp);

    map->bhead_fp = fopen(map->bhead_path, "rb+");

    init1(map);

    branch_index_init(map);

    //search_init(map);

    map->in_q = queue_create();
    map->out_q = queue_create();

    map->in_event = NULL;

    view_index_init(map);

    map_index_put(app, map);

    branch_t *branch = (branch_t *) calloc(1, sizeof(branch_t));
    branch->id = 1;
    branch->findex = map->branch_n++;

    branch->data = (data_t *) calloc(1, sizeof(data_t));
    branch->data->text = strdup("My map");

    map->branch = branch;

    branch_index_put(map, branch);

    io_branch_new(map, branch);

    //io_resave_map(map);

    //branch_put_search(map, map->branch);

    branch_calculate(map->branch);

    pthread_create(&map->thread_map, NULL, thread_map, map);

    free(path);

    return map;
}

map_t *map_create_test(app_t *app, char *map_hexid) {
    map_t *map = (map_t *) malloc(sizeof(map_t));
    memset(map, 0, sizeof(map_t));

    map->id = hex2id32(map_hexid);

    map->test = 1;

    branch_index_init(map);

    branch_t *new_b = (branch_t *) malloc(sizeof(branch_t));
    memset(new_b, 0, sizeof(branch_t));

    new_b->findex = map->branch_n++;

    new_b->data = (data_t *) malloc(sizeof(data_t));
    memset(new_b->data, 0, sizeof(data_t));


    new_b->id = 1;
    new_b->data->text = strdup("My map");

    map->branch = new_b;

    branch_index_put(map, new_b);

    //search_init(map);

    map->in_q = queue_create();
    map->out_q = queue_create();

    map->in_event = NULL;

    view_index_init(map);

    map_index_put(app, map);

    //io_resave_map(map);

    //branch_t *b = import_mm(map, "p1.mm");
    //branch_add(b, map->tree, -1);
    //branch_print(map->tree, 0);

    //io_resave_map(map);

    branch_put_search(map, map->branch);

    branch_calculate(map->branch);

    pthread_t th;
    //pthread_create(&th, NULL, map_thread, map);

    return map;
}

int view_inconsistency_detector(branch_t *branch) {
    int i, j, a, b;

    for (a = 0; a < branch->child_n; a++) {
        branch_t *branch1 = branch->child_list[a];

        for (i = 0; i < branch1->data->vbranch_n; i++) {
            vbranch_t *vbranch = branch1->data->vbranch_list[i];


            for (b = 0; b < branch->child_n; b++) {
                branch_t *branch2 = branch->child_list[b];
                int found = 0;
                for (j = 0; j < branch2->data->vbranch_n; j++) {
                    vbranch_t *vbranch2 = branch2->data->vbranch_list[j];

                    if (vbranch->view == vbranch2->view) {
                        found = 1;
                        break;
                    }

                }
                if (!found) {
                    printf("inconsistent\n");
                }
            }

        }

        view_inconsistency_detector(branch1);
    }

    return 0;
}


int view_inconsistency_detector2(branch_t *branch) {
    int i, j, a, b;

    for (a = 0; a < branch->child_n; a++) {
        branch_t *branch1 = branch->child_list[a];

        for (i = 0; i < branch1->data->vbranch_n; i++) {
            vbranch_t *vbranch = branch1->data->vbranch_list[i];

            int found = 0;
            for (j = 0; j < branch1->data->vbranch_n; j++) {
                vbranch_t *vbranch2 = branch1->data->vbranch_list[j];

                if (vbranch->view == vbranch2->view) {
                    found++;
                    //break;
                }

            }
            if (found > 1) {
                printf("inconsistent 1\n");
            }

            if (vbranch->child_n > 100000) {
                printf("inconsistent 2\n");
            }


        }

        view_inconsistency_detector2(branch1);
    }

    return 0;
}

