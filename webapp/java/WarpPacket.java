package org.apache.catalina.connector.warp;

import java.io.UnsupportedEncodingException;

public class WarpPacket {

    /** This packet's data buffer */
    protected byte buffer[]=null;
    /** Number of bytes stored in the buffer */
    protected int size=0;

    /* Pointer to the last byte read in the buffer */
    private int pointer=0;
    /* Type of this packet */
    private int type=-1;
    /* Maximum value for a 16 bit unsigned value (0x0ffff +1) */
    private static final int MAX_LENGTH=65536;

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
            
        int k=0;
        k=(k<<8)|(this.buffer[this.pointer++]);
        k=(k<<8)|(this.buffer[this.pointer++]);

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
            
        int k=0;
        k=(k<<8)|(this.buffer[this.pointer++]);
        k=(k<<8)|(this.buffer[this.pointer++]);
        k=(k<<8)|(this.buffer[this.pointer++]);
        k=(k<<8)|(this.buffer[this.pointer++]);

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
            return(new String(this.buffer,this.pointer,length,"UTF-8"));
        } catch (UnsupportedEncodingException s) {
            throw new RuntimeException("Unsupported encoding UTF-8");
        }
    }
}
