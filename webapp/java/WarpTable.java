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

 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>

 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The

 *         Apache Software Foundation.

 * @version CVS $Id$

 */

public class WarpTable {



    /** The default capacity of our tables. */

    private static int defaultcapacity=32;



    /** The current capacity of our arrays. */

    private int capacity;



    /** The number of elements present in our arrays. */

    protected int size;



    /** The array of objects. */

    protected Object objects[];



    /** The array of ids. */

    protected int ids[];



    /**

     * Construct a new WarpTable instance with the default capacity.

     */

    public WarpTable() {

        this(defaultcapacity);

    }



    /**

     * Construct a new WarpTable instance with a specified capacity.

     *

     * @param capacity The initial capacity of the table.

     */

    public WarpTable(int capacity) {

        super();

        // Paranoid, it's pointless to build a smaller than minimum capacity.

        if (capacity<defaultcapacity) capacity=defaultcapacity;



        // Set the current and default capacitys (in Hashtable terms the load

        // factor is always 1.0)

        this.defaultcapacity=capacity;

        this.capacity=capacity;



        // Set the initial values.

        this.size=0;

        this.ids=new int[capacity];

        this.objects=new Object[capacity];

    }



    /**

     * Get the Object associated with a specific ID.

     *

     * @param id The ID number.

     * @return The Object or null if the ID was not associated with the

     *         specified ID.

     */

    public Object get(int id) {

        // Iterate thru the array of ids

        for (int x=0; x<this.size; x++) {



            // We got our id, return the object at the same position

            if (this.ids[x]==id) return(this.objects[x]);

        }



        // Not found.

        return(null);

    }



    /**

     * Associate a Object with a specified ID.

     *

     * @param object The Object to put in the table.

     * @param id The ID number associated with the Object.

     * @return If another Object is associated with this ID return

     *         false, otherwise return true.

     */

    public boolean add(Object object, int id)

    throws NullPointerException {

        // Check if we were given a valid object

        if (object==null) throw new IllegalArgumentException("Null Object");

        if (id<0) throw new IllegalArgumentException("ID is less than 0");



        // Check if another object was registered for the specified id.

        if (this.get(id)!=null) return(false);



        // Check if we reached the capacity limit

        if(this.capacity==this.size) {



            // Allocate some space for the new arrays

            int newcapacity=this.capacity+defaultcapacity;

            Object newobjects[]=new Object[newcapacity];

            int newids[]=new int[newcapacity];

            // Copy the original arrays into the new arrays

            System.arraycopy(this.objects,0,newobjects,0,this.size);

            System.arraycopy(this.ids,0,newids,0,this.size);

            // Update our variables

            this.capacity=newcapacity;

            this.objects=newobjects;

            this.ids=newids;

        }



        // Add the object and its id to the arrays

        this.objects[this.size]=object;

        this.ids[this.size]=id;

        this.size++;



        // Whohoo!

        return(true);

    }



    /**

     * Remove the Object associated with a specified ID.

     *

     * @param id The ID number of the Object to remove.

     * @return The old Object associated with the specified ID or null.

     */

    public Object remove(int id) {

        // Iterate thru the array of ids

        for (int x=0; x<this.size; x++) {



            // We got our id, and we need to get rid of its object

            if (this.ids[x]==id) {

                Object oldobject=this.objects[x];

                // Decrease the number of objects stored in this table

                this.size--;

                // Move the last record in our arrays in the current position

                // (dirty way to compact a table)

                this.objects[x]=this.objects[this.size];

                this.ids[x]=this.ids[this.size];



                // Now we want to see if we need to shrink our arrays (we don't

                // want them to grow indefinitely in case of peak data storage)

                // We check the number of available positions against the value

                // of defaultcapacity*2, so that our free positions are always

                // between 0 and defaultcapacity*2 (this will reduce the number

                // of System.arraycopy() calls. Even after the shrinking is done

                // we still have defaultcapacity positions available.

                if ((this.capacity-this.size)>(this.defaultcapacity<<1)) {



                    // Allocate some space for the new arrays

                    int newcapacity=this.capacity-defaultcapacity;

                    Object newobjects[]=new Object[newcapacity];

                    int newids[]=new int[newcapacity];

                    // Copy the original arrays into the new arrays

                    System.arraycopy(this.objects,0,newobjects,0,this.size);

                    System.arraycopy(this.ids,0,newids,0,this.size);

                    // Update our variables

                    this.capacity=newcapacity;

                    this.objects=newobjects;

                    this.ids=newids;

                }



                // The object and its id were removed, and if necessary the

                // arrays were shrunk. We just need to return the old object.

                return(oldobject);

            }

        }



        // Not found.

        return(null);

    }



    /**

     * Return the number of objects present in this table.

     *

     * @return The number of objects present in this table.

     */

    public int count() {

        return(this.size);

    }

}

