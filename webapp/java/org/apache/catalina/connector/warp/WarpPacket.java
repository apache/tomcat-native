/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
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

import java.io.UnsupportedEncodingException;

public class WarpPacket {

    /** This packet's data buffer */
    protected byte buffer[]=null;
    /** Number of bytes stored in the buffer */
    protected int size=0;

    /* Pointer to the last byte read in the buffer */
    protected int pointer=0;
    /* Type of this packet */
    private int type=-1;
    /* Maximum value for a 16 bit unsigned value (0x0ffff +1) */
    private static final int MAX_LENGTH=65535;

    /**
     * Construct a new WarpPacket instance.
     */
    public WarpPacket() {
        super();
        this.buffer=new byte[MAX_LENGTH];
        this.reset();
    }

    /**
     * Reset this packet.
     */
    public void reset() {
        this.pointer=0;
        this.size=0;
        this.type=Constants.TYPE_INVALID;
    }

    /**
     * Set this packet type.
     *
     * @param type The type of this packet.
     */
    public void setType(int type) {
        this.type=type;
    }

    /**
     * Return the type of this packet.
     *
     * @return The type of this packet.
     */
    public int getType() {
        return(this.type);
    }

    /**
     * Write an unsigned short value (16 bit) in the packet buffer.
     *
     * @param value The unsigned short value to write.
     * @exception IllegalArgumentException If the value is negative or greater
     *                than 65535.
     * @exception ArrayIndexOutOfBoundsException If the packet buffer cannot
     *                contain the new value.
     */
    public void writeUnsignedShort(int value) {
        if (value<0)
            throw new IllegalArgumentException("Negative unsigned short");
        if (value>65535)
            throw new IllegalArgumentException("Unsigned short is too big");

        if ((this.size+2)>=MAX_LENGTH)
            throw new ArrayIndexOutOfBoundsException("Too much data");

        this.buffer[this.size++]=(byte) ((value>>8)&0x0ff);
        this.buffer[this.size++]=(byte) ((value>>0)&0x0ff);
    }

    /**
     * Write a signed integer value (32 bit) in the packet buffer.
     *
     * @param value The signed integer value to write.
     * @exception ArrayIndexOutOfBoundsException If the packet buffer cannot
     *                contain the new value.
     */
    public void writeInteger(int value) {
        if ((this.size+4)>=MAX_LENGTH)
            throw new ArrayIndexOutOfBoundsException("Too much data");

        this.buffer[this.size++]=(byte) ((value>>24)&0x0ff);
        this.buffer[this.size++]=(byte) ((value>>16)&0x0ff);
        this.buffer[this.size++]=(byte) ((value>>8)&0x0ff);
        this.buffer[this.size++]=(byte) ((value>>0)&0x0ff);
    }

    /**
     * Write a string into the packet buffer.
     *
     * @param string The string to write into the packet buffer.
     * @exception ArrayIndexOutOfBoundsException If the packet buffer cannot
     *                contain the new value.
     * @exception RuntimeException If the platform doesn't support UTF-8
     *                encoding.
     */
    public void writeString(String string) {
        try {
            if (string==null) string="";
            byte temp[]=string.getBytes("UTF-8");
            if ((this.size+temp.length+2)>MAX_LENGTH)
                throw new ArrayIndexOutOfBoundsException("Too much data");

            this.writeUnsignedShort(temp.length);
            System.arraycopy(temp,0,this.buffer,this.size,temp.length);
            this.size+=temp.length;
        } catch (UnsupportedEncodingException s) {
            throw new RuntimeException("Unsupported encoding UTF-8");
        }
    }

    /**
     * Read an unsigned short value (16 bit) from the packet buffer.
     *
     * @return The unsigned short value as an integer.
     * @exception ArrayIndexOutOfBoundsException If no data is left in the
     *                packet buffer to be read.
     */
    public int readUnsignedShort() {
        if ((this.pointer+2)>this.size)
            throw new ArrayIndexOutOfBoundsException("No data available");

        int k=(this.buffer[this.pointer++])&0xff;
        k=(k<<8)+((this.buffer[this.pointer++])&0xff);

        return(k);
    }

    /**
     * Read a signed integer value (32 bit) from the packet buffer.
     *
     * @return The signed integer value.
     * @exception ArrayIndexOutOfBoundsException If no data is left in the
     *                packet buffer to be read.
     */
    public int readInteger() {
        if ((this.pointer+4)>this.size)
            throw new ArrayIndexOutOfBoundsException("No data available");

        int k=(this.buffer[this.pointer++])&0xff;
        k=(k<<8)+((this.buffer[this.pointer++])&0xff);
        k=(k<<8)+((this.buffer[this.pointer++])&0xff);
        k=(k<<8)+((this.buffer[this.pointer++])&0xff);

        return(k);
    }

    /**
     * Read a string from the packet buffer.
     *
     * @return The string red from the packet buffer.
     * @exception ArrayIndexOutOfBoundsException If no data is left in the
     *                packet buffer to be read.
     */
    public String readString() {
        int length=this.readUnsignedShort();
        try {
            String ret=new String(this.buffer,this.pointer,length,"UTF-8");
            this.pointer+=length;
            return(ret);
        } catch (UnsupportedEncodingException s) {
            throw new RuntimeException("Unsupported encoding UTF-8");
        }
    }

    public String dump() {
        StringBuffer buf=new StringBuffer("DATA=");
        for (int x=0; x<this.size; x++) {
            if ((this.buffer[x]>32)&&(this.buffer[x]<127)) {
                buf.append((char)this.buffer[x]);
            } else {
                buf.append("0x");
                String digit=Integer.toHexString((int)this.buffer[x]);
                if (digit.length()<2) buf.append('0');
                if (digit.length()>2) digit=digit.substring(digit.length()-2);
                buf.append(digit);
            }
            buf.append(" ");
        }
        return(buf.toString());
    }
}
