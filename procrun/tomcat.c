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


int                    ac_lview_current = 0;
static struct {
    char *  label;
    DWORD   width;
} ac_columns[] = {
    {   "Status",   140     },
    {   "Class",    172     },
    {   "Message",  300     },
};

static char *ac_lv_stat = NULL;
static char ac_lv_clbuf[1024] = {0};
static char *ac_lv_class = NULL;
static char *ac_lv_clmsg = NULL;
static int  ac_lv_iicon = 0;
/* splash window handle */
static HWND ac_splash_hwnd = NULL;
static HWND ac_splist_hwnd;

#define STR_NOT_NULL(s) { if((s) == NULL) (s) = ""; }


LRESULT CALLBACK ac_splash_dlg_proc(HWND hdlg, UINT message, WPARAM wparam, LPARAM lparam)
{

    switch (message) {
        case WM_INITDIALOG:
           ac_splash_hwnd = hdlg;
           ac_center_window(hdlg);
           ac_splist_hwnd = GetDlgItem(hdlg, IDL_INFO); 
           break;
    }

    return FALSE;
}

void acx_create_view(HWND hdlg, LPRECT pr, LPRECT pw)
{
    LV_COLUMN lvc;
    int i;
    HIMAGELIST  imlist;
    HICON hicon; 
    imlist = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 3, 0);
    hicon = LoadImage(ac_instance, MAKEINTRESOURCE(IDI_ICOI),
                      IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    ImageList_AddIcon(imlist, hicon);
    hicon = LoadImage(ac_instance, MAKEINTRESOURCE(IDI_ICOW),
                      IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    ImageList_AddIcon(imlist, hicon);
    hicon = LoadImage(ac_instance, MAKEINTRESOURCE(IDI_ICOS),
                      IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    ImageList_AddIcon(imlist, hicon);
    
    ac_list_hwnd = CreateWindowEx(0L, WC_LISTVIEW, "", 
                                  WS_VISIBLE | WS_CHILD |
                                  LVS_REPORT | WS_EX_CLIENTEDGE,
                                  0, 0, pr->right - pr->left,
                                  pr->bottom - abs((pw->top - pw->bottom)),
                                  hdlg, NULL, ac_instance, NULL);
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt  = LVCFMT_LEFT;
    
    ListView_SetImageList(ac_list_hwnd,imlist, LVSIL_SMALL);
    
    for (i = 0; i < sizeof(ac_columns) / sizeof(ac_columns[0]); ++i)  {
        lvc.iSubItem    = i;
        lvc.cx          = ac_columns[i].width;
        lvc.pszText     = ac_columns[i].label;
        ListView_InsertColumn(ac_list_hwnd, i, &lvc );
    }
#ifdef LVS_EX_FULLROWSELECT
    ListView_SetExtendedListViewStyleEx(ac_list_hwnd, 0,
        LVS_EX_FULLROWSELECT |
        LVS_EX_INFOTIP);
#endif
    
}

void acx_parse_list_string(const char *str)
{
    int row = 0x7FFFFFFF;
    LV_ITEM lvi;
    int off = 0;

    if (isdigit(*str)) {
        char *p;
                
        strncpy(ac_lv_clbuf, str, 1023);
        ac_lv_stat = p = &ac_lv_clbuf[0];

        while (*p && !isspace(*p))
            ++p;
        ++p;
        while (*p && !isspace(*p))
            ++p;
        *(p++) = 0;
        while (*p && isspace(*p))
            ++p;
        ac_lv_class = p;

        while (*p && !isspace(*p))
            ++p;
        *(p++) = 0;
        ac_lv_clmsg = p;
        
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
            ac_lv_iicon = 2;
            off = STRN_SIZE("SEVERE:") + 1;
        }
        ac_lv_clmsg = (char *)str + off;

        /* skip leading spaces */
        while (*ac_lv_clmsg && isspace(*ac_lv_clmsg))
            ++ac_lv_clmsg;
    }

    STR_NOT_NULL(ac_lv_class);
    STR_NOT_NULL(ac_lv_clmsg);
    STR_NOT_NULL(ac_lv_stat);

    memset(&lvi, 0, sizeof(LV_ITEM));
    lvi.mask        = LVIF_IMAGE | LVIF_TEXT;
    lvi.iItem       = ac_lview_current;
    lvi.iImage      = ac_lv_iicon;
    lvi.pszText     = ac_lv_stat;
    lvi.cchTextMax  = strlen(ac_lv_stat) + 1;
    row = ListView_InsertItem(ac_list_hwnd, &lvi);
    if (row == -1)
        return;
    ListView_SetItemText(ac_list_hwnd, row, 1, ac_lv_class); 
    ListView_SetItemText(ac_list_hwnd, row, 2, ac_lv_clmsg);
    ListView_EnsureVisible(ac_list_hwnd,
                               ListView_GetItemCount(ac_list_hwnd) - 1,
                               FALSE); 

    ac_lview_current++;
}


void acx_process_splash(const char *str)
{
   /* set the status to 'green' when received Jk running 
    * and close the spash window if present
    */
    if (STRN_COMPARE(str, "INFO: Jk running")) {
        ac_show_try_icon(ac_main_hwnd, NIM_MODIFY, ac_cmdname, 0);
        /* kill the splash window if present */
        if (ac_splash_hwnd)
            EndDialog(ac_splash_hwnd, TRUE);
        ac_splash_hwnd = NULL;
    }
    else if (ac_splash_hwnd) {
        SendMessage(ac_splist_hwnd, LB_INSERTSTRING, 0, (LPARAM)str);
    }

}

void acx_create_spash(HWND hwnd)
{

    if (ac_use_try) {
        DialogBox(ac_instance, MAKEINTRESOURCE(IDD_DLGSPLASH),
            hwnd, (DLGPROC)ac_splash_dlg_proc);
    }

}

void acx_close_spash()
{
    if (ac_use_try && ac_splash_hwnd)
        EndDialog(ac_splash_hwnd, TRUE);
}

void acx_init_extended()
{
    ac_use_lview = 1;
}

#endif
