
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "branch.h"
#include "io.h"
#include "map.h"

int io_branch_walk_save(map_t *map, branch_t *b, uint8_t *bhead_buf) {
    int i;
    bhead_t *bhead = ((bhead_t*) bhead_buf) + b->findex;
    bhead->id = b->id;
    bhead->child_n = b->child_n;

    if (b->parent) {
        bhead->parent_id = b->parent->id;
        bhead->pos = branch_get_pos(b);
    } else {
        bhead->parent_id = 0;
        bhead->pos = 0;
    }

    io_save_branch_data(map, b);

    if (b->child_n) {
        for (i = 0; i < b->child_n; i++) {
            io_branch_walk_save(map, *(b->child_list + i), bhead_buf);
        }
    }

    return 0;
}

int io_resave_map(map_t *map) {

    if(map->bhead_fp) {
        fflush(map->bhead_fp);
        fclose(map->bhead_fp);
    }


    uint32_t bhead_buf_size = map->branch_n * sizeof (bhead_t);
    uint8_t *bhead_buf = calloc(1, bhead_buf_size);

    io_branch_walk_save(map, map->branch, bhead_buf);

    FILE *bhead_fp;

    bhead_fp = fopen(map->bhead_path, "wb"); // w for write, b for binary

    fwrite(bhead_buf, bhead_buf_size, 1, bhead_fp);
    fflush(bhead_fp);
    fclose(bhead_fp);

    map->bhead_fp = fopen(map->bhead_path, "rb+");
}

int io_save_branch_head(map_t *map, branch_t *branch) {
    if(map->test) return 0;
    bhead_t bhead;
    uint32_t offset;

    bhead.id = branch->id;
    bhead.child_n = branch->child_n;
    bhead.parent_id = 0;
    bhead.pos = 0;

    if (branch->parent) {
        bhead.parent_id = branch->parent->id;
        bhead.pos = branch_get_pos(branch);
    }

    offset = branch->findex * sizeof (bhead_t);

    fseek(map->bhead_fp, offset, SEEK_SET);
    int ret = fwrite(&bhead, sizeof (bhead_t), 1, map->bhead_fp);

    fflush(map->bhead_fp);

    return 1;
}

