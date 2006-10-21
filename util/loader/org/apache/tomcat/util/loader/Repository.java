/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
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
   
    private static final boolean DEBUG=Loader.getProperty("loader.debug.Repository") != null;
    
    // Allows the (experimental) use of jar indexes
    // Right now ( for small set of jars, incomplete build ) it's a tiny 3.5 -> 3.4 sec dif.
    private static final boolean USE_IDX=Loader.getProperty("loader.Repository.noIndex") == null;
    
    private Vector loaders=new Vector();
    private String name;
    private Vector grpModules=new Vector();
    private transient Loader loader;
    
    private transient RepositoryClassLoader groupClassLoader;
    private Hashtable prefixes=new Hashtable();

    // For delegation
    private ClassLoader parentClassLoader;
    private Repository parent;


    private Repository() {
    }

    public Repository(Loader loader) {
        if( loader== null ) throw new NullPointerException();
        this.loader=loader;
    }

    public Loader getLoader() {
        return loader;
    }
    
    void addModule(  Module mod ) {
        mod.setRepository( this );

        grpModules.addElement(mod);
        if( loader.listener!=null ) {
            loader.listener.moduleAdd(mod);
        }
        
        if( parentClassLoader != null ) 
            mod.setParentClassLoader( parentClassLoader );

        if(! mod.isStarted()) {
            mod.start();
            //log("started " + mod);
        } else {
            //log("already started " + mod);
        }
        
        try {
            if( USE_IDX ) {
                processJarIndex(mod);
                // TODO: if we are in the initial starting, write cache only once
                // TODO: write it only if there is a change in the timestamp
                writeCacheIdx();
            }
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        
    }
    
    public void newModule( String path ) {
        Module m=new Module();
        m.setPath( path );
        addModule( m );
    }
    
    public Enumeration getModules() {
        return grpModules.elements();
    }
    
    /** Reload any module that is modified
     */
    public void checkReload() {
        try {
        Enumeration mE=grpModules.elements();
        while( mE.hasMoreElements() ) {
            Module m=(Module)mE.nextElement();
            boolean modif=m.modified();
            log("Modified " + m + " " + modif);
            
            if( modif ) {
                m.stop();
                m.start();
            }
        }
        } catch( Throwable t ) {
            t.printStackTrace();
        }
    }

    /** Verify if any module is modified. This is a deep search, including dirs.
     *  Expensive operation.
     *  
     * @return
     */
    public boolean isModified() {
        try {
            Enumeration mE=grpModules.elements();
            while( mE.hasMoreElements() ) {
                Module m=(Module)mE.nextElement();
                boolean modif=m.modified();
                log("Modified " + m + " " + modif);
                if( modif ) return true;
            }
        } catch( Throwable t ) {
            t.printStackTrace();
        }
        return false;
    }
    
    Repository getParent() {
        return parent;
    }
    
    public String toString() {
        return "Repository " + name + "(" + getClasspathString() + ")";
    }

    private String getClasspathString() {
        StringBuffer sb=new StringBuffer();
        Enumeration mE=grpModules.elements();
        while( mE.hasMoreElements() ) {
            Module m=(Module)mE.nextElement();
            sb.append( m.getClasspathString() + ":");
        }
        return sb.toString();
    }

    /**
     * 
     * @param parent The parent group
     */
    public void setParent(Repository parent) {
        this.parent = parent;
    }
    
    /** Set the parent class loader - can be used instead of setParent, 
     * in case this is the top loader and needs to delagate to embedding app
     * 
     * @param myL
     */
    public void setParentClassLoader(ClassLoader myL) {
        this.parentClassLoader=myL;
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
        // TODO: remove from index
    }

    /** Return a class loader associated with the group.
     *  This will delegate to all modules in the group, then to parent.
     * 
     * @return
     */
    public ClassLoader getClassLoader() {
        if( groupClassLoader==null ) {
            
            ClassLoader pcl=parentClassLoader;
            if( pcl==null && parent!=null ) {
                pcl=parent.getClassLoader();
            } 
            if( pcl==null ) {
                pcl=Thread.currentThread().getContextClassLoader();
             }

            if( pcl == null ) {
                // allow delegation to embedding app
                groupClassLoader=new RepositoryClassLoader(new URL[0], this);
            } else {
                groupClassLoader=new RepositoryClassLoader(new URL[0], pcl, this);
            }
            if( DEBUG ) log("---------- Created repository loader " + pcl );
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
    Class findClass(ClassLoader caller, String classN ) {
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
    URL findResource(ModuleClassLoader caller, String classN) {
        URL url=null;
        if( DEBUG ) log("Repository.findResource " + classN + " " + caller );
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
        // For each module we write the timestamp, filename then the index
        // The idea is to load this single file to avoid scanning many jars

        // we'll use the cache 
        
        
    }

}
