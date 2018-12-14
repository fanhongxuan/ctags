/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to entry.c
*/

#include "general.h"  /* must always come first */

#include "entry_p.h"
#include "mio.h"
#include "options_p.h"
#include "parse_p.h"
#include "ptag_p.h"
#include "read.h"
#include "writer_p.h"


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

static int writeRocksdbEntry (tagWriter *writer,
    MIO * mio, const tagEntryInfo *const tag)
{
    const char *pType = NULL;
    const char *pName = tag->name;
    const char *pClass = NULL;
    kindDefinition *kdef = getLanguageKind(tag->langType, tag->kindIndex);
    const char kind_letter_str[2] = {kdef->letter, '\0'};
    if (kdef->name != NULL && (isFieldEnabled (FIELD_KIND_LONG)  ||
            (isFieldEnabled (FIELD_KIND)  && kdef->letter == KIND_NULL)))
	{
		/* Use kind long name */
		pType = kdef->name;
	}
	else if (kdef->letter != KIND_NULL  && (isFieldEnabled (FIELD_KIND) ||
			(isFieldEnabled (FIELD_KIND_LONG) &&  kdef->name == NULL)))
	{
		/* Use kind letter */
		pType = kind_letter_str;
	}
    if (NULL == pType){
        pType = "";
    }
    if (NULL == pName){
        pName = "";
    }
    if (NULL == pClass){
        pClass = "";
    }
    // key is
    // type-class::name-line
    char key[1024] = {0};
    
    snprintf(key, 1024, "%s/%s%s/%d", pType, pClass, pName, tag->lineNumber);
    printf("key:%s\n", key);
    return 0;
}
