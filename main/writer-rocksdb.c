/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to entry.c
*/

#include "general.h"  /* must always come first */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "entry_p.h"
#include "mio.h"
#include "options_p.h"
#include "parse_p.h"
#include "ptag_p.h"
#include "read.h"
#include "writer_p.h"

#include "db/dbop.h"
#include "db/die.h"
// #include "rocksdb/c.h"
// #include <unistd.h>  // sysconf() - get CPU count

static int writeRocksdbEntry (tagWriter *writer CTAGS_ATTR_UNUSED,
    MIO * mio, const tagEntryInfo *const tag);

tagWriter rocksdbWriter = {
	.writeEntry = writeRocksdbEntry,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = "",
};

static DBOP *theDb = NULL;
static char *theDbPath = "/tmp/ctagsdb";
static int gMode = 1; // create
static int gTagCount = 0;
void SetDbAppend(){
    gMode = 2; // modify
}

void SetDbName(const char *dbname){
    char name[1024] = {0};
    if (NULL != dbname){
        sprintf(name, "%s.db", dbname);
    }
    theDbPath = strdup(name);
    printf("SetDbName:%s\n", theDbPath);
}

static void openDb()
{
    if (NULL == theDb){
        printf("Open %s (mode:%d)\n", theDbPath, gMode);
        theDb = dbop_open(theDbPath, gMode, 0644, DBOP_DUP);
    }
    if (NULL == theDb){
        printf("Failed to open db:%s\n", theDbPath);
        die("Failed to open db\n");
    }
}

static int writeDb(const char *key, const char *value)
{
    gTagCount++;
    openDb();
    dbop_put(theDb, key, value);
    return 0;
}

void storeFile(const char *pFileName, const char *pKey){
    if (NULL == pFileName || NULL == pKey){
        return;
    }
    openDb();
    // char key[1024+1] = {0};
    // snprintf(key, 1024, "file/%s", pFileName);
    if (gMode != 2){
        return;
    }
    static char hasDelete = 0;
    if (hasDelete){
        return;
    }
    hasDelete = 1;
    char key[1024+2] = {0};
    snprintf(key, 1025, "%s'", pFileName);
    int len = strlen(key);
    int count = 0;
    const char *pConent = dbop_first(theDb, NULL, NULL, 0);
    while (NULL != pConent){
        if (strncmp(pConent, key, len) == 0){
            count++;
            dbop_delete(theDb, NULL);
        }
        pConent = dbop_next(theDb);
    }
    printf("Append mode, delete %d tag(s) for target(%s)\n", count, pFileName);
}

void OnExit(int code){
    if (NULL != theDb){
        printf("Total add %d tag\n", gTagCount);
        dbop_close(theDb);
        theDb = NULL;
    }
}

static int writeRocksdbEntry (tagWriter *writer,
    MIO * mio, const tagEntryInfo *const tag)
{
    // key is
    // type-class::name-line
    char key[1024+1] = {0};
    char value[4096+1] = {0};
    // int i = 0;
    // for ( i = FIELD_NAME; i < FIELD_BUILTIN_LAST; i++){
    //     printf("[%d] = %s\n", i, renderFieldEscaped(writer->type, i, tag, NO_PARSER_FIELD, NULL));
    // }
    const char *pScope = tag->extensionFields.scopeName;
    if (pScope == NULL){
        pScope = "";
    }
    if (strlen(pScope) > strlen("__anon") && strncmp(pScope, "__anon", strlen("__anon")) == 0){
        if (strcmp("e", renderFieldEscaped(writer->type, FIELD_KIND, tag, NO_PARSER_FIELD, NULL)) == 0){
            // note:fanhongxuan@gmail.com
            // only mark the type of enum to __anon.
            pScope = "__anon";
        }
                // printf("Use __anon as scope\n");
    }
    const char *pLan = renderFieldEscaped(writer->type, FIELD_LANGUAGE, tag, NO_PARSER_FIELD, NULL); 
    if (strcmp("C++", pLan) == 0){
        pLan = "C";
    }
    const char *pFileName = renderFieldEscaped(writer->type, FIELD_INPUT_FILE, tag, NO_PARSER_FIELD, NULL);
    snprintf(key,1024, "%s/%s/%s/%s",
        renderFieldEscaped(writer->type, FIELD_KIND, tag, NO_PARSER_FIELD, NULL), pLan, pScope,
        renderFieldEscaped(writer->type, FIELD_NAME, tag, NO_PARSER_FIELD, NULL));
    // printf("Add %s\n", key);
    snprintf(value, 4096, "%s'%ld'%ld'%s'%s'%s'%s",
        pFileName, tag->lineNumber,tag->extensionFields.endLine,
        renderFieldEscaped(writer->type, FIELD_ACCESS, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_INHERITANCE, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_TYPE_REF, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_SIGNATURE, tag, NO_PARSER_FIELD, NULL));
    // printf("value:%s\n", value);
    storeFile(pFileName, key);
    writeDb(key, value);
    return 0;
}
