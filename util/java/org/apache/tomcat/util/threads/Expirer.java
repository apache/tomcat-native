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
package org.apache.tomcat.util.threads;

import java.io.IOException;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;
import org.apache.tomcat.util.buf.*;

/**
 * Expire unused objects. 
 * 
 */
public final class Expirer  implements ThreadPoolRunnable
{
    // We can use Event/Listener, but this is probably simpler
    // and more efficient
    public static interface ExpireCallback {
	public void expired( TimeStamp o );
    }
    
    private int checkInterval = 60;
    private Reaper reaper;
    ExpireCallback expireCallback;

    public Expirer() {
    }

    // ------------------------------------------------------------- Properties
    public int getCheckInterval() {
	return checkInterval;
    }

    public void setCheckInterval(int checkInterval) {
	this.checkInterval = checkInterval;
    }

    public void setExpireCallback( ExpireCallback cb ) {
	expireCallback=cb;
    }
    
    // -------------------- Managed objects --------------------
    static final int INITIAL_SIZE=8;
    TimeStamp managedObjs[]=new TimeStamp[INITIAL_SIZE];
    int managedLen=managedObjs.length;
    int managedCount=0;
    
    public void addManagedObject( TimeStamp ts ) {
	synchronized( managedObjs ) {
	    if( managedCount >= managedLen ) {
		// What happens if expire is on the way ? Nothing,
		// expire will do it's job on the old array ( GC magic )
		// and the expired object will be marked as such
		// Same thing would happen ( and did ) with Hashtable
		TimeStamp newA[]=new TimeStamp[ 2 * managedLen ];
		System.arraycopy( managedObjs, 0, newA, 0, managedLen);
		managedObjs = newA;
		managedLen = 2 * managedLen;
	    }
	    managedObjs[managedCount]=ts;
	    managedCount++;
	}
    }

    public void removeManagedObject( TimeStamp ts ) {
	for( int i=0; i< managedCount; i++ ) {
	    if( ts == managedObjs[i] ) {
		synchronized( managedObjs ) {
		    managedObjs[ i ] = managedObjs[managedCount-1];
		    managedCount--;
		}
		return;
	    }
	}
    }
    
    // --------------------------------------------------------- Public Methods

    public void start() {
	// Start the background reaper thread
	if( reaper==null) {
	    reaper=new Reaper("Expirer");
	    reaper.addCallback( this, checkInterval * 1000 );
	}
	
	reaper.startReaper();
    }

    public void stop() {
	reaper.stopReaper();
    }


    // -------------------------------------------------------- Private Methods

    // ThreadPoolRunnable impl

    public Object[] getInitData() {
	return null;
    }

    public void runIt( Object td[] ) {
	long timeNow = System.currentTimeMillis();
	if( dL > 2 ) debug( "Checking " + timeNow );
	for( int i=0; i< managedCount; i++ ) {
	    TimeStamp ts=managedObjs[i];
	    
	    if (ts==null || !ts.isValid())
		continue;
	    
	    long maxInactiveInterval = ts.getMaxInactiveInterval();
	    if( dL > 3 ) debug( "TS: " + maxInactiveInterval + " " +
				ts.getLastAccessedTime());
	    if (maxInactiveInterval < 0)
		continue;
	    
	    long timeIdle = timeNow - ts.getLastAccessedTime();
	    
	    if (timeIdle >= maxInactiveInterval) {
		if( expireCallback != null ) {
		    if( dL > 0 )
			debug( ts + " " + timeIdle + " " +
			       maxInactiveInterval );
		    expireCallback.expired( ts );
		}
	    }
	}
    }

    private static final int dL=0;
    private void debug( String s ) {
	System.out.println("Expirer: " + s );
    }
}
