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

    /** The default size of our tables. */
    private static int defaultsize=32;

    /** The current size of our arrays. */
    private int size;
    
    /** The number of elements present in our arrays. */
    private int num;
    
    /** The array of handlers. */
    private WarpHandler handlers[];
    
    /** The array of rids. */
    private int rids[];

    /**
     * Construct a new WarpHandlerTable instance with the default size.
     */
    public WarpHandlerTable() {
        this(defaultsize);
    }

    /**
     * Construct a new WarpHandlerTable instance with a specified size.
     *
     * @param size The initial size of the table.
     */
    public WarpHandlerTable(int size) {
        super();
        // Paranoid, it's pointless to build a smaller than minimum size.
        if (size<defaultsize) size=defaultsize;

        // Set the current and default sizes (in Hashtable terms the load
        // factor is always 1.0)
        this.defaultsize=size;
        this.size=size;

        // Set the initial values.
        this.num=0;
        this.rids=new int[size];
        this.handlers=new WarpHandler[size];
    }

    /**
     * Get the WarpHandler associated with a specific RID.
     *
     * @param rid The RID number.
     * @return The WarpHandler or null if the RID was not associated with the
     *         specified RID.
     */
    public WarpHandler get(int rid) {
        // Iterate thru the array of rids
        for (int x=0; x<this.num; x++) {

            // We got our rid, return the handler at the same position
            if (this.rids[x]==rid) return(this.handlers[x]);
        }

        // Not found.
        return(null);
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
        // Check if we were given a valid handler
        if (handler==null) throw new NullPointerException("Null Handler");

        // Check if another handler was registered for the specified rid.
        if (this.get(rid)!=null) return(false);

        // Check if we reached the capacity limit
        if(this.size==this.num) {

            // Allocate some space for the new arrays
            int newsize=this.size+defaultsize;
            WarpHandler newhandlers[]=new WarpHandler[newsize];
            int newrids[]=new int[newsize];
            // Copy the original arrays into the new arrays
            System.arraycopy(this.handlers,0,newhandlers,0,this.num);
            System.arraycopy(this.rids,0,newrids,0,this.num);
            // Update our variables
            this.size=newsize;
            this.handlers=newhandlers;
            this.rids=newrids;
        }

        // Add the handler and its rid to the arrays
        this.handlers[this.num]=handler;
        this.rids[this.num]=rid;
        this.num++;
        
        // Whohoo!
        return(true);
    }

    /**
     * Remove the WarpHandler associated with a specified RID.
     *
     * @param rid The RID number of the WarpHandler to remove.
     * @return The old WarpHandler associated with the specified RID or null.
     */
    public WarpHandler remove(int rid) {
        // Iterate thru the array of rids
        for (int x=0; x<this.num; x++) {

            // We got our rid, and we need to get "rid" of its handler :)
            if (this.rids[x]==rid) {
                WarpHandler oldhandler=this.handlers[x];
                // Decrease the number of handlers stored in this table
                this.num--;
                // Move the last record in our arrays in the current position
                // (dirty way to compact a table)
                this.handlers[x]=this.handlers[this.num];
                this.rids[x]=this.rids[this.num];

                // Now we want to see if we need to shrink our arrays (we don't
                // want them to grow indefinitely in case of peak data storage)
                // We check the number of available positions against the value
                // of defaultsize*2, so that our free positions are always 
                // between 0 and defaultsize*2 (this will reduce the number of
                // System.arraycopy() calls. Even after the shrinking is done
                // we still have defaultsize positions available.
                if ((this.size-this.num)>(this.defaultsize<<1)) {

                    // Allocate some space for the new arrays
                    int newsize=this.size-defaultsize;
                    WarpHandler newhandlers[]=new WarpHandler[newsize];
                    int newrids[]=new int[newsize];
                    // Copy the original arrays into the new arrays
                    System.arraycopy(this.handlers,0,newhandlers,0,this.num);
                    System.arraycopy(this.rids,0,newrids,0,this.num);
                    // Update our variables
                    this.size=newsize;
                    this.handlers=newhandlers;
                    this.rids=newrids;
                }
                
                // The handler and its rid were removed, and if necessary the
                // arrays were shrunk. We just need to return the old handler.
                return(oldhandler);
            }
        }

        // Not found.
        return(null);
    }
    
    /**
     * Return the array of WarpHandler objects present in this table.
     *
     * @return An array (maybe empty) of WarpHandler objects.
     */
    public WarpHandler[] handlers() {
        WarpHandler buff[]=new WarpHandler[this.num];
        if (this.num>0) System.arraycopy(this.handlers,0,buff,0,this.num);
        return(buff);
    }
}
