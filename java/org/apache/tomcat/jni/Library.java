/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package org.apache.tomcat.jni;

import java.io.File;

public final class Library {

    /* Default library names */
    private static final String [] NAMES = {"tcnative-2", "libtcnative-2", "tcnative-1", "libtcnative-1"};
    /*
     * A handle to the unique Library singleton instance.
     */
    private static Library _instance = null;

    private Library() throws Exception {
        boolean loaded = false;
        StringBuilder err = new StringBuilder();
        File binLib = new File(System.getProperty("catalina.home"), "bin");
        for (int i = 0; i < NAMES.length; i++) {
            File library = new File(binLib, System.mapLibraryName(NAMES[i]));
            try {
                System.load(library.getAbsolutePath());
                loaded = true;
            } catch (ThreadDeath | VirtualMachineError t) {
                throw t;
            } catch (Throwable t) {
                if (library.exists()) {
                    // File exists but failed to load
                    throw t;
                }
                if (i > 0) {
                    err.append(", ");
                }
                err.append(t.getMessage());
            }
            if (loaded) {
                break;
            }
        }
        if (!loaded) {
            String path = System.getProperty("java.library.path");
            String [] paths = path.split(File.pathSeparator);
            for (String value : NAMES) {
                try {
                    System.loadLibrary(value);
                    loaded = true;
                } catch (ThreadDeath | VirtualMachineError t) {
                    throw t;
                } catch (Throwable t) {
                    String name = System.mapLibraryName(value);
                    for (String s : paths) {
                        File fd = new File(s, name);
                        if (fd.exists()) {
                            // File exists but failed to load
                            throw t;
                        }
                    }
                    if (err.length() > 0) {
                        err.append(", ");
                    }
                    err.append(t.getMessage());
                }
                if (loaded) {
                    break;
                }
            }
        }
        if (!loaded) {
            StringBuilder names = new StringBuilder();
            for (String name : NAMES) {
                names.append(name);
                names.append(", ");
            }
            throw new LibraryNotFoundError(names.substring(0, names.length() -2), err.toString());
        }
    }

    private Library(String libraryName)
    {
        System.loadLibrary(libraryName);
    }

    /* create global TCN's APR pool
     * This has to be the first call to TCN library.
     */
    private static native boolean initialize();
    /* destroy global TCN's APR pool
     * This has to be the last call to TCN library.
     */
    public static native void terminate();
    /* Internal function for loading APR Features */
    private static native boolean has(int what);
    /* Internal function for loading APR Features */
    private static native int version(int what);
    /* Internal function for loading APR sizes */
    private static native int size(int what);

    /* TCN_MAJOR_VERSION */
    public static int TCN_MAJOR_VERSION  = 0;
    /* TCN_MINOR_VERSION */
    public static int TCN_MINOR_VERSION  = 0;
    /* TCN_PATCH_VERSION */
    public static int TCN_PATCH_VERSION  = 0;
    /* TCN_IS_DEV_VERSION */
    public static int TCN_IS_DEV_VERSION = 0;
    /* APR_MAJOR_VERSION */
    public static int APR_MAJOR_VERSION  = 0;
    /* APR_MINOR_VERSION */
    public static int APR_MINOR_VERSION  = 0;
    /* APR_PATCH_VERSION */
    public static int APR_PATCH_VERSION  = 0;
    /* APR_IS_DEV_VERSION */
    public static int APR_IS_DEV_VERSION = 0;

    /* TCN_VERSION_STRING */
    public static native String versionString();
    /* APR_VERSION_STRING */
    public static native String aprVersionString();

    /*  APR Feature Macros */
    @Deprecated
    public static boolean APR_HAVE_IPV6           = false;
    @Deprecated
    public static boolean APR_HAS_SHARED_MEMORY   = false;
    @Deprecated
    public static boolean APR_HAS_THREADS         = false;
    @Deprecated
    public static boolean APR_HAS_SENDFILE        = false;
    @Deprecated
    public static boolean APR_HAS_MMAP            = false;
    @Deprecated
    public static boolean APR_HAS_FORK            = false;
    @Deprecated
    public static boolean APR_HAS_RANDOM          = false;
    @Deprecated
    public static boolean APR_HAS_OTHER_CHILD     = false;
    @Deprecated
    public static boolean APR_HAS_DSO             = false;
    @Deprecated
    public static boolean APR_HAS_SO_ACCEPTFILTER = false;
    @Deprecated
    public static boolean APR_HAS_UNICODE_FS      = false;
    @Deprecated
    public static boolean APR_HAS_PROC_INVOKED    = false;
    @Deprecated
    public static boolean APR_HAS_USER            = false;
    @Deprecated
    public static boolean APR_HAS_LARGE_FILES     = false;
    @Deprecated
    public static boolean APR_HAS_XTHREAD_FILES   = false;
    @Deprecated
    public static boolean APR_HAS_OS_UUID         = false;
    /* Are we big endian? */
    @Deprecated
    public static boolean APR_IS_BIGENDIAN        = false;
    /* APR sets APR_FILES_AS_SOCKETS to 1 on systems where it is possible
     * to poll on files/pipes.
     */
    @Deprecated
    public static boolean APR_FILES_AS_SOCKETS    = false;
    /* This macro indicates whether or not EBCDIC is the native character set.
     */
    @Deprecated
    public static boolean APR_CHARSET_EBCDIC      = false;
    /* Is the TCP_NODELAY socket option inherited from listening sockets?
     */
    @Deprecated
    public static boolean APR_TCP_NODELAY_INHERITED = false;
    /* Is the O_NONBLOCK flag inherited from listening sockets?
     */
    @Deprecated
    public static boolean APR_O_NONBLOCK_INHERITED  = false;
    /* Poll operations are interruptable by apr_pollset_wakeup().
     */
    @Deprecated
    public static boolean APR_POLLSET_WAKEABLE      = false;
    /* Support for Unix Domain Sockets.
     */
    @Deprecated
    public static boolean APR_HAVE_UNIX             = false;


