/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights
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
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
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
 * [Additional notices, if required by prior licensing conditions]
 *
 */

package org.apache.jk.common;

import java.io.*;

import java.net.*;
import java.util.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;

import org.apache.tomcat.util.threads.*;

import org.apache.jk.core.*;
import org.apache.jk.apr.*;

import org.apache.tomcat.util.IntrospectionUtils;


/** Handle the shared memory objects.
 *
 * @author Costin Manolache
 */
public class Shm extends JniHandler {
    String file="/tmp/shm.file";
    int size;

    // Will be dynamic ( getMethodId() ) after things are stable 
    static final int SHM_SET_ATTRIBUTE=0;
    static final int SHM_REGISTER_TOMCAT=2;
    static final int SHM_ATTACH=3;
    static final int SHM_DETACH=4;
    
    public Shm() {
    }
    
    public void setFile( String f ) {
        file=f;
    }

    public void setSize( int size ) {
        this.size=size;
    }

    public void init() throws IOException {
        super.initNative( "shm" );
        if( apr==null ) return;
        if( file==null ) {
            log.error("No shm file, disabling shared memory");
            apr=null;
            return;
        }

        // Set properties and call init.
        setNativeAttribute( "file", file );
        if( size > 0 )
            setNativeAttribute( "size", Integer.toString( size ) );
        attach();
    }

    public void attach() throws IOException {
        if( apr==null ) return;
        MsgContext mCtx=createMsgContext();
        Msg msg=(Msg)mCtx.getMsg(0);
        msg.reset();

        msg.appendByte( SHM_ATTACH );
        
        this.invoke( msg, mCtx );
    }

    public void setNativeAttribute(String name, String val) throws IOException {
        if( apr==null ) return;
        MsgContext mCtx=createMsgContext();
        Msg msg=(Msg)mCtx.getMsg(0);
        C2BConverter c2b=(C2BConverter)mCtx.getNote(C2B_NOTE);
        msg.reset();

        msg.appendByte( SHM_SET_ATTRIBUTE );

        appendString( msg, name, c2b);
        
        appendString(msg, val, c2b );
        
        this.invoke( msg, mCtx );
    }
    
    public void registerTomcat(String host, int port)
        throws IOException
    {
        if( apr==null ) return;
        MsgContext mCtx=createMsgContext();
        Msg msg=(Msg)mCtx.getMsg(0);
        msg.reset();
        C2BConverter c2b=(C2BConverter)mCtx.getNote(C2B_NOTE);

        String slotName="TOMCAT:" + host + ":" + port;
        
        msg.appendByte( SHM_REGISTER_TOMCAT );
        appendString( msg, slotName, c2b );

        this.invoke( msg, mCtx );
    }

    public void destroy() throws IOException {
        if( apr==null ) return;
        
        MsgContext mCtx=createMsgContext();
        Msg msg=(Msg)mCtx.getMsg(0);
        msg.reset();

        msg.appendByte( SHM_DETACH );

        this.invoke( msg, mCtx );
    }

    
    public  int invoke(Msg msg, MsgContext ep )
        throws IOException
    {
        if( apr==null ) return;
        System.err.println("ChannelShm.invoke: "  + ep );
        super.nativeDispatch( msg, ep, JK_HANDLE_SHM_DISPATCH );
        return 0;
    }    

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( Shm.class );

    
    //-------------------- Main - use the shm functions from ant or CLI ------

    public void execute() {
        try {
            init();
        } catch (Exception ex ) {
            ex.printStackTrace();
        }
    }
    
    public static void main( String args[] ) {
        try {
            if( args.length == 1 &&
                ( "-?".equals(args[0]) || "-h".equals( args[0])) ) {
                System.out.println("Usage: ");
                System.out.println("  Shm [OPTIONS]");
                System.out.println();
                System.out.println("  -file SHM_FILE");
                return;
            }

            Shm shm=new Shm();

            IntrospectionUtils.processArgs( shm, args, new String[] {},
                                            null, new Hashtable());
            shm.execute();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }
}
