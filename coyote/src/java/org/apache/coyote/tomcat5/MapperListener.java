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

            MBeanServer mBeanServer = Registry.getServer();

            // Query contexts
            String onStr = "*:j2eeType=WebModule,*";
            ObjectName objectName = new ObjectName(onStr);
            Set set = mBeanServer.queryMBeans(objectName, null);
            Iterator iterator = set.iterator();
            while (iterator.hasNext()) {
                ObjectInstance oi = (ObjectInstance) iterator.next();
                String name = oi.getObjectName().getKeyProperty("name");
            }

            // Query wrappers
            onStr = "*:j2eeType=Servlet,*";
            objectName = new ObjectName(onStr);
            set = mBeanServer.queryMBeans(objectName, null);
            iterator = set.iterator();
            while (iterator.hasNext()) {
                ObjectInstance oi = (ObjectInstance) iterator.next();
                String name = oi.getObjectName().getKeyProperty("name");
                String contextName = 
                    oi.getObjectName().getKeyProperty("WebModule");
                registerWrapper(contextName, name);
            }

            //mBeanServer.addNotificationListener(objectName, this, null, null);

        } catch (Exception e) {
            e.printStackTrace();
        }

    }


    // ------------------------------------------- NotificationListener Methods


    public void handleNotification(Notification notification,
                                   java.lang.Object handback) {



    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Find context.
     */
    private void registerWrapper(String name, String wrapperName) {

        String defaultHost = null;
        Host host = null;
        Context context = null;
        Wrapper wrapper = null;

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

        Container container = 
            ServerFactory.getServer().findServices()[0].getContainer();

        if (container instanceof Engine) {
            defaultHost = ((Engine) container).getDefaultHost();
            host = (Host) container.findChild(hostName);
        } else if (container instanceof Host) {
            defaultHost = container.getName();
            host = (Host) container;
        }

        context = (Context) host.findChild(contextName);
        wrapper = (Wrapper) context.findChild(wrapperName);
        String[] mappings = wrapper.findMappings();

        mapper.setDefaultHostName(defaultHost);
        mapper.addHost(hostName, host);
        mapper.addContext(hostName, contextName, context, 
                          context.findWelcomeFiles(), context.getResources());
        for (int i = 0; i < mappings.length; i++) {
            mapper.addWrapper(hostName, contextName, mappings[i], wrapper);
        }

    }


}
