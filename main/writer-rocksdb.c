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
    dbop_put(theDb, key, value);
    return 0;
}

void storeFile(const char *pFileName, const char *pKey){
    if (NULL == pFileName || NULL == pKey){
        return;
    }
    char key[1024+1] = {0};
    snprintf(key, 1024, "file/%s", pFileName);
    if (gMode == 2){
        static char hasDelete = 0;
        if (hasDelete == 0){
            hasDelete = 1;
            char item[1024+1] = {0};
            const char *pContent = dbop_get(theDb, key);
            if (NULL != pContent){
                int len = strlen(pContent);
                int i = 0, j = 0;
                while(i < len){
                    if (pContent[i] == '\n'){
                        item[j] = '\0';
                        j = 0;
                        printf("Delete %s\n", item);
                        dbop_delete(theDb, item);
                    }
                    else{
                        item[j%1024] = pContent[i];
                        j++;
                    }
                    i++;
                }
            }
            dbop_delete(theDb, key);
            printf("Append mode, delete all the old target first(%s)\n", key);
        }
    }
    const char *pValue = dbop_get(theDb, key);
    int len = strlen(pKey);
    if (NULL != pValue){
        len += strlen(pValue);
        len += 1; // for "\n"
        char *pRet = malloc(len+1);
        printf("Add Key:%s\n", pKey);
        snprintf(pRet, len+1, "%s\n%s", pValue, pKey);
        dbop_delete(theDb, key);
        dbop_put(theDb, key, pRet);
        free(pRet);
    }
    else{
        dbop_delete(theDb, key);
        dbop_put(theDb, key, pKey);
    }
    
    // if (NULL == pFileName || NULL == pKey){
    //     return;
    // }
    // if (pFileName != theFileName){
    //     if (theFileName == NULL){
    //         // set the first value.
    //         theFileName = strdup(pFileName);
    //         theFileContent = strdup(pKey);
    //         return;
    //     }
    //     // do store
    //     char key[1024+1] = {0};
    //     snprintf(key, 1024, "file/%s", pFileName);
    //     dbop_put(theDb, key, theFileContent);
    //     free(theFileName); theFileName = NULL;
    //     free(theFileConent); theFileContent = NULL;
    //     if (NULL != pFileName && NULL != pKey){
    //         theFileName = strdup(pFileName);
    //         theFileContent = strdup(pKey);
    //     }
    // }
    // else{
    //     // file is same, append the key to theFileContent
    // }
}

void OnExit(int code){
    if (NULL != theDb){
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
        pScope = "__anon";
        printf("Use __anon as scope\n");
    }
    const char *pLan = renderFieldEscaped(writer->type, FIELD_LANGUAGE, tag, NO_PARSER_FIELD, NULL); 
    if (strcmp("C++", pLan) == 0){
        pLan = "C";
    }
    const char *pFileName = renderFieldEscaped(writer->type, FIELD_INPUT_FILE, tag, NO_PARSER_FIELD, NULL);
    snprintf(key,1024, "%s/%s/%s/%s",
        renderFieldEscaped(writer->type, FIELD_KIND, tag, NO_PARSER_FIELD, NULL), pLan, pScope,
        renderFieldEscaped(writer->type, FIELD_NAME, tag, NO_PARSER_FIELD, NULL));
    // printf("key:%s\n", key);
    snprintf(value, 4096, "%s'%ld'%ld'%s'%s'%s'%s",
        pFileName, tag->lineNumber,tag->extensionFields.endLine,
        renderFieldEscaped(writer->type, FIELD_ACCESS, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_INHERITANCE, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_TYPE_REF, tag, NO_PARSER_FIELD, NULL),
        renderFieldEscaped(writer->type, FIELD_SIGNATURE, tag, NO_PARSER_FIELD, NULL));
    // printf("value:%s\n", value);
    writeDb(key, value);
    
    storeFile(pFileName, key);
    
    // snprintf(key, 1024, "%s/%s%s", pType, pClass, pName);
    // printf("key:%s\n", key);
    return 0;
}
