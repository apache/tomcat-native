/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

/***************************************************************************
 * Description: DSAPI plugin for Lotus Domino                              *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Date:        20010603                                                   *
 * Version:     $Revision$                                           *
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
#define SYNFMT ERRPFX "File %s, line %d: %s"

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

/* Return a new syntax error message. */
static ERRTYPE inifile__syntax(jk_pool_t *p, const char *file, int line, const char *msg)
{
	static const char synfmt[] = SYNFMT;
	size_t len = sizeof(synfmt) + strlen(msg) + strlen(file) + 10 /* fudge for line number */;
	char *buf = jk_pool_alloc(p, len);
	sprintf(buf, synfmt, file, line, msg);
	return buf;
}

/* Various macros to tidy up the parsing code */

/* Characters that are OK in the keyname */
#define KEYCHR(c) \
	(isalnum(c) || (c) == '.' || (c) == '_')

/* Skip whitespace */
#define SKIPSPC() \
	while (*fp == '\t' || *fp == ' ') fp++

/* Skip to the end of the current line */
#define SKIPLN() \
	while (*fp != '\0' && *fp != '\r' && *fp != '\n') fp++

/* Move from the end of the current line to the start of the next, learning what the
 * newline character is and counting lines
 */
#define NEXTLN() \
	do { while (*fp == '\r' || *fp == '\n') { if (nlc == -1) nlc = *fp; if (*fp == nlc) ln++; fp++; } } while (0)

/* Build the index. Called when the inifile is loaded by inifile_load()
 */
static ERRTYPE inifile__index(jk_pool_t *p, const char *name)
{
	int pass;
	int ln = 1;
	int nlc = -1;

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
				while (KEYCHR(*fp)) fp++;
				ke = fp;		/* end of key */
				SKIPSPC();

				if (*fp != '=')
					return inifile__syntax(p, name, ln, "Missing '=' or illegal character in key");

				fp++; /* past the = */
				SKIPSPC();
				vs = fp;
				SKIPLN();
				ve = fp;
				/* back up over any trailing space */
				while (ve > vs && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
				NEXTLN(); /* move forwards *before* we trash the eol characters */

				if (NULL != keys) /* second pass? if so stash a pointer */
				{
					*ke = '\0';
					*ve = '\0';
					keys[klen].key = ks;
					keys[klen].value = vs;
				}

				klen++;
			}
			else
			{
				NEXTLN();
			}
		}

		if (NULL == keys && (keys = jk_pool_alloc(p, sizeof(inifile_key) * klen), NULL == keys))
			return inifile_outofmemory;
	}

	/* got the index now, sort it so we can search it quickly */
	qsort(keys, klen, sizeof(inifile_key), inifile__cmp);

	return ERRNONE;
}

/* Read an INI file from disk
 */
ERRTYPE inifile_read(jk_pool_t *p, const char *name)
{
	FILE *fl;
	size_t flen;
	int ok;

	if (fl = fopen(name, "rb"), NULL == fl)
		return inifile_filenotfound;

	fseek(fl, 0L, SEEK_END);
	flen = (size_t) ftell(fl);
	fseek(fl, 0L, SEEK_SET);

	/* allocate one extra byte for trailing \0
	 */
	if (file = jk_pool_alloc(p, flen+1), NULL == file)
	{
		fclose(fl);
		return inifile_outofmemory;
	}

	ok = (fread(file, flen, 1, fl) == 1);
	fclose(fl);
	if (!ok) return inifile_readerror;

	file[flen] = '\0';	/* terminate it to simplify parsing */

	return inifile__index(p, name);
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

#ifdef TEST

static jk_pool_t pool;
extern void jk_dump_pool(jk_pool_t *p, FILE *f); /* not declared in header */

int main(void)
{
	ERRTYPE e;
	unsigned k;
	int rc = 0;

	jk_open_pool(&pool, NULL, 0);

	e = inifile_read(&pool, "ok.ini");
	if (e == ERRNONE)
	{
		printf("%u keys in ok.ini\n", klen);
		for (k = 0; k < klen; k++)
		{
			const char *val = inifile_lookup(keys[k].key);
			printf("Key: \"%s\", value: \"%s\"\n", keys[k].key, val);
		}
	}
	else
	{
		printf("Error reading ok.ini: %s\n", e);
		rc = 1;
	}

	e = inifile_read(&pool, "bad.ini");
	if (e == ERRNONE)
	{
		printf("%u keys in bad.ini\n", klen);
		for (k = 0; k < klen; k++)
		{
			const char *val = inifile_lookup(keys[k].key);
			printf("Key: \"%s\", value: \"%s\"\n", keys[k].key, val);
		}
		rc = 1;		/* should be a syntax error */
	}
	else
	{
		printf("Error reading bad.ini: %s (which is OK: that's what we expected)\n", e);
	}

	jk_dump_pool(&pool, stdout);
	jk_close_pool(&pool);

	return rc;
}
#endif
