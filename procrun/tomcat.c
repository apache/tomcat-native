/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
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
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/* ====================================================================
 * procrun tomcat gui extensions
 *
 * Contributed by Mladen Turk <mturk@apache.org>
 *
 * 05 Aug 2002
 * ==================================================================== 
 */

#if defined(PROCRUN_WINAPP)

#ifndef STRICT
#define STRICT
#endif
#ifndef OEMRESOURCE
#define OEMRESOURCE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>

#include <Shlwapi.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <time.h>
#include <stdarg.h>
#include <jni.h>
 
#include "procrun.h"

prcrun_lview_t tac_columns[] = {
    {   "Type",     60      },
    {   "Date",     80      },
    {   "Time",     60      },
    {   "Class",    172     },
    {   "Message",  300     },
    {   NULL,       0       },
};

static char ac_lv_buf[1024] = {0};
static char *ac_lv_class = NULL;
static char *ac_lv_msg = NULL;
static char *ac_lv_date = NULL;
static char *ac_lv_time = NULL;
static int  ac_lv_iicon = 0;

static char *lv_infos[] = {
    "Info",
    "Warning",
    "Error",
    "Severe"
};


void tc_parse_list_string(const char *str, int from)
{
    int row = 0x7FFFFFFF;
    LV_ITEM lvi;
    int off = 0;

    if (isdigit(*str)) {
        char *p;
                
        strncpy(ac_lv_buf, str, 1023);
        ac_lv_date = p = &ac_lv_buf[0];

        while (*p && !isspace(*p))
            ++p;
        *(p++) = 0;
        while (*p && isspace(*p))
            ++p;
        ac_lv_time = p;

        while (*p && !isspace(*p))
            ++p;
        *(p++) = 0;
        while (*p && isspace(*p))
            ++p;
        ac_lv_class = p;

        while (*p && !isspace(*p))
            ++p;
        *(p++) = 0;
        ac_lv_msg = p;
        
    }
    else {
        if (STRN_COMPARE(str, "INFO:")) {
            ac_lv_iicon = 0;
            off = STRN_SIZE("INFO:") + 1;
        }
        else if (STRN_COMPARE(str, "WARNING:")) {
            ac_lv_iicon = 1;
            off = STRN_SIZE("WARNING:") + 1;
        }
        else if (STRN_COMPARE(str, "ERROR:")) {
            ac_lv_iicon = 2;
            off = STRN_SIZE("ERROR:") + 1;
        }
        else if (STRN_COMPARE(str, "SEVERE:")) {
            ac_lv_iicon = 3;
            off = STRN_SIZE("SEVERE:") + 1;
        }
        ac_lv_msg = (char *)str + off;

        /* skip leading spaces */
        while (*ac_lv_msg && isspace(*ac_lv_msg))
            ++ac_lv_msg;
    }
    
    /* if this is from stderr set the error icon */
    if (from)
        ac_lv_iicon = 2;

    STR_NOT_NULL(ac_lv_class);
    STR_NOT_NULL(ac_lv_msg);
    STR_NOT_NULL(ac_lv_date);
    STR_NOT_NULL(ac_lv_time);

    memset(&lvi, 0, sizeof(LV_ITEM));
    lvi.mask        = LVIF_IMAGE | LVIF_TEXT;
    lvi.iItem       = ac_lview_current;
    lvi.iImage      = ac_lv_iicon;
    lvi.pszText     = lv_infos[ac_lv_iicon];
    lvi.cchTextMax  = 8;
    row = ListView_InsertItem(ac_list_hwnd, &lvi);
    if (row == -1)
        return;
    ListView_SetItemText(ac_list_hwnd, row, 1, ac_lv_date); 
    ListView_SetItemText(ac_list_hwnd, row, 2, ac_lv_time); 
    ListView_SetItemText(ac_list_hwnd, row, 3, ac_lv_class); 
    ListView_SetItemText(ac_list_hwnd, row, 4, ac_lv_msg);
    ListView_EnsureVisible(ac_list_hwnd,
                               ListView_GetItemCount(ac_list_hwnd) - 1,
                               FALSE); 

    ac_lview_current++;
}


void acx_init_extended()
{
    ac_splash_timeout = 20000;
    ac_splash_msg = "INFO: Jk running";
    ac_columns = &tac_columns[0];
    lv_parser = tc_parse_list_string;

}

#endif
