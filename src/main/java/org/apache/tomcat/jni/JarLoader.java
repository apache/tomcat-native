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
import java.io.InputStream;
import java.io.FileOutputStream;

import java.util.Enumeration;
import java.util.Locale;

import java.security.CodeSource;

import java.util.jar.JarFile;
import java.util.jar.JarEntry;

/** JarLoader
 */
public final class JarLoader {
    private static final String [] fileswin = { "libapr-1.dll", "libeay32.dll", "ssleay32.dll", "libtcnative-1.dll" };
    private static final String [] fileslin = { "libapr-1.so.0", "libcrypto.so.1", "libssl.so.1" , "libtcnative-1.so" };
    private static final String [] filessol = { "libapr-1.so.0", "libcrypto.so.10", "libssl.so.10" , "libtcnative-1.so" };
    private static final String [] filesmac = { "libapr-1.0.dylib", "libcrypto.1.0.0.dylib", "libssl.1.0.0.dylib" , "libtcnative-1.0.dylib" };

    public static String [] getLibraries() throws Exception {
        String oS = System.getProperty("os.name");
        String [] files = null;
        String ending = null;

        /* find the platform we are using */   
        if (oS != null) {
            oS = oS.toUpperCase(Locale.US); 

            if (oS.startsWith("WIN")) {
                files = fileswin;
                ending = ".dll";
            } else if (oS.startsWith("LINUX")) {
                files = fileslin;
                ending = ".so";
            } else if (oS.startsWith("MAC")) {
                files = filesmac;
                ending = ".dylib";
            } else if (oS.startsWith("SOLARIS") || oS.startsWith("SUNOS")) {
                files = filessol;
                ending = ".so";
            } else {
                return null;
            }
        }

        String aprdir = null;
        ClassLoader classLoader = JarLoader.class.getClassLoader();
        Class aClass = classLoader.loadClass("org.apache.tomcat.jni.JarLoader");
        CodeSource codeSource = aClass.getProtectionDomain().getCodeSource();
        JarFile jarFile = new JarFile(codeSource.getLocation().getFile());
        Enumeration enumOfJar = jarFile.entries();   
        while (enumOfJar.hasMoreElements()) {
            String dir = ((JarEntry) enumOfJar.nextElement()).getName();
            if (dir.indexOf("libapr") > 0) {
                /* so we have something like Linux-x86_64/lib/libapr-1.so.0 */
                aprdir = dir;
                int i = aprdir.lastIndexOf('/');
                aprdir = aprdir.substring(0, i+1);
                break;
            }
        }
        if (aprdir == null) {
            /* can't find libapr the jar is broken */
            return null;
        }

        /* find the file names: basically the files can be versioned */
        jarFile = new JarFile(codeSource.getLocation().getFile());
        enumOfJar = jarFile.entries();   
        while (enumOfJar.hasMoreElements()) {
            String dir = ((JarEntry) enumOfJar.nextElement()).getName();
           
            if (dir.startsWith(aprdir)) {
                String file =  dir;
                int i = file.lastIndexOf('/');
                if (i+1 == aprdir.length()) {
                    file = file.substring(i+1);
                }
                /* Check and replace the file */
                for(int j = 0; j <= files.length - 1; j++) {
                    if (file.indexOf(files[j]) >= 0) {
                        files[j] = file;
                    }
                }
            }
        } 
     
        File temp = File.createTempFile("tmp-", "openssl");
        temp.delete();
        temp.mkdir();
        for(int j = 0; j <= files.length - 1; j++) {
            String complete = aprdir + files[j];
            final InputStream resource = aClass.getClassLoader().getResourceAsStream(complete);
            if (resource != null) {
                File result = new File(temp, complete);
                result.getParentFile().mkdirs();
                FileOutputStream out = new FileOutputStream(result);
                byte[] buf = new byte[1000];
                int r;
                while ((r = resource.read(buf)) > 0) {
                    out.write(buf, 0, r);
                }
                result.setExecutable(true);
                result.deleteOnExit();
                files[j] = result.getAbsolutePath();
            }
        }
        temp.deleteOnExit();
        return files;
    }
}
