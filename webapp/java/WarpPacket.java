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

public class WarpPacket {



    /** The default size. */

    private static final int defaultsize=4096;



    /** The current buffer. */

    private byte buf[]=null;



    /** The current read position in the buffer. */

    private int rpos=-1;



    /** The current write position in the buffer. */

    private int wpos=-1;

    

    /** The next WarpPacket in a chain. */

    private WarpPacket next=null;



    /** The request id (REQ) of this packet. */

    private int request=-1;



    /** The type id (TYP) of this packet. */

    private int type=-1;



    /** Construct a new instance of a WarpPacket. */

    public WarpPacket() {

        this.reset();

    }



    /** Construct a new instance of a WarpPacket. */

    public WarpPacket(byte buf[]) {

        this.reset(buf);

    }



    /** Construct a new instance of a WarpPacket. */

    public WarpPacket(int request, int type) {

        this.reset(request,type);

    }



    /** Construct a new instance of a WarpPacket. */

    public WarpPacket(byte buf[], int request, int type) {

        this.reset(buf, request,type);

    }



    /** Reset the packet buffer and positions to its default. */

    public void reset() {

        this.rpos=0;

        this.wpos=0;

        if (this.buf==null) buf=new byte[defaultsize];

        else if (this.buf.length>(defaultsize<<1)) this.buf=new byte[defaultsize];

    }



    /** Reset the packet buffer and positions to its default. */

    public void reset(int request, int type) {

        this.reset();

        this.request=request;

        this.type=type;

    }



    /** Reset the packet positions and set the specified byte array as buffer. */

    public void reset(byte buf[]) {

        if (buf==null) {

            this.reset();

            return;

        } else {

            this.rpos=0;

            this.wpos=0;

            this.buf=buf;

        }

    }



    /** Reset the packet positions and set the specified byte array as buffer. */

    public void reset(byte buf[], int request, int type) {

        this.reset(buf);

        this.request=request;

        this.type=type;

    }



    /** Write a short integer (16 bits) in the packet buffer. */

    public void writeShort(int k) {

        // Check if we have room

        if ((this.wpos+2)<this.buf.length) {

            byte newbuf[]=new byte[this.buf.length+defaultsize];

            System.arraycopy(this.buf,0,newbuf,0,this.wpos);

            this.buf=newbuf;

        }

        // Store the number

        this.buf[this.wpos++]=(byte)((k>>8)&0x0ff);

        this.buf[this.wpos++]=(byte)(k&0x0ff);

    }



    /** Write a string in the packet buffer. */

    public void writeString(String s) {

        // Retrieve the string bytes

        byte k[]=s.getBytes();

        // Write the string length

        this.writeShort(k.length);

        // Check if we have room

        if ((this.wpos+k.length)<this.buf.length) {

            byte newbuf[]=new byte[this.buf.length+k.length+defaultsize];

            System.arraycopy(this.buf,0,newbuf,0,this.wpos);

            this.buf=newbuf;

        }

        // Store the string

        System.arraycopy(k,0,this.buf,this.wpos,k.length);

        this.wpos+=k.length;

    }



    /** Read a short integer (16 bits) from the packet buffer. */

    public int readShort() {

        // Check if we have a buffer

        if (this.buf==null) throw new NullPointerException("No buffer");

        // Check if we have room for a short in the buffer

        if ((this.rpos+2)>this.buf.length)

            throw new IllegalStateException("Not enough data ("+this.rpos+"/"+

                                            this.buf.length+")");

        // Read the short and return it

        int x=(((int)this.buf[this.rpos++])&0x0ff);

        x=((x<<8)|(((int)this.buf[this.rpos++])&0x0ff));

        return(x);

    }



    /** Read a string from the packet buffer. */

    public String readString() {

        // Check if we have a buffer

        if (this.buf==null) throw new NullPointerException("No buffer");

        // Attempt to read the string length

        int len=this.readShort();

        // Check if we have enough space in the buffer for the string

        if ((this.rpos+len)>buf.length)

            throw new IllegalStateException("Not enough data ("+this.rpos+"+"+

                                            len+"="+this.buf.length+")");

        // Create the string, update the position and return.

        String s=new String(this.buf,this.rpos,len);

        this.rpos+=len;

        return(s);

    }



    /** Return the content of the packet buffer. */

    public byte[] getBuffer() {

        return(this.buf);

    }



    /** Return the length of the data written to the buffer. */

    public int getLength() {

        return(this.wpos);

    }



    /** Set the next WarpPacket in a chain. */

    public void setNext(WarpPacket next) {

        this.next=next;

    }



    /** Return the next WarpPacket in a chain. */

    public WarpPacket getNext() {

        return(this.next);

    }



    /** Return the request id (REQ) of this packet. */

    public int getRequestID() {

        return(this.request);

    }



    /** Set the request id (REQ) of this packet. */

    public void setRequestID(int request) {

        this.request=request;

    }



    /** Return the type id (TYP) of this packet. */

    public int getTypeID() {

        return(this.type);

    }



    /** Set the type id (TYP) of this packet. */

    public void setTypeID(int type) {

        this.type=type;

    }

}

