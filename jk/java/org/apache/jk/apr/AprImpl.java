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

    public native long shmDetach( long pool, long shmP );

    public native long shmDestroy( long pool, long shmP );

    // --------------------  java to C --------------------

    // Temp - interface will evolve
    
    /** Send the packet to the C side. On return it contains the response
     *  or indication there is no response. Asymetrical because we can't
     *  do things like continuations.
     */
    public static native int sendPacket(long xEnv, long endpointP,
                                        byte data[], int len);

    
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
            log.error("Native code not initialized, disabling UnixSocket and JNI channels: " + t.toString());
            return;
        }
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

    // Hack for Catalina who hungs the calling thread.
    public static void main( String args[] ) {
        System.err.println("Main");
        try {
            // Find the class
            Class c=null;
            int i=0;

            for( i=0; i<args.length; i++ ) {
                String classN=args[i];
                if( "-".equals( classN ) ) {
                    // end of options.
                    break;
                }
                if( c!=null ) continue;
                try {
                    System.err.println("Try " + classN);
                    c=Class.forName( classN );
                } catch( ClassNotFoundException ex  ) {
                    continue;
                }
            }

            i++;
            if( c==null ) {
                System.err.println("No class found ");
                return;
            }
            
            if( args.length >= i ) {
                String newArgs[]=new String[ args.length - i  ];
                System.out.println("Replacing args: " + i + " " + args.length);
                for( int j=0; j<newArgs.length; j++ ) {
                    newArgs[j]=args[i+j];
                    System.out.println("ARG: " + newArgs[j]);
                }
                args=newArgs;
            } else {
                System.out.println("No extra args: " + i + " " + args.length);
                args=new String[0];
            }
                
                System.err.println("Starting");
                Thread startThread=new Thread( new TomcatStartThread(c, args));
                startThread.start();
        } catch (Throwable t ) {
            t.printStackTrace(System.err);
        }
    }

    static class TomcatStartThread implements Runnable {
        Class c;
        String args[];
        TomcatStartThread( Class c, String args[] ) {
            this.c=c;
            this.args=args;
        }
        
        public void run() {
            try {
                Class argClass=args.getClass();
                Method m=c.getMethod( "main", new Class[] {argClass} );
                m.invoke( c, new Object[] { args } );
            } catch( Throwable t ) {
                t.printStackTrace(System.err);
            }
        }
    }
    
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( AprImpl.class );

}
