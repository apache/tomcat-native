/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and
 *    "Java Apache Project" must not be used to endorse or promote products
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: DSAPI plugin for Lotus Domino                              *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Date:        20010603                                                   *
 * Version:     $Revision$                                             *
 ***************************************************************************/

#include "config.h"
#include "inifile.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* We have one of these for each ini file line. Once we've read the
 * file and parsed it we'll have an array in key order containing one
 * of these for each configuration item in file.
 */
typedef struct
{
	const char *key;
	const char *value;

} inifile_key;

static char *file;			/* the text of the ini file				*/
static inifile_key *keys;	/* an array of keys, one per item		*/
static size_t klen;			/* length of the key array				*/

/* Text that will prefix all of our error messages */
#define ERRPFX "INIFILE: "
/* Various error messages that we can return */
ERRTYPE inifile_outofmemory		= ERRPFX "Out of memory";
ERRTYPE inifile_filenotfound	= ERRPFX "File not found";
ERRTYPE inifile_readerror		= ERRPFX "Error reading file";

/* Discard the current ini file */
void inifile_dispose(void)
{
	if (NULL != file)
	{
		free(file);
		file = NULL;
	}

	if (NULL != keys)
	{
		free(keys);
		keys = NULL;
		klen = 0;
	}
}

/* Case insensitive string comparison, works like strcmp() */
static int inifile__stricmp(const char *s1, const char *s2)
{
	while (*s1 && tolower(*s1) == tolower(*s2))
		s1++, s2++;
	return tolower(*s1) - tolower(*s2);
}

/* Compare keys, suitable for passing to qsort() */
static int inifile__cmp(const void *k1, const void *k2)
{
	const inifile_key *kk1 = (const inifile_key *) k1;
	const inifile_key *kk2 = (const inifile_key *) k2;
	return inifile__stricmp(kk1->key, kk2->key);
}

/* Various macros to tidy up the parsing code */

/* Skip whitespace */
#define SKIPSPC() \
	while (*fp == '\t' || *fp == ' ') fp++

/* Skip to the end of the current line */
#define SKIPLN() \
	while (*fp != '\0' && *fp != '\r' && *fp != '\n') fp++

/* Move from the end of the current line to the start of the next */
#define NEXTLN() \
	while (*fp == '\r' || *fp == '\n') fp++

/* Build the index. Called when the inifile is loaded by inifile_load()
 */
static ERRTYPE inifile__index(void)
{
	int pass;

	/* Make two passes over the data. First time we're just counting
	 * the lines that contain values so we can allocate the index, second
	 * time we build the index.
	 */
	for (pass = 0; pass < 2; pass++)
	{
		char *fp = file;
		char *ks = NULL, *ke;	/* key start, end */
		char *vs = NULL, *ve;	/* value start, end */
		klen = 0;

		while (*fp != '\0')
		{
			SKIPSPC();

			/* turn a comment into an empty line by moving to the next \r|\n */
			if (*fp == '#' || *fp == ';')
				SKIPLN();

			if (*fp != '\0' && *fp != '\r' && *fp != '\n')
			{
				ks = fp;		/* start of key */
				while (*fp > ' ' && *fp != '=') fp++;
				ke = fp;		/* end of key */
				SKIPSPC();
				if (*fp == '=') /* lines with no equals are ignored */
				{
					fp++; /* past the = */
					SKIPSPC();
					vs = fp;
					SKIPLN();
					ve = fp;
					/* back up over any trailing space */
					while (ve > vs && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
					NEXTLN(); /* see note[1] below */

					if (NULL != keys) /* second pass? if so stash a pointer */
					{
						*ke = '\0';
						*ve = '\0';
						keys[klen].key = ks;
						keys[klen].value = vs;
					}

					klen++;
				}
			}

			/* [1] you may notice that this macro might get invoked twice
			 * for any given line. This isn't a problem -- it won't do anything
			 * if it's called other than at the end of a line, and it needs to
			 * be called the first time above to move fp past the line end
			 * before the line end gets trampled on during the stringification
			 * of the value.
			 */
			NEXTLN();
		}

		if (NULL == keys && (keys = malloc(sizeof(inifile_key) * klen), NULL == keys))
			return inifile_outofmemory;
	}

	/* got the index now, sort it so we can search it quickly */
	qsort(keys, klen, sizeof(inifile_key), inifile__cmp);

	return ERRNONE;
}

/* Read an INI file from disk
 */
ERRTYPE inifile_read(const char *name)
{
	FILE *fl;
	ERRTYPE e;
	size_t flen;

	inifile_dispose();

	if (fl = fopen(name, "r"), NULL == fl)
		return inifile_filenotfound;

	fseek(fl, 0L, SEEK_END);
	flen = (size_t) ftell(fl);
	fseek(fl, 0L, SEEK_SET);

	/* allocate one extra byte for trailing \0
	 */
	if (file = malloc(flen+1), NULL == file)
	{
		fclose(fl);
		return inifile_outofmemory;
	}

	if (fread(file, flen, 1, fl) < 1)
	{
		inifile_dispose();
		fclose(fl);
		return inifile_readerror;
	}

	file[flen] = '\0';	/* terminate it to simplify parsing */

	if (e = inifile__index(), ERRNONE != e)
	{
		inifile_dispose();
		fclose(fl);
	}

	return e;
}

/* Find the value associated with the given key returning it or NULL
 * if no match is found. Key name matching is case insensitive.
 */
const char *inifile_lookup(const char *key)
{
	int lo, mid, hi, cmp;

	if (NULL == keys)
		return NULL;

	for (lo = 0, hi = klen-1; lo <= hi; )
	{
		mid = (lo + hi) / 2;
		cmp = inifile__stricmp(key, keys[mid].key);
		if (cmp < 0)	/* key in array is greater */
			hi = mid-1;
		else if (cmp > 0)
			lo = mid+1;
		else
			return keys[mid].value;
	}

	return NULL;
}
