package org.apache.jk.apr;

import java.io.*;

/** Implements the interface with the APR library. This is for internal-use
 *  only. The goal is to use 'natural' mappings for user code - for example
 *  java.net.Socket for unix-domain sockets, etc. 
 * 
 */
public class AprImpl {
    String baseDir;
    String aprHome;
    String soExt="so";
    
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
    
    
}
