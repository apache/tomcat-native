/*
 * Copyright 1999,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package org.apache.tomcat.util.loader;


import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;


/**
 * A group of modules and libraries. 
 * 
 * Modules can have one or more jars and class dirs. Classloaders are created 
 * from modules when the module is started are be disposed when the module is stopped.
 * 
 * The module will delegate to the associated repository in addition to the 
 * normal delegation rules. The repository will search on all sibling modules.
 * This mechanism is defined in the MLetClassLoader and is also used by JBoss and
 * few other servers. 
 * 
 * TODO: explain more ( or point to the right jboss/mlet pages )
 * TODO: explain how this can be used for webapps to support better partitioning 
 *
 * @author Costin Manolache
 */
public class Repository {
   
    private static final boolean DEBUG=true; //LoaderProperties.getProperty("loader.Repository.debug") != null;
    
    // Allows the (experimental) use of jar indexes
    // Right now ( for small set of jars, incomplete build ) it's a tiny 3.5 -> 3.4 sec dif.
    private static final boolean USE_IDX=true;// //LoaderProperties.getProperty("loader.Repository.useIdx") != null;
    
    private Vector loaders=new Vector();
    private String name;
    private Vector grpModules=new Vector();
    private transient Loader loader;
    
    private Repository parent;

    private transient ModuleClassLoader groupClassLoader;
    private Hashtable prefixes=new Hashtable();

    public Repository() {
    }

    public Repository(Loader loader) {
        if( loader== null ) throw new NullPointerException();
        this.loader=loader;
    }

    public Loader getLoader() {
        return loader;
    }
    
