package org.apache.jk.apr;

import java.io.*;
import java.util.*;

/** Implements the interface with the APR library. This is for internal-use
 *  only. The goal is to use 'natural' mappings for user code - for example
 *  java.net.Socket for unix-domain sockets, etc. 
 * 
 */
public class AprImpl {
    static AprImpl aprSingleton=new AprImpl();

    String baseDir;
    String aprHome;
    String soExt="so";

    public static AprImpl getAprImpl() {
        return aprSingleton;
    }
    
    /** Initialize APR
     */
    public native int initialize();

    public native int terminate();

    public native long poolCreate(long parentPool);

    public native long poolClear(long pool);

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
    public void loadNative() {
        if( aprHome==null )
            aprHome=baseDir;
        if( aprHome==null ) {
            // Use load()
            try {
                System.loadLibrary( "apr" );
                System.loadLibrary( "jkjni" );
            } catch( Throwable ex ) {
                ok=false;
                ex.printStackTrace();
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

    boolean ok=true;
    
    public void loadNative(String libPath) {
        try {
            System.load( libPath );
        } catch( Throwable ex ) {
            ok=false;
            ex.printStackTrace();
        }
    }

    // Mostly experimental, the interfaces need to be cleaned up - after everything works
    
    Hashtable jniContextFactories=new Hashtable();
    
    public void addJniContextFactory(String type, JniContextFactory cb) {
        jniContextFactories.put( type, cb );
    }

    public static interface JniContext {

        /** Each context contains a number of byte[] buffers used for communication.
         *  The C side will contain a char * equivalent - both buffers are long-lived
         *  and recycled.
         *
         *  This will be called at init time. A long-lived global reference to the byte[]
         *  will be stored in the C context.
         */
        public byte[] getBuffer(  int id );


        /** Invoke a java hook. The xEnv is the representation of the current execution
         *  environment ( the jni_env_t * )
         */
        public int jniInvoke(  long xEnv );
    }
    
    public static interface JniContextFactory {

        /** Create a Jni context - it is the corespondent of a C context, represented
         *    by a pointer
         *
         *  The 'context' is a long-lived object ( recycled ) that manages state for
         *  each jk operation.
         */
        public JniContext createJniContext( String type, long cContext );
    }

    
    // -------------------- Called from C --------------------

    public static Object createJavaContext(String type, long cContext) {
        JniContextFactory cb=(JniContextFactory)AprImpl.getAprImpl().jniContextFactories.get( type );
        if( cb==null ) return null;

        return cb.createJniContext( type, cContext );
    }

    /** Return a buffer associated with the ctx.
     */
    public static byte[] getBuffer( Object ctx, int id ) {
        return ((JniContext)ctx).getBuffer(  id );
    }

    public static int jniInvoke( long jContext, Object ctx ) {
        return ((JniContext)ctx).jniInvoke(  jContext );
   }

    // --------------------  java to C --------------------

    // Temp - interface will evolve
    
    /** Send the packet to the C side. On return it contains the response
     *  or indication there is no response. Asymetrical because we can't
     *  do things like continuations.
     */
    public static native int sendPacket(long xEnv, long endpointP,
                                        byte data[], int len);

}
