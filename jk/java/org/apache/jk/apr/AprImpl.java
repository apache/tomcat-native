package org.apache.jk.apr;

/** Implements the interface with the APR library. This is for internal-use
 *  only. The goal is to use 'natural' mappings for user code - for example
 *  java.net.Socket for unix-domain sockets, etc. 
 * 
 */
public class AprImpl {

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

    public void loadNative(String libPath) {
        try {
            if( libPath==null )
                libPath="jni_connect";
            // XXX use load() for full path
            if( libPath.indexOf( "/" ) >=0 ||
                libPath.indexOf( "\\" ) >=0 ) {
                System.load( libPath );
            } else {
                System.loadLibrary( libPath );
            }
        } catch( RuntimeException ex ) {
            ex.printStackTrace();
        }
    }
    
    
}
