/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
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
 * 4. The names "The Jakarta Project",  "Apache WebApp Module",  and "Apache *
 *    Software Foundation"  must not be used to endorse or promote  products *
 *    derived  from this  software  without  prior  written  permission. For *
 *    written permission, please contact <apache@apache.org>.                *
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

/* ------------------------------------------------------------------------- *
 * Author Pier Fumagalli <pier.fumagalli@sun.com>
 * Version $Id$
 * ------------------------------------------------------------------------- */

#include <stdio.h>

int main(void) {
    char buf[1024];
    char *ret=NULL;
    int x=0;
    int y=0;
    int k=0;
    int s=0;
    int l=0;

    while ((ret=fgets(buf,1024,stdin))!=NULL) {
        for (x=strlen(ret); x>=0; x--) {
            if((ret[x]=='\0')||(ret[x]=='\t')||(ret[x]=='\n')||(ret[x]==' '))
                ret[x]='\0';
            else break;
        }

        k=0;
        for (x=0; x<strlen(ret); x++) {
            if (ret[x]=='\t') {
                fprintf(stdout," ");
                k++;
                s=((((k/4)+1)*4)-k);
                if (s==4) s=0;
                for (y=0; y<s; y++) fprintf(stdout," ");
                k+=s;
            } else {
                fprintf(stdout,"%c",ret[x]);
                k++;
            }
        }
        fprintf(stdout,"\n");
        l++;
        if (k>=80) fprintf(stderr,"line %4d (%4d) [%s]\n",l,k,ret);
    }
}
