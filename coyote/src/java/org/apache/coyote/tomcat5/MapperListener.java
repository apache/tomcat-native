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
package org.apache.coyote.tomcat5;

import java.util.Iterator;
import java.util.Set;

import javax.management.MBeanServer;
import javax.management.MBeanServerNotification;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

import org.apache.commons.modeler.Registry;

import org.apache.tomcat.util.http.mapper.Mapper;

import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Engine;
import org.apache.catalina.Host;
import org.apache.catalina.ServerFactory;
import org.apache.catalina.Wrapper;

/**
 * Mapper listener.
 *
 * @author Remy Maucherat
 */
public class MapperListener
    implements NotificationListener {


    // ----------------------------------------------------- Instance Variables


    /**
     * Associated mapper.
     */
    protected Mapper mapper = null;


    /**
     * MBean server.
     */
    protected MBeanServer mBeanServer = null;


    // ----------------------------------------------------------- Constructors


    /**
     * Create mapper listener.
     */
    public MapperListener(Mapper mapper) {
        this.mapper = mapper;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Initialize associated mapper.
     */
    public void init() {

        try {

            mBeanServer = Registry.getServer();

            // FIXME
            registerHost(null);

            // Query contexts
            String onStr = "*:j2eeType=WebModule,*";
            ObjectName objectName = new ObjectName(onStr);
            Set set = mBeanServer.queryMBeans(objectName, null);
            Iterator iterator = set.iterator();
            while (iterator.hasNext()) {
                ObjectInstance oi = (ObjectInstance) iterator.next();
                registerContext(oi.getObjectName());
            }

            // Query wrappers
            onStr = "*:j2eeType=Servlet,*";
            objectName = new ObjectName(onStr);
            set = mBeanServer.queryMBeans(objectName, null);
            iterator = set.iterator();
            while (iterator.hasNext()) {
                ObjectInstance oi = (ObjectInstance) iterator.next();
                registerWrapper(oi.getObjectName());
            }

            onStr = "JMImplementation:type=MBeanServerDelegate";
            objectName = new ObjectName(onStr);
            mBeanServer.addNotificationListener(objectName, this, null, null);

        } catch (Exception e) {
            e.printStackTrace();
        }

    }


    // ------------------------------------------- NotificationListener Methods


    public void handleNotification(Notification notification,
                                   java.lang.Object handback) {

        if (notification instanceof MBeanServerNotification) {
            ObjectName objectName = 
                ((MBeanServerNotification) notification).getMBeanName();
            if (notification.getType().equals
                (MBeanServerNotification.REGISTRATION_NOTIFICATION)) {
                String j2eeType = objectName.getKeyProperty("j2eeType");
                if (j2eeType != null) {
                    if (j2eeType.equals("WebModule")) {
                        try {
                            registerContext(objectName);
                        } catch (Throwable t) {
                            t.printStackTrace();
                        }
                    } else if (j2eeType.equals("Servlet")) {
                        try {
                            registerWrapper(objectName);
                        } catch (Throwable t) {
                            t.printStackTrace();
                        }
                    }
                }
            } else if (notification.getType().equals
                       (MBeanServerNotification.UNREGISTRATION_NOTIFICATION)) {
                String j2eeType = objectName.getKeyProperty("j2eeType");
                if (j2eeType != null) {
                    if (j2eeType.equals("WebModule")) {
                        try {
                            unregisterContext(objectName);
                        } catch (Throwable t) {
                            t.printStackTrace();
                        }
                    }
                }
            }
        }

    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Register host (FIXME).
     */
    private void registerHost(ObjectName objectName)
        throws Exception {

        Container container = 
            ServerFactory.getServer().findServices()[0].getContainer();

        Container[] hosts = null;
        String defaultHost = null;

        if (container instanceof Engine) {
            defaultHost = ((Engine) container).getDefaultHost();
            hosts = container.findChildren();
        } else if (container instanceof Host) {
            defaultHost = container.getName();
            hosts = new Container[1];
            hosts[0] = container;
        }

        mapper.setDefaultHostName(defaultHost);
        for (int i = 0; i < hosts.length; i++) {
            mapper.addHost(hosts[i].getName(), hosts[i]);
        }

    }


    /**
     * Unregister host (FIXME.
     */
    private void unregisterHost(ObjectName objectName)
        throws Exception {



    }


    /**
     * Register context.
     */
    private void registerContext(ObjectName objectName)
        throws Exception {

        String name = objectName.getKeyProperty("name");

        String hostName = null;
        String contextName = null;
        if (name.startsWith("//")) {
            name = name.substring(2);
        }
        int slash = name.indexOf("/");
        if (slash != -1) {
            hostName = name.substring(0, slash);
            contextName = name.substring(slash);
        } else {
            return;
        }
        // Special case for the root context
        if (contextName.equals("/")) {
            contextName = "";
        }

        Object context = 
            mBeanServer.getAttribute(objectName, "mappingObject");
        javax.naming.Context resources = (javax.naming.Context)
            mBeanServer.getAttribute(objectName, "staticResources");
        String[] welcomeFiles = (String[])
            mBeanServer.getAttribute(objectName, "welcomeFiles");

        mapper.addContext(hostName, contextName, context, 
                          welcomeFiles, resources);

    }


    /**
     * Unregister context.
     */
    private void unregisterContext(ObjectName objectName)
        throws Exception {

        String name = objectName.getKeyProperty("name");

        String hostName = null;
        String contextName = null;
        if (name.startsWith("//")) {
            name = name.substring(2);
        }
        int slash = name.indexOf("/");
        if (slash != -1) {
            hostName = name.substring(0, slash);
            contextName = name.substring(slash);
        } else {
            return;
        }
        // Special case for the root context
        if (contextName.equals("/")) {
            contextName = "";
        }

        mapper.removeContext(hostName, contextName);

    }


    /**
     * Register wrapper.
     */
    private void registerWrapper(ObjectName objectName)
        throws Exception {
    
        String wrapperName = objectName.getKeyProperty("name");
        String name = objectName.getKeyProperty("WebModule");

        String hostName = null;
        String contextName = null;
        if (name.startsWith("//")) {
            name = name.substring(2);
        }
        int slash = name.indexOf("/");
        if (slash != -1) {
            hostName = name.substring(0, slash);
            contextName = name.substring(slash);
        } else {
            return;
        }
        // Special case for the root context
        if (contextName.equals("/")) {
            contextName = "";
        }

        String[] mappings = (String[])
            mBeanServer.invoke(objectName, "findMappings", null, null);
        Object wrapper = 
            mBeanServer.invoke(objectName, "findMappingObject", null, null);

        for (int i = 0; i < mappings.length; i++) {
            mapper.addWrapper(hostName, contextName, mappings[i], wrapper);
        }

    }


}
