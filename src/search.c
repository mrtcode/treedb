#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sqlite3.h>
#include "map.h"
#include "search.h"
#include "helpers.h"
#include "branch.h"

int init1(map_t *map) {
    char *err_msg = 0;

    int rc = sqlite3_open(map->sqlite_path, &map->sqlite);

    if (rc != SQLITE_OK) {

        printf("Cannot open database (%s): %s\n",
               map->sqlite_path,
               sqlite3_errmsg(map->sqlite));
        sqlite3_close(map->sqlite);

        return 1;
    }

    char *sql = "CREATE TABLE IF NOT EXISTS branch(id INTEGER PRIMARY KEY, note_id INTEGER, text TEXT);";

    rc = sqlite3_exec(map->sqlite, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(map->sqlite);

        return 1;
    }

    char *sql2 = "CREATE VIRTUAL TABLE IF NOT EXISTS branch_fts USING fts4(content=\"branch\", text, tokenize=unicode61 \"tokenchars=#\");";

    rc = sqlite3_exec(map->sqlite, sql2, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(map->sqlite);

        return 1;
    }

    char *sql3 = "CREATE TRIGGER branch_bu BEFORE UPDATE ON branch BEGIN\n"
            "  DELETE FROM branch_fts WHERE docid=old.rowid;\n"
            "END;\n"
            "CREATE TRIGGER branch_bd BEFORE DELETE ON branch BEGIN\n"
            "  DELETE FROM branch_fts WHERE docid=old.rowid;\n"
            "END;\n"
            "\n"
            "CREATE TRIGGER branch_au AFTER UPDATE ON branch BEGIN\n"
            "  INSERT INTO branch_fts(docid, text) VALUES(new.rowid, new.text);\n"
            "END;\n"
            "CREATE TRIGGER branch_ai AFTER INSERT ON branch BEGIN\n"
            "  INSERT INTO branch_fts(docid, text) VALUES(new.rowid, new.text);\n"
            "END;";

    rc = sqlite3_exec(map->sqlite, sql3, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(map->sqlite);

        return 1;
    }
}

int search_init(map_t *map) {
    char *err_msg = 0;

    int rc = sqlite3_open(map->sqlite_path, &map->sqlite);

    if (rc != SQLITE_OK) {

        printf("Cannot open database (%s): %s\n",
                map->sqlite_path,
                sqlite3_errmsg(map->sqlite));
        sqlite3_close(map->sqlite);

        return 1;
    }


    /*
    //char *sql = "CREATE VIRTUAL TABLE IF NOT EXISTS data USING fts4(content=\"\", tags, text, tokenize=unicode61 \"tokenchars=#\");";
    char *sql = "CREATE VIRTUAL TABLE IF NOT EXISTS data USING fts4(tags, text, tokenize=unicode61 \"tokenchars=#\");";

    rc = sqlite3_exec(map->sqlite, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(map->sqlite);

        return 1;
    }
     */

}

/*
int search_insert(map_t *map, uint64_t branch_id, uint64_t note_id, char *text) {

    if(map->test) return 0;

    char sql[4096];

    char *err_msg = 0;

    int rc;

    //sprintf(sql, "INSERT OR REPLACE INTO data(docid, tags, text) VALUES(%" PRIu64 ", ?, ?);", branch_id);
    sprintf(sql, "INSERT OR REPLACE INTO branch(rowid, note_id, text) VALUES(%" PRIu64 ", %" PRIu64 ", ?);", branch_id, note_id);

    //printf(sql);
    sqlite3_stmt *stmt;


    if (sqlite3_prepare(map->sqlite, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("Could not prepare statement.\n");
        return 1;
    }

    if (sqlite3_bind_text(stmt, 1, text, strlen(text), SQLITE_STATIC) != SQLITE_OK) {
        printf("Could not bind text.\n");
        return 1;
    }


    if (sqlite3_step(stmt) != SQLITE_DONE) {
        printf("Could not step (execute) stmt.\n");
        return 1;
    }


    //sqlite3_finalize(stmt);


    /*printf(sql);
    rc = sqlite3_exec(map->sqlite, stmt, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(map->sqlite);

        return 1;
    }*/
/*
    return 0;
}
*/
int search_find(map_t *map, uint64_t from_id, char *tags, char *text, branch_t **branch_list, int *branch_n) {

    if(map->test) return 0;

    int branch_max=*branch_n;
    
    *branch_n=0;
    
    char *match = text;

    uint64_t id;
    branch_t *b, *t;

    const char sql[] = "SELECT docid FROM branch_fts WHERE text MATCH ? LIMIT 9999";
    sqlite3_stmt *stmt = NULL;

    int rc;

    rc = sqlite3_prepare_v2(map->sqlite, sql, -1, &stmt, NULL);
    if (SQLITE_OK != rc) {
        fprintf(stderr, "Can't prepare select statment %s (%i): %s\n", sql, rc, sqlite3_errmsg(map->sqlite));
        //        sqlite3_close(db);
        return 1;
    }

    if (sqlite3_bind_text(stmt, 1, match, strlen(match), SQLITE_STATIC) != SQLITE_OK) {
        printf("Could not bind text.\n");
        return 1;
    }

    while (*branch_n < branch_max && SQLITE_ROW == (rc = sqlite3_step(stmt))) {

        if (sqlite3_column_count(stmt) == 1) {
            char *id_str = (char*)sqlite3_column_text(stmt, 0);


            //printf("id_str: %s\n", id_str);
            id = strtoull(id_str, NULL, 10);
            
            b=branch_index_get(map, id);
            
            if(b) {
                
                t=b;
                 do {
                    if(t->id==from_id) {
                        branch_list[(*branch_n)++]=b;
                        //printf("%" PRIu64 ": %s\n", id, b->data->text);
                        break;
                    }
                } while((t=t->parent));

            }

        }
    }
    if (SQLITE_DONE != rc) {
    //if (SQLITE_OK != rc) {
        fprintf(stderr, "select statement didn't finish with DONE (%i): %s\n", rc, sqlite3_errmsg(map->sqlite));
    } else {
        //printf("\nSELECT successfully completed\n");
    }


    sqlite3_finalize(stmt);

    return 0;
}


int search_find2(map_t *map, uint64_t note_id, branch_t **branch_list, int *branch_n) {

    if(map->test) return 0;

    int branch_max=*branch_n;

    *branch_n=0;


    uint64_t id;
    branch_t *b, *t;

    char sql[4096];

    char *err_msg = 0;

    int rc;

    sprintf(sql, "SELECT id FROM branch WHERE note_id =  %" PRIu64 " LIMIT 9999", note_id);

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(map->sqlite, sql, -1, &stmt, NULL);
    if (SQLITE_OK != rc) {
        fprintf(stderr, "Can't prepare select statment %s (%i): %s\n", sql, rc, sqlite3_errmsg(map->sqlite));
        //        sqlite3_close(db);
        return 1;
    }

    while (*branch_n < branch_max && SQLITE_ROW == (rc = sqlite3_step(stmt))) {

        if (sqlite3_column_count(stmt) == 1) {
            char *id_str = (char*)sqlite3_column_text(stmt, 0);


            //printf("id_str: %s\n", id_str);
            id = strtoull(id_str, NULL, 10);

            b=branch_index_get(map, id);

            if(b) {
                branch_list[(*branch_n)++] = b;
            }

        }

    }
    if (SQLITE_DONE != rc) {
        fprintf(stderr, "select statement didn't finish with DONE (%i): %s\n", rc, sqlite3_errmsg(map->sqlite));
    }

    sqlite3_finalize(stmt);

    return 0;
}
