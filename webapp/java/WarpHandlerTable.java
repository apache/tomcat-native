/* ========================================================================= *

 *                                                                           *

 *                 The Apache Software License,  Version 1.1                 *

 *                                                                           *

 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *

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

 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *

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

package org.apache.catalina.connector.warp;



/**

 *

 *

 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>

 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The

 *         Apache Software Foundation.

 * @version CVS $Id$

 */

public class WarpHandlerTable {



    /** The table used by this implementation. */

    private WarpTable table=null;



    /**

     * Construct a new WarpHandlerTable instance with the default size.

     */

    public WarpHandlerTable() {

        super();

        this.table=new WarpTable();

    }



    /**

     * Construct a new WarpHandlerTable instance with a specified size.

     *

     * @param size The initial size of the table.

     */

    public WarpHandlerTable(int size) {

        super();

        this.table=new WarpTable(size);

    }



    /**

     * Get the WarpHandler associated with a specific RID.

     *

     * @param rid The RID number.

     * @return The WarpHandler or null if the RID was not associated with the

     *         specified RID.

     */

    public WarpHandler get(int rid) {

        return((WarpHandler)this.table.get(rid));

    }



    /**

     * Associate a WarpHandler with a specified RID.

     *

     * @param handler The WarpHandler to put in the table.

     * @param rid The RID number associated with the WarpHandler.

     * @return If another WarpHandler is associated with this RID return

     *         false, otherwise return true.

     */

    public boolean add(WarpHandler handler, int rid)

    throws NullPointerException {

        return(this.table.add(handler,rid));

    }



    /**

     * Remove the WarpHandler associated with a specified RID.

     *

     * @param rid The RID number of the WarpHandler to remove.

     * @return The old WarpHandler associated with the specified RID or null.

     */

    public WarpHandler remove(int rid) {

        return((WarpHandler)this.table.remove(rid));

    }



    /**

     * Return the array of WarpHandler objects present in this table.

     *

     * @return An array (maybe empty) of WarpHandler objects.

     */

    public WarpHandler[] handlers() {

        int num=this.table.count();

        WarpHandler buff[]=new WarpHandler[num];

        if (num>0) System.arraycopy(this.table.objects,0,buff,0,num);

        return(buff);

    }

}

