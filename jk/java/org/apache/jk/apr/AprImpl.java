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

    static boolean ok=true;
    // Handlers for native callbacks
    Hashtable jkHandlers=new Hashtable();

    // Name of the so used in inprocess mode 
    String jniModeSo="inprocess";
    // name of the so used by java. If not set we'll loadLibrary("jkjni" ),
    // if set we load( nativeSo )
    String nativeSo;
    
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
    
    /** Name of the so used in inprocess mode
     */
    public void setJniModeSo(String jniModeSo ) {
        this.jniModeSo=jniModeSo;
    }

    /** name of the so used by java. If not set we'll loadLibrary("jkjni" ),
        if set we load( nativeSo )
    */
    public void setNativeSo( String nativeSo ) {
        this.nativeSo=nativeSo;
    }

    // -------------------- Apr generic utils --------------------
    /** Initialize APR
     */
    public native int initialize();

    public native int terminate();

    /* -------------------- Access to the jk_env_t -------------------- */

    /* The jk_env_t provide temporary storage ( pool ), logging, common services
     */
    
    /* Return a jk_env_t, used to keep the execution context ( temp pool, etc )
     */
    public native long getJkEnv();

    /** Clean the temp pool, put back the env in the pool
     */
    public native void releaseJkEnv(long xEnv);

    /* -------------------- Interface to the jk_bean object -------------------- */
    /* Each jk component is 'wrapped' as a bean, with a specified lifecycle
     *
     */
    
    /** Get a native component
     *  @return 0 if the component is not found.
     */
    public native long getJkHandler(long xEnv, String compName );

    public native long createJkHandler(long xEnv, String compName );

    public native int jkSetAttribute( long xEnv, long componentP, String name, String val );

    public native String jkGetAttribute( long xEnv, long componentP, String name );
    
    public native int jkInit( long xEnv, long componentP );

    public native int jkDestroy( long xEnv, long componentP );
    
    /** Send the packet to the C side. On return it contains the response
     *  or indication there is no response. Asymetrical because we can't
     *  do things like continuations.
     */
    public static native int jkInvoke(long xEnv, long componentP, long endpointP,
                                      int code, byte data[], int off, int len, int raw);

    /** Recycle an endpoint after use.
     */
    public native void jkRecycle(long xEnv, long endpointP);

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
            jkSetAttribute(0, 0, "channel:jni", "starting");
        } catch( Throwable t ) {
            throw new IOException( t.getMessage() );
        }
        ok=true;
    }

    public boolean isLoaded() {
        return ok;
    }

    static boolean jniMode=false;

    
    public static void jniMode() {
        jniMode=true;
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

        // XXX Update for windows
        if( jniMode ) {
            /* In JNI mode we use mod_jk for the native functions.
               This seems the cleanest solution that works with multiple
               VMs.
            */
            if (jniModeSo.equals("inprocess")) {
                ok=true;
                return;                                
            }
            try {
                System.out.println("Loading " + jniModeSo);
                if( jniModeSo!= null ) System.load( jniModeSo );
            } catch( Throwable ex ) {
                // ignore
                ex.printStackTrace();
                return;
            }
            ok=true;
            return;
        }
        
            /*
              jkjni _must_ be linked with apr and crypt -
              this seem the only ( decent ) way to support JDK1.4 and
              JDK1.3 at the same time
              try {
                  System.loadLibrary( "crypt" );
              } catch( Throwable ex ) {
                  // ignore
                  ex.printStackTrace();
              }
              try {
                  System.loadLibrary( "apr" );
              } catch( Throwable ex ) {
                  System.out.println("can't load apr, that's fine");
                  ex.printStackTrace();
              }
            */
        try {
            if( nativeSo == null ) {
                // This will load libjkjni.so or jkjni.dll in LD_LIBRARY_PATH
                System.loadLibrary( "jkjni" );
            } else {
                System.load( nativeSo );
            }
        } catch( Throwable ex ) {
            ok=false;
            throw ex;
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
