/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */
package org.apache.catalina.connector.warp;

import java.io.File;
import java.io.IOException;
import java.net.URL;

import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Deployer;
import org.apache.catalina.Host;
import org.apache.catalina.core.StandardHost;
import org.apache.catalina.core.StandardContext;

public class WarpConfigurationHandler {

    /* ==================================================================== */
    /* Constructor                                                          */
    /* ==================================================================== */

    public WarpConfigurationHandler() {
        super();
    }

    public boolean handle(WarpConnection connection, WarpPacket packet)
    throws IOException {
        WarpLogger logger=new WarpLogger(this);
        logger.setContainer(connection.getConnector().getContainer());

        // Prepare the Welcome packet
        packet.setType(Constants.TYPE_CONF_WELCOME);
        packet.writeUnsignedShort(Constants.VERS_MAJOR);
        packet.writeUnsignedShort(Constants.VERS_MINOR);
        packet.writeInteger(connection.getConnector().uniqueId);
        connection.send(packet);

        // Loop for configuration packets
        while (true) {        
            connection.recv(packet);
            
            switch (packet.getType()) {

                case Constants.TYPE_CONF_DEPLOY: {
                    String appl=packet.readString();
                    String host=packet.readString();
                    int port=packet.readUnsignedShort();
                    String path=packet.readString();
                    Context context=null;
                    packet.reset();

                    if (Constants.DEBUG) 
                        logger.log("Deploying web application \""+appl+"\" "+
                                   "under <http://"+host+":"+port+path+">");
                    try {
                        context=deploy(connection,logger,appl,host,path);
                    } catch (Exception e) {
                        logger.log(e);
                    }
                    if (context==null) {
                        String msg="Error deploying web application \""+appl+
                                   "\" under <http://"+host+":"+port+path+">";
                        logger.log(msg);
                        packet.setType(Constants.TYPE_ERROR);
                        packet.writeString(msg);
                        connection.send(packet);
                    } else {
                        int k=connection.getConnector().applicationId(context);
                        packet.setType(Constants.TYPE_CONF_APPLIC);
                        packet.writeInteger(k);
                        packet.writeString(context.getDocBase());
                        connection.send(packet);
                        if (Constants.DEBUG)
                            logger.debug("Application \""+appl+"\" deployed "+
                                         "under <http://"+host+":"+port+path+
                                         "> with root="+context.getDocBase()+
                                         " ID="+k);
                    }
                    break;
                }

                case Constants.TYPE_CONF_DONE: {
                    return(true);
                }

                case Constants.TYPE_DISCONNECT: {
                    return(false);
                }

                default: {
                    String msg="Invalid packet with type "+packet.getType();
                    logger.log(msg);
                    packet.reset();
                    packet.setType(Constants.TYPE_FATAL);
                    packet.writeString(msg);
                    connection.send(packet);
                    return(false);
                }
            }
        }        
    }
    
    /** Deploy a web application */
    private Context deploy(WarpConnection connection, WarpLogger logger,
                           String applName, String hostName, String applPath)
    throws IOException {
        synchronized (connection.getConnector()) {

        Container container=connection.getConnector().getContainer();

        Host host=(Host)container.findChild(hostName);
        if (host==null) {
            WarpHost whost=new WarpHost();
            whost.setName(hostName);
            whost.setParent(container);
            whost.setAppBase(connection.getConnector().getAppBase());
            whost.setDebug(connection.getConnector().getDebug());
            container.addChild(whost);
            host=whost;
            if (Constants.DEBUG)
                logger.debug("Created new host "+host.getName());
        } else if (Constants.DEBUG) {
            logger.debug("Reusing instance of Host \""+host.getName()+"\"");
        }

        // TODO: Set up mapping mechanism for performance
        
        if (applPath.endsWith("/"))
            applPath=applPath.substring(0,applPath.length()-1);

        Context appl=(Context)host.findChild(applPath);

        if (appl==null) {

            if (Constants.DEBUG)
                logger.debug("No application for \""+applPath+"\"");

            Deployer deployer=(Deployer)host;
            File file=new File(host.getAppBase()+File.separator+applName);
            if (!file.isAbsolute()) {
                file=new File(System.getProperty("catalina.home"),
                              host.getAppBase()+File.separator+applName);
            }

            if (!file.exists()) {
                logger.log("Cannot find \""+file.getPath()+"\" for appl. \""+
                           applName);
                return(null);
            }

            String path=file.getCanonicalPath();
            URL url=new URL("file",null,path);
            if (path.toLowerCase().endsWith(".war"))
                url=new URL("jar:"+url.toString()+"!/");

            if (Constants.DEBUG)
                logger.debug("Application URL \""+url.toString()+"\"");

            deployer.install(applPath, url);
            StandardContext context=null;
            context=(StandardContext)deployer.findDeployedApp(applPath);
            context.setDebug(connection.getConnector().getDebug());
            return(context);
        } else {
            if (Constants.DEBUG)
                logger.debug("Found application for \""+appl.getName()+"\"");
            return(appl);
        }
        }
    }
}
