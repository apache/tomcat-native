package org.apache.jk.apr;

import java.io.*;
import java.lang.reflect.*;
import java.util.*;
import org.apache.jk.core.*;

/** Implements the interface with the APR library. This is for internal-use
 *  only. The goal is to use 'natural' mappings for user code - for example
 *  java.net.Socket for unix-domain sockets, etc. 
 * 
 */
public class AprImpl extends JkHandler { // This will be o.a.t.util.handler.TcHandler - lifecycle and config
    static AprImpl aprSingleton=null;

    String baseDir;
    String aprHome;
    String soExt="so";

    boolean ok=true;
    // Handlers for native callbacks
    Hashtable jkHandlers=new Hashtable();

    public AprImpl() {
        aprSingleton=this;
    }
    
    // -------------------- Properties --------------------
    
    /** Native libraries are located based on base dir.
     *  XXX Add platform, version, etc
     */
    public void setBaseDir(String s) {
        baseDir=s;
    }
    
    public void setSoExt(String s ) {
        soExt=s;
    }
    
    // XXX maybe install the jni lib in apr-home ?
    public void setAprHome( String s ) {
        aprHome=s;
    }

    /** Add a Handler for jni callbacks.
     */
    public void addJkHandler(String type, JkHandler cb) {
        jkHandlers.put( type, cb );
    }
    
    // -------------------- Apr generic utils --------------------
    /** Initialize APR
     */
    public native int initialize();

    public native int terminate();

    public native long poolCreate(long parentPool);

    public native long poolClear(long pool);

    // -------------------- Unix sockets --------------------
    // XXX Will be 'apr sockets' as soon as APR supports unix domain sockets.
    // For the moment there is little benefit of using APR TCP sockets, since
    // the VM abstraction is decent. However poll and other advanced features
    // are not available - and will be usefull. For the next release.
    
    public native long unSocketClose( long pool, long socket, int type );

    /** Create a unix socket and start listening. 
     *  @param file the name of the socket
     *  @param bl backlog
     */
    public native long unSocketListen( long pool, String file, int bl );
    
    /** Create a unix socket and connect. 
     *  @param file the name of the socket
     *  @param bl backlog
     */
    public native long unSocketConnect( long pool, String file );

    /** Accept a connection.
     */
    public native long unAccept( long pool, long unListeningSocket );

    public native int unRead( long pool, long unSocket,
                                byte buf[], int off, int len );

    public native int unWrite( long pool, long unSocket,
                                byte buf[], int off, int len );

    // -------------------- Shared memory methods --------------------

    public native long shmAttach( long pool, String file );

    public native long shmCreate( long pool, long size, String file );

    public native long shmBaseaddrGet( long pool, long shmP );

    public native long shmSizeGet( long pool, long shmP );

    public native long shmDetach( long pool, long shmP );

    public native long shmDestroy( long pool, long shmP );

    public native int shmRead( long pool, long mP, 
                               byte buf[], int off, int len );
    
    public native int shmWrite( long pool, long mP, 
                                byte buf[], int off, int len );


    // -------------------- Mutexes --------------------

    public native long mutexCreate( long pool, String file, int type );

    public native long mutexLock( long pool, long mutexP );

    public native long mutexUnLock( long pool, long mutexP );

    public native long mutexTryLock( long pool, long mutexP );

    public native long mutexDestroy( long pool, long mutexP );
    
    // --------------------  Interface to jk components --------------------
    // 

    /* Return a jk_env_t, used to keep the execution context ( temp pool, etc )
     */
    public native long getJkEnv();

    /** Clean the temp pool, put back the env in the pool
     */
    public native void releaseJkEnv(long xEnv);

    public native void jkRecycle(long xEnv, long endpointP);

    /** Get a native component
     *  @return 0 if the component is not found.
     */
    public native long getJkHandler(long xEnv, String compName );

    public native long createJkHandler(long xEnv, String compName );

    /** Get the id of a method.
     *  @return -1 if the method is not found.
     */
    public native int jkGetId(long xEnv, String ns, String name );

    /** Send the packet to the C side. On return it contains the response
     *  or indication there is no response. Asymetrical because we can't
     *  do things like continuations.
     */
    public static native int jkInvoke(long xEnv, long componentP, long endpointP,
                                      int code, byte data[], int len);

    
    // -------------------- Called from C --------------------
    // XXX Check security, add guard or other protection
    // It's better to do it the other way - on init 'push' AprImpl into
    // the native library, and have native code call instance methods.
    
    public static Object createJavaContext(String type, long cContext) {
        // XXX will be an instance method, fields accessible directly
        AprImpl apr=aprSingleton;
        JkHandler jkH=(JkHandler)apr.jkHandlers.get( type );
        if( jkH==null ) return null;

        MsgContext ep=jkH.createMsgContext();

        ep.setSource( jkH );
        
        ep.setJniContext( cContext );
        return ep;
    }

    /** Return a buffer associated with the ctx.
     */
    public static byte[] getBuffer( Object ctx, int id ) {
        return ((MsgContext)ctx).getBuffer(  id );
    }

    public static int jniInvoke( long jContext, Object ctx ) {
        try {
            MsgContext ep=(MsgContext)ctx;
            ep.setJniEnv(  jContext );
            ep.setType( 0 );
            return ((MsgContext)ctx).execute();
        } catch( Throwable ex ) {
            ex.printStackTrace();
            return -1;
        }
    }

    // -------------------- Initialization -------------------- 

    public void init() throws IOException {
        try {
            loadNative();

            initialize();
        } catch( Throwable t ) {
            throw new IOException( t.getMessage() );
        }
        ok=true;
    }

    public boolean isLoaded() {
        return ok;
    }
    
    /** This method of loading the libs doesn't require setting
     *   LD_LIBRARY_PATH. Assuming a 'right' binary distribution,
     *   or a correct build all files will be in their right place.
     *
     *  The burden is on our code to deal with platform specific
     *  extensions and to keep the paths consistent - not easy, but
     *  worth it if it avoids one extra step for the user.
     *
     *  Of course, this can change to System.load() and putting the
     *  libs in LD_LIBRARY_PATH.
     */
    public void loadNative() throws Throwable {
        if( aprHome==null )
            aprHome=baseDir;
        if( aprHome==null ) {
            // Use load()
            try {
                System.loadLibrary( "apr" );
                System.loadLibrary( "jkjni" );
            } catch( Throwable ex ) {
                ok=false;
                throw ex;
            }
        } else {
            File dir=new File(aprHome);
            // XXX platform independent, etc...
            File apr=new File( dir, "libapr." + soExt );
            
            loadNative( apr.getAbsolutePath() );
            
            dir=new File(baseDir);
            File jniConnect=new File( dir, "jni_connect." + soExt );
            
            loadNative( jniConnect.getAbsolutePath() );
        }
    }
    
    
    public void loadNative(String libPath) {
        try {
            System.load( libPath );
        } catch( Throwable ex ) {
            ok=false;
            ex.printStackTrace();
        }
    }
}