int io_save_branch_data(map_t *map, branch_t *branch) {

    char sql[4096];

    char *err_msg = 0;

    int rc;

    sprintf(sql, "INSERT OR REPLACE INTO branch(rowid, note_id, text) VALUES(%" PRIu64 ", %" PRIu64 ", ?);", branch->id, branch->data->note_id);

    sqlite3_stmt *stmt;

    if (sqlite3_prepare(map->sqlite, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("Could not prepare statement.\n");
        return 1;
    }

    if (sqlite3_bind_text(stmt, 1, branch->data->text, strlen(branch->data->text), SQLITE_STATIC) != SQLITE_OK) {
        printf("Could not bind text.\n");
        return 1;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        printf("Could not step (execute) stmt.\n");
        return 1;
    }

    return 1;
}


int branch_load(map_t *map, branch_t *branch) {

    char sql[4096];
    sprintf(sql, "SELECT note_id, text FROM branch WHERE id = %" PRIu64, branch->id);
    sqlite3_stmt *stmt = NULL;

    int rc;

    rc = sqlite3_prepare_v2(map->sqlite, sql, -1, &stmt, NULL);
    if (SQLITE_OK != rc) {
        fprintf(stderr, "Can't prepare select statment %s (%i): %s\n", sql, rc, sqlite3_errmsg(map->sqlite));
        //        sqlite3_close(db);
        return 1;
    }

    if(SQLITE_ROW == (rc = sqlite3_step(stmt))) {

        if (sqlite3_column_count(stmt) == 2) {
            char *note_id = sqlite3_column_text(stmt, 0);
            char *text = sqlite3_column_text(stmt, 1);

            branch->data = (data_t*) malloc(sizeof (data_t));
            memset(branch->data, 0, sizeof(data_t));

            branch->data->text = (uint8_t*) malloc(strlen(text) + 1);
            memcpy(branch->data->text, text, strlen(text)+1);
            //printf(text);

            //printf("id_str: %s\n", id_str);
            branch->data->note_id = strtoull(note_id, NULL, 10);
        }
    }
}

//pos_affected - position from which childs must update their position in
//bhead record, 0 to update all

int io_childs_reordered(map_t *map, branch_t *branch, uint16_t pos_affected) {
    int i;
    for (i = pos_affected; i < branch->child_n; i++) {
        branch_t *b = branch->child_list[i];
        io_save_branch_head(map, b);
    }

}

int io_branch_moved(map_t *map, branch_t *branch, branch_t *old_parent, uint16_t old_pos) {
    io_save_branch_head(map, branch);

    if (branch->parent)
        io_save_branch_head(map, branch->parent);

    io_save_branch_head(map, old_parent);
    uint16_t current_pos = branch_get_pos(branch);
    io_childs_reordered(map, branch->parent, current_pos + 1);
    io_childs_reordered(map, old_parent, old_pos);
}

int io_branch_deleted(map_t *map, branch_t *branch, branch_t *old_parent, uint16_t old_pos) {
    io_save_branch_head(map, branch);
    io_save_branch_head(map, old_parent);
    io_childs_reordered(map, old_parent, old_pos);
    return 0;
}

int io_branch_new(map_t *map, branch_t *branch) {
    uint16_t pos;
    io_save_branch_head(map, branch);
    if (branch->parent) {
        io_save_branch_head(map, branch->parent);
        pos = branch_get_pos(branch);
        io_childs_reordered(map, branch->parent, pos);
    }
    io_save_branch_data(map, branch);
}

int io_load_map(map_t *map) {
    FILE *bhead_fp;
    uint32_t filesize;
    uint32_t branch_n;
    uint8_t *bhead_buf;
    bhead_t *bhead_list;
    uint32_t i;

    bhead_fp = fopen(map->bhead_path, "rb+");

    fseek(bhead_fp, 0L, SEEK_END);
    filesize = ftell(bhead_fp);
    fseek(bhead_fp, 0L, SEEK_SET);

    branch_n = filesize / sizeof (bhead_t);

    //printf("There are %u branches in head file\n", branch_n);

    bhead_buf = malloc(filesize);

    fread(bhead_buf, filesize, 1, bhead_fp);

    bhead_list = (bhead_t*) bhead_buf;

    branch_t **fast_list = (branch_t**) malloc(branch_n * sizeof (branch_t));

    for (i = 0; i < branch_n; i++) {
        /*printf("bhead id: %" PRIu64 ", parent: %" PRIu64 ", pos: %" PRIu64 ", child_n: %" PRIu64 "\n",
                bhead_list[i].id, bhead_list[i].parent_id,
                bhead_list[i].pos, bhead_list[i].child_n);*/

        bhead_t *bhead = bhead_list + i;

        if(!bhead->id) continue; //skip deleted branches

        branch_t *branch = malloc(sizeof (branch_t));
        memset(branch, 0, sizeof (branch_t));
        branch->findex = i;
        branch->id = bhead->id;
        branch->child_n = bhead->child_n;
        branch->child_list = malloc(branch->child_n * sizeof (branch_t*));
        branch_index_put(map, branch);
        fast_list[i] = branch;
    }

    map->branch_n = branch_n;

    map->bhead_fp = bhead_fp;

    //map->bdata_fp = fopen(map->bdata_path, "rb+");

    for (i = 0; i < branch_n; i++) {
        //printf("bhead id: %" PRIu64 "\n", bhead_list[i].id);
        bhead_t *bhead;
        branch_t *branch, *parent_branch;
        uint64_t parent_branch_id;

        bhead = bhead_list + i;

        if(!bhead->id) continue; //skip deleted branches

        branch = fast_list[i];
        parent_branch_id = bhead->parent_id;

        if (parent_branch_id) {

            parent_branch = branch_index_get(map, parent_branch_id);

            if(!parent_branch) {
                printf("Abnormality detected: no parent_branch");
                exit(0);

            } else {

                branch->parent = parent_branch;
                *(branch->parent->child_list + bhead->pos) = branch;
            }

        } else {
            map->branch = branch;
        }

        branch_load(map, branch);
    }


    return 1;
}