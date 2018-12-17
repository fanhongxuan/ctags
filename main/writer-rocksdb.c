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
int gMode = 1;

void SetDbAppend(){
    gMode = 2;
}

void SetDbName(const char *dbname){
    char name[1024] = {0};
    if (NULL != dbname){
        sprintf(name, "%s.db", dbname);
    }
    theDbPath = strdup(name);
    printf("SetDbName:%s\n", theDbPath);
}

void OnExit(int code){
    if (NULL != theDb){
        dbop_close(theDb);
        theDb = NULL;
    }
}

static int writeDb(const char *key, const char *value)
{
    if (NULL == theDb){
        printf("Open %s (mode:%d)\n", theDbPath, gMode);
        theDb = dbop_open(theDbPath, gMode, 0644, DBOP_DUP);
    }
    if (NULL == theDb){
        printf("Failed to open db:%s\n", theDbPath);
        return -1;
    }
    // todo:fanhongxuan@gmail.com
    //
    dbop_put(theDb, key, value);
    return 0;
}

static int writeRocksdbEntry (tagWriter *writer,
    MIO * mio, const tagEntryInfo *const tag)
{
    // key is
    // type-class::name-line
    char key[1024] = {0};
    char value[4096] = {0};
    // int i = 0;
    // for ( i = FIELD_NAME; i < FIELD_BUILTIN_LAST; i++){
    //     printf("[%d] = %s\n", i, renderFieldEscaped(writer->type, i, tag, NO_PARSER_FIELD, NULL));
    // }
    const char *pScope = tag->extensionFields.scopeName;
    if (pScope == NULL){
        pScope = "";
    }
    const char *pLan = renderFieldEscaped(writer->type, FIELD_LANGUAGE, tag, NO_PARSER_FIELD, NULL); 
    if (strcmp("C++", pLan) == 0){
        pLan = "C";
    }
    snprintf(key,1024, "%s/%s/%s/%s",
        renderFieldEscaped(writer->type, FIELD_KIND, tag, NO_PARSER_FIELD, NULL), pLan, pScope,
        renderFieldEscaped(writer->type, FIELD_NAME, tag, NO_PARSER_FIELD, NULL));
    printf("key:%s\n", key);
    snprintf(value, 1024, "%s'%ld'%ld'%s'%s'%s'%s",
        renderFieldEscaped(writer->type, FIELD_INPUT_FILE, tag, NO_PARSER_FIELD, NULL),
        tag->lineNumber,tag->extensionFields.endLine,
        renderFieldEscaped(writer->type, FIELD_ACCESS, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_INHERITANCE, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_TYPE_REF, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_SIGNATURE, tag, NO_PARSER_FIELD, NULL));
    printf("value:%s\n", value);
    writeDb(key, value);
    // snprintf(key, 1024, "%s/%s%s", pType, pClass, pName);
    // printf("key:%s\n", key);
    return 0;
}
