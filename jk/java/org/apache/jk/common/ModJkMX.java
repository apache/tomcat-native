/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */
package org.apache.jk.common;

import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URLConnection;
import java.net.URL;
import java.util.List;
import java.util.StringTokenizer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import javax.management.MBeanServer;
import javax.management.AttributeNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.Attribute;
import javax.management.ObjectName;

import org.apache.jk.core.JkHandler;
import org.apache.commons.modeler.Registry;
import org.apache.commons.modeler.BaseModelMBean;
import org.apache.commons.modeler.ManagedBean;
import org.apache.commons.modeler.AttributeInfo;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

/**
 * A small mbean that will act as a proxy for mod_jk2.
 *
 * For efficiency, it'll get bulk results and cache them - you
 * can force an update by calling the refreshAttributes and refreshMetadata
 * operations on this mbean.
 *
 * TODO: implement the user/pass auth ( right now you must use IP based security )
 * TODO: eventually support https
 * TODO: support for metadata ( mbean-descriptors ) for description and type conversions
 * TODO: filter out trivial components ( mutexes, etc )
 *
 * @author Costin Manolache
 */
public class ModJkMX extends JkHandler
{
    private static Log log = LogFactory.getLog(ModJkMX.class);

    MBeanServer mserver;
    String webServerHost="localhost";
    int webServerPort=80;
    String statusPath="/jkstatus";
    String user;
    String pass;
    Registry reg;

    HashMap mbeans=new HashMap();
    long lastRefresh=0;
    long updateInterval=5000; // 5 sec - it's min time between updates

    public ModJkMX()
    {
    }

    /* -------------------- Public methods -------------------- */

    public String getWebServerHost() {
        return webServerHost;
    }

    public void setWebServerHost(String webServerHost) {
        this.webServerHost = webServerHost;
    }

    public int getWebServerPort() {
        return webServerPort;
    }

    public void setWebServerPort(int webServerPort) {
        this.webServerPort = webServerPort;
    }

    public long getUpdateInterval() {
        return updateInterval;
    }

    public void setUpdateInterval(long updateInterval) {
        this.updateInterval = updateInterval;
    }

    public String getUser() {
        return user;
    }

    public void setUser(String user) {
        this.user = user;
    }

    public String getPass() {
        return pass;
    }

    public void setPass(String pass) {
        this.pass = pass;
    }

    public String getStatusPath() {
        return statusPath;
    }

    public void setStatusPath(String statusPath) {
        this.statusPath = statusPath;
    }
    /* ==================== Start/stop ==================== */

    public void destroy() {
        try {
            // We should keep track of loaded beans and call stop.
            // Modeler should do it...
            Iterator mbeansIt=mbeans.values().iterator();
            while( mbeansIt.hasNext()) {
                MBeanProxy proxy=(MBeanProxy)mbeansIt.next();
                ObjectName oname=proxy.getObjectName();
                Registry.getRegistry().getMBeanServer().unregisterMBean(oname);
            }
        } catch( Throwable t ) {
            log.error( "Destroy error", t );
        }
    }

    public void init() throws IOException {
        try {
            //if( log.isDebugEnabled() )
            log.info("init " + webServerHost + " " + webServerPort);
            reg=Registry.getRegistry();
            refreshMetadata();
            refreshAttributes();
        } catch( Throwable t ) {
            log.error( "Init error", t );
        }
    }

    public void start() throws IOException {
        if( reg==null)
            init();
    }

    /** Refresh the proxies, if updateInterval passed
     *
     */
    public void refresh()  {
        long time=System.currentTimeMillis();
        if( time - lastRefresh < updateInterval ) {
            return;
        }
        lastRefresh=time;
        refreshMetadata();
        refreshAttributes();
    }

    public void refreshAttributes()  {
        try {
            int cnt=0;
            // connect to apache, get a list of mbeans
            BufferedReader is=getStream( "dmp=*");
            if( is==null ) return;
            while(true) {
                String line=is.readLine();
                if( line==null ) break;
                // for each mbean, create a proxy
                if(log.isDebugEnabled())
                    log.debug("Read " + line);
                StringTokenizer st=new StringTokenizer(line,"|");
                if( st.countTokens() < 4 ) continue;
                String key=st.nextToken();
                if( ! "G".equals( key )) continue;
                String name=st.nextToken();
                String att=st.nextToken();
                String val=st.nextToken("").substring(1);
                if( log.isDebugEnabled())
                    log.debug("Token: " + key + " name: " + name + " att=" + att +
                        " val=" + val);
                MBeanProxy proxy=(MBeanProxy)mbeans.get(name);
                if( proxy==null ) {
                    log.info( "Unknown object " + name);
                } else {
                    proxy.update(att, val);
                    cnt++;
                }
            }
            log.info( "Refreshing attributes " + cnt);
        } catch( Exception ex ) {
            log.info("Error ", ex);
        }
    }

    /** connect to apache, get a list of mbeans
      */
    BufferedReader getStream(String qry) throws Exception {
        try {
            String path=statusPath + "?" + qry;
            URL url=new URL( "http", webServerHost, webServerPort, path);
            URLConnection urlc=url.openConnection();
            BufferedReader is=new BufferedReader(new InputStreamReader(urlc.getInputStream()));
            return is;
        } catch (IOException e) {
            log.info( "Can't connect to jkstatus " + webServerHost + ":" + webServerPort
            + " " + e.toString());
            return null;
        }
    }