    public void addModule(  Module mod ) {
        mod.setRepository( this );

        grpModules.addElement(mod);
        if( loader.listener!=null ) {
            loader.listener.moduleAdd(mod);
        }

        if(! mod.isStarted()) {
            mod.start();
            //log("started " + mod);
        } else {
            //log("already started " + mod);
        }
        
        try {
            if( USE_IDX ) {
                processJarIndex(mod);
                writeCacheIdx();
            }
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        
    }
    
    public Enumeration getModules() {
        return grpModules.elements();
    }
    
    public Repository getParent() {
        return parent;
    }

    /**
     * 
     * @param parent The parent group
     */
    public void setParent(Repository parent) {
        this.parent = parent;
    }

    /** Add a class loder to the group.
     *
     *  If this is a StandardClassLoader instance, it will be able to delegate
     * to the group.
     *
     *  If it's a regular ClassLoader - it'll be searched for classes, but
     * it will not be able to delegate to peers.
     *
     * In future we may fine tune this by using manifests.
     */
    void addClassLoader(ClassLoader cl ) {
        if( ( cl instanceof ModuleClassLoader )) {
            ((ModuleClassLoader)cl).setRepository(this);
        }
        loaders.addElement(cl);
        //    log("Adding classloader " + cl);
    }

    public String getName() {
        return name;
    }

    public void removeClassLoader(ClassLoader cl) {
        int oldSize=loaders.size();
        loaders.removeElement(cl);

        if(DEBUG) log("removed " + loaders.size() + "/" + oldSize + ": "  + cl);
    }

    /** Return a class loader associated with the group.
     *  This will delegate to all modules in the group, then to parent.
     * 
     * @return
     */
    public ClassLoader getClassLoader() {
        if( groupClassLoader==null ) {
            if( parent == null ) {
                groupClassLoader=new ModuleClassLoader(new URL[0]);
            } else {
                groupClassLoader=new ModuleClassLoader(new URL[0], parent.getClassLoader());
            }
            groupClassLoader.start();
            groupClassLoader.setRepository(this);
        }
        return groupClassLoader;
    }
    /** 
     * Find a class in the group. It'll iterate over each loader
     * and try to find the class - using only the method that
     * search locally or on parent ( i.e. not in group, to avoid
     * recursivity ).
     *
     *
     * @param classN
     * @return
     */
    public Class findClass(ClassLoader caller, String classN ) {
        Class clazz=null;
        
        // do we have it in index ?
        if( USE_IDX ) {
        int lastIdx=classN.lastIndexOf(".");
        String prefix=(lastIdx>0) ? classN.substring(0, lastIdx) : classN;
        Object mO=prefixes.get(prefix.replace('.', '/'));
        if( mO!=null ) {
            if( mO instanceof Module ) {
                Module m=(Module)mO;
                try {
                    Class c=((ModuleClassLoader)m.getClassLoader()).findLocalClass(classN);
                    //log("Prefix: " +prefix + " " + classN  + " " + m);
                    return c;
                } catch (Exception e) {
                    //log("Prefix err: " +prefix + " " + classN  + " " + m + " " + e);
                    //return null;
                }
            } else {
                Module mA[]=(Module[])mO;
                for( int i=0; i<mA.length; i++ ) {
                    Module m=mA[i];
                    try {
                        Class c=((ModuleClassLoader)m.getClassLoader()).findLocalClass(classN);
                        //log("Prefix: " +prefix + " " + classN  + " " + m);
                        return c;
                    } catch (Exception e) {
                        //log("Prefix err: " +prefix + " " + classN  + " " + m + " " + e);
                        //return null;
                    }
                }
            }
        }
        }

        // TODO: move the vector to a []
        for( int i=loaders.size()-1; i>=0; i-- ) {
            
            // TODO: for regular CL, just use loadClass, they'll not recurse
            // The behavior for non-SCL or not in the group loader is the same as for parent loader
            ModuleClassLoader cl=(ModuleClassLoader)loaders.elementAt(i);
            // TODO: move loaders with index in separate vector
            //if( cl.getModule().hasIndex ) continue;
            if( cl== caller ) continue;
            //if( classN.indexOf("SmtpCoyoteProtocolHandler") > 0 ) {
            //log("try " + cl.debugObj + " " + name + " " + classN + " " + loaders.size());
            //}
            try {
                if( cl instanceof ModuleClassLoader ) {
                    clazz=((ModuleClassLoader)cl).findLocalClass(classN );
                } else {
                    clazz=cl.findClass(classN);
                }

                //System.err.println("GRPLD: " + classN + " from " + info.get(cl));
                return clazz;
            } catch (ClassNotFoundException e) {
                //System.err.println("CNF: " + classN + " " + info.get(cl) );
                //if( classN.indexOf("smtp") > 0 ) e.printStackTrace();
            }
        }
        return null;
    }
    
    /**
     * @param loader
     * @param name2
     * @return
     */
    public URL findResource(ModuleClassLoader caller, String classN) {
        URL url=null;
        
        for( int i=loaders.size()-1; i>=0; i-- ) {
            // TODO: for regular CL, just use loadClass, they'll not recurse
            // The behavior for non-SCL or not in the group loader is the same as for parent loader
            ModuleClassLoader cl=(ModuleClassLoader)loaders.elementAt(i);
            if( cl== caller ) continue;
            url=((ModuleClassLoader)cl).findResource(classN );
            if( url!=null )
                return url;
        }
        return null;
    }

    private void log(String s) {
        System.err.println("Repository (" + name + "): " + s );
    }

    /**
     * @param name2
     */
    public void setName(String name2) {
        this.name=name2;
    }

    /*
     * Work in progress: 
     * 
     * -use the INDEX.LIST to get prefixes to avoid linear
     * search in repositories.
     * 
     * - serialize the state ( including timestamps ) to improve startup time
     * ( avoids the need to open all jars - if INDEX.LIST is ok)
     */
    
    /**
     * Read the index. The index contain packages and top level resources
     * 
     * @param cl
     * @throws Exception
     */
    private void processJarIndex(Module m) throws Exception {
        ModuleClassLoader cl=(ModuleClassLoader)m.createClassLoader();
        // only support index for modules with a single jar in CP
        String cp=m.getClasspathString();
        if( ! cp.endsWith(".jar")) return;
        URL urlIdx=cl.findResource("META-INF/INDEX.LIST");
        if( urlIdx == null ) {
            log("INDEX.LIST not found, run: jar -i " + m.getClasspathString());
            return;
        }
        try {
            InputStream is=urlIdx.openStream();
            if( is==null ) {
                log("Can't read " + urlIdx + " " + m.getClasspathString());
                return;
            }
            BufferedReader br=new BufferedReader( new InputStreamReader(is) );
            String line=br.readLine();
            if( line==null ) return;
            if( ! line.startsWith( "JarIndex-Version:") ||  
                    ! line.endsWith("1.0")) {
                log("Invalid signature " + line + " " + m.getClasspathString());
            }
            br.readLine(); // ""
            
            while( readSection(br, m) ) {
            }
           
            m.hasIndex=true;
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }
    
    private boolean readSection( BufferedReader br, Module m) throws IOException {
        String jarName=br.readLine();
        if( jarName==null ) return false; // done
        if( "".equals( jarName )) {
            log("Invalid jarName " + jarName + " " + m.getClasspathString() );
            return false;
        }
        //log("Index " + jarName + " " + m.getClasspathString());
        String prefix=null;
        while( ((prefix=br.readLine()) != null ) && 
                (! "".equals( prefix )) ) {
            //log("found " + prefix + " " + m);
            Object o=prefixes.get(prefix);
            if( o == null ) {
                prefixes.put(prefix, m);
            } else {
                Module mA[]=null;
                if( o instanceof Module ) {
                    mA=new Module[2];
                    mA[0]=(Module)o;
                    mA[1]=m;
                } else {
                    Object oldA[]=(Module[])o;
                    mA=new Module[oldA.length + 1];
                    System.arraycopy(oldA, 0, mA, 0, oldA.length);
                    mA[oldA.length]=m;
                }
                prefixes.put( prefix, mA);
                //log("Multiple prefixes: " + prefix + " " + mA);
                
            }
        }
        
        return prefix!=null;
    }


    /** Read loader.REPO.cache from work dir
     * 
     * This file will hold timestamps for each module/jar and cache the INDEX -
     * to avoid opening the jars/modules that are not used 
     * 
     * @throws IOException
     */
    private void readCachedIdx() throws IOException {
        
    }
    
    /** Check the index and verify that:
     * - all jars are older than timestamp and still exist
     * - there are no new jars 
     * 
     * @throws IOException
     */
    private void checkCacheIdx() throws IOException {
        
    }
    
    
    private void writeCacheIdx() throws IOException {
        
    }
}