    @Deprecated
    public static int APR_SIZEOF_VOIDP;
    @Deprecated
    public static int APR_PATH_MAX;
    @Deprecated
    public static int APRMAXHOSTLEN;
    @Deprecated
    public static int APR_MAX_IOVEC_SIZE;
    @Deprecated
    public static int APR_MAX_SECS_TO_LINGER;
    @Deprecated
    public static int APR_MMAP_THRESHOLD;
    @Deprecated
    public static int APR_MMAP_LIMIT;

    /* return global TCN's APR pool */
    @Deprecated
    public static native long globalPool();

    /**
     * Setup any APR internal data structures.  This MUST be the first function
     * called for any APR library.
     * @param libraryName the name of the library to load
     *
     * @return {@code true} if the native code was initialized successfully
     *         otherwise {@code false}
     *
     * @throws Exception if a problem occurred during initialization
     */
    public static synchronized boolean initialize(String libraryName) throws Exception {
        if (_instance == null) {
            if (libraryName == null) {
                _instance = new Library();
            } else {
                _instance = new Library(libraryName);
            }
            TCN_MAJOR_VERSION  = version(0x01);
            TCN_MINOR_VERSION  = version(0x02);
            TCN_PATCH_VERSION  = version(0x03);
            TCN_IS_DEV_VERSION = version(0x04);
            APR_MAJOR_VERSION  = version(0x11);
            APR_MINOR_VERSION  = version(0x12);
            APR_PATCH_VERSION  = version(0x13);
            APR_IS_DEV_VERSION = version(0x14);

            APR_SIZEOF_VOIDP        = size(1);
            APR_PATH_MAX            = size(2);
            APRMAXHOSTLEN           = size(3);
            APR_MAX_IOVEC_SIZE      = size(4);
            APR_MAX_SECS_TO_LINGER  = size(5);
            APR_MMAP_THRESHOLD      = size(6);
            APR_MMAP_LIMIT          = size(7);

            APR_HAVE_IPV6           = has(0);
            APR_HAS_SHARED_MEMORY   = has(1);
            APR_HAS_THREADS         = has(2);
            APR_HAS_SENDFILE        = has(3);
            APR_HAS_MMAP            = has(4);
            APR_HAS_FORK            = has(5);
            APR_HAS_RANDOM          = has(6);
            APR_HAS_OTHER_CHILD     = has(7);
            APR_HAS_DSO             = has(8);
            APR_HAS_SO_ACCEPTFILTER = has(9);
            APR_HAS_UNICODE_FS      = has(10);
            APR_HAS_PROC_INVOKED    = has(11);
            APR_HAS_USER            = has(12);
            APR_HAS_LARGE_FILES     = has(13);
            APR_HAS_XTHREAD_FILES   = has(14);
            APR_HAS_OS_UUID         = has(15);
            APR_IS_BIGENDIAN        = has(16);
            APR_FILES_AS_SOCKETS    = has(17);
            APR_CHARSET_EBCDIC      = has(18);
            APR_TCP_NODELAY_INHERITED = has(19);
            APR_O_NONBLOCK_INHERITED  = has(20);
            APR_POLLSET_WAKEABLE      = has(21);
            APR_HAVE_UNIX             = has(22);
            if (APR_MAJOR_VERSION < 1) {
                throw new UnsatisfiedLinkError("Unsupported APR Version (" +
                                               aprVersionString() + ")");
            }
            if (!APR_HAS_THREADS) {
                throw new UnsatisfiedLinkError("Missing threading support from APR");
            }
        }
        return initialize();
    }

    /**
     * Calls System.load(filename). System.load() associates the
     * loaded library with the class loader of the class that called
     * the System method. A native library may not be loaded by more
     * than one class loader, so calling the System method from a class that
     * was loaded by a Webapp class loader will make it impossible for
     * other Webapps to load it.
     *
     * Using this method will load the native library via a shared class
     * loader (typically the Common class loader, but may vary in some
     * configurations), so that it can be loaded by multiple Webapps.
     *
     * @param filename - absolute path of the native library
     *
     * @deprecated Unused. Will be removed in Tomcat 10.1.x
     */
    @Deprecated
    public static void load(String filename){
        System.load(filename);
    }

    /**
     * Calls System.loadLibrary(libname). System.loadLibrary() associates the
     * loaded library with the class loader of the class that called
     * the System method. A native library may not be loaded by more
     * than one class loader, so calling the System method from a class that
     * was loaded by a Webapp class loader will make it impossible for
     * other Webapps to load it.
     *
     * Using this method will load the native library via a shared class
     * loader (typically the Common class loader, but may vary in some
     * configurations), so that it can be loaded by multiple Webapps.
     *
     * @param libname - the name of the native library
     *
     * @deprecated Unused. Will be removed in Tomcat 10.1.x
     */
    @Deprecated
    public static void loadLibrary(String libname){
        System.loadLibrary(libname);
    }

}