    public void refreshMetadata() {
        try {
            int cnt=0;
            int newCnt=0;
            BufferedReader is=getStream("lst=*");
            if( is==null ) return;
            String current=null;
            ArrayList getters=new ArrayList();
            ArrayList setters=new ArrayList();
            StringTokenizer st=null;
            String key=null;
            while(true) {
                String line=is.readLine();
                if( log.isDebugEnabled())
                    log.debug("Read " + line);
                // for each mbean, create a proxy
                if( line!= null ) {
                    st=new StringTokenizer(line,"|");
                    if( st.countTokens() < 3 ) continue;
                    key=st.nextToken();
                }
                if( line==null ||  "N".equals( key )) {
                    if( current!=null ) {
                        // switched to a different object or end
                        cnt++;
                        if( mbeans.get( current ) ==null ) {
                            // New component
                            newCnt++;
                            MBeanProxy mproxy=new MBeanProxy(this);
                            mproxy.init( current, getters, setters);
                            mbeans.put( current, mproxy );
                        }

                        getters.clear();
                        setters.clear();
                    }
                    if( line==null ) break;
                    String type=st.nextToken();
                    String name=st.nextToken();
                    current=name;
                    if( log.isDebugEnabled())
                        log.debug("Token: " + key + " name: " + name + " type=" + type);
                }
                if( "G".equals( key )) {
                    String name=st.nextToken();
                    if( ! name.equals( current )) {
                        log.error("protocol error, name=" + name + " current=" + current);
                        break;
                    }
                    String att=st.nextToken();
                    getters.add(att);
                }
                if( "S".equals( key )) {
                    String name=st.nextToken();
                    if( ! name.equals( current )) {
                        log.error("protocol error, name=" + name + " current=" + current);
                        break;
                    }
                    String att=st.nextToken();
                    setters.add(att);
                }


            }
            log.info( "Refreshing metadata " + cnt + " " +  newCnt);
        } catch( Exception ex ) {
            log.info("Error ", ex);
        }
    }

    /** Use the same metadata, except that we replace the attribute
     * get/set methods.
     */
    static class MBeanProxy extends BaseModelMBean {
        private static Log log = LogFactory.getLog(MBeanProxy.class);

        String jkName;
        List getAttNames;
        List setAttNames;
        HashMap atts=new HashMap();
        ModJkMX jkmx;
        ObjectName oname;

        public MBeanProxy(ModJkMX jkmx) throws Exception {
            this.jkmx=jkmx;
        }

        public ObjectName getObjectName() {
            return oname;
        }

        void init( String name, List getters, List setters )
            throws Exception
        {
            if(log.isDebugEnabled())
                log.debug("Register " + name );
            int col=name.indexOf( ':' );
            this.jkName=name;
            String type=name.substring(0, col );
            String id=name.substring(col+1);
            id=id.replace('*','%');
            id=id.replace(':', '%');
            if( id.length() == 0 ) {
                id="default";
            }
            ManagedBean mbean= new ManagedBean();

            AttributeInfo ai=new AttributeInfo();
            ai.setName( "jkName" );
            ai.setType( "java.lang.String");
            ai.setWriteable(false);
            mbean.addAttribute(ai);

            for( int i=0; i<getters.size(); i++ ) {
                String att=(String)getters.get(i);
                // Register metadata
                ai=new AttributeInfo();
                ai.setName( att );
                ai.setType( "java.lang.String");
                if( ! setters.contains(att))
                    ai.setWriteable(false);
                mbean.addAttribute(ai);
            }
            for( int i=0; i<setters.size(); i++ ) {
                String att=(String)setters.get(i);
                if( getters.contains(att))
                    continue;
                // Register metadata
                ai=new AttributeInfo();
                ai.setName( att );
                ai.setType( "java.lang.String");
                ai.setReadable(false);
                mbean.addAttribute(ai);
            }

            this.setModelMBeanInfo(mbean.createMBeanInfo());

            MBeanServer mserver=Registry.getRegistry().getMBeanServer();
            oname=new ObjectName("apache:type=" + type + ",id=" + id);
            mserver.registerMBean(this, oname);
        }

        private void update( String name, String val ) {
            log.debug( "Updating " + jkName + " " + name + " " + val);
            atts.put( name, val);
        }

        public Object getAttribute(String name)
            throws AttributeNotFoundException, MBeanException,
                ReflectionException {
            if( "jkName".equals( name )) {
                return jkName;
            }
            jkmx.refresh();
            return atts.get(name);
        }

        public void setAttribute(Attribute attribute)
            throws AttributeNotFoundException, MBeanException,
            ReflectionException
        {
            try {
                // we support only string values
                String val=(String)attribute.getValue();
                String name=attribute.getName();
                BufferedReader is=jkmx.getStream("set=" + jkName + "|" +
                        name + "|" + val);
                if( is==null ) return;
                if( log.isDebugEnabled())
                    log.debug( "Setting " + jkName + " " + name + " result " + is.readLine());
                jkmx.refreshMetadata();
                jkmx.refreshAttributes();
            } catch( Exception ex ) {
                throw new MBeanException(ex);
            }
        }
    }
}

