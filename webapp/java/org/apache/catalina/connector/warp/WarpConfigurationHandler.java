/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.catalina.connector.warp;

import java.io.File;
import java.io.IOException;
import java.net.URL;

import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Deployer;
import org.apache.catalina.Host;
import org.apache.catalina.Wrapper;
import org.apache.catalina.core.StandardContext;
import org.apache.catalina.deploy.FilterMap;
import org.apache.catalina.deploy.SecurityCollection;
import org.apache.catalina.deploy.SecurityConstraint;

public class WarpConfigurationHandler {

    private static final String DEFAULT_SERVLET = 
        "org.apache.catalina.servlets.DefaultServlet";

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
                        break;
                    }
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
                    break;
                }

                case Constants.TYPE_CONF_MAP: {
                    int id=packet.readInteger();
                    Context context=connection.getConnector()
                                    .applicationContext(id);

                    if (context==null) {
                        String msg="Invalid application ID for mappings "+id;
                        logger.log(msg);
                        packet.reset();
                        packet.setType(Constants.TYPE_ERROR);
                        packet.writeString(msg);
                        connection.send(packet);
                        break;
                    }

                    String smap[]=context.findServletMappings();
                    if (smap!=null) {
                        for (int x=0; x<smap.length; x++) {
                            Container c=context.findChild(
                                context.findServletMapping(smap[x]));
                            packet.reset();
                            packet.setType(Constants.TYPE_CONF_MAP_DENY);
                            packet.writeString(smap[x]);

                            if (c instanceof Wrapper) {
                                String servlet=((Wrapper)c).getServletClass();
                                if (DEFAULT_SERVLET.equals(servlet)) {
                                    packet.setType(
                                        Constants.TYPE_CONF_MAP_ALLOW);
                                    if (Constants.DEBUG)
                                        logger.debug("Servlet mapping \""+
                                                     smap[x]+"\" allowed");
                                } else if (Constants.DEBUG) {
                                    logger.debug("Servlet mapping \""+smap[x]+
                                                 "\" denied");
                                }
                            }
                            connection.send(packet);
                        }
                    }

                    FilterMap fmap[]=context.findFilterMaps();
                    if (fmap!=null) {
                        logger.log("Filter mappings ("+fmap.length+")");
                        for (int x=0; x<fmap.length; x++) {
                            String map=fmap[x].getURLPattern();
                            if (map!=null) {
                                if (Constants.DEBUG)
                                    logger.debug("Filter mapping \""+map+
                                                 "\" denied");
                                packet.reset();
                                packet.setType(Constants.TYPE_CONF_MAP_DENY);
                                packet.writeString(map);
                                connection.send(packet);
                            }
                        }
                    }

                    SecurityConstraint scon[]=context.findConstraints();
                    if (scon!=null) {
                        for (int x=0; x<scon.length; x++) {
                            SecurityCollection col[]=scon[x].findCollections();
                            if (col!=null) {
                                for (int y=0; y<col.length; y++) {
                                    String patt[]=col[y].findPatterns();
                                    if (patt!=null) {
                                        for (int q=0; q<patt.length; q++) {
                                            packet.reset();
                                            packet.setType(
                                                Constants.TYPE_CONF_MAP_DENY);
                                            packet.writeString(patt[q]);
                                            connection.send(packet);
                                            if (Constants.DEBUG) {
                                                logger.debug("Seurity "+
                                                    " mapping \""+patt[q]+
                                                    "\"");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    packet.reset();
                    packet.setType(Constants.TYPE_CONF_MAP_DONE);
                    connection.send(packet);
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

        // the hostName should be in lowewr case (setName makes a toLowerCase).
        Host host=(Host)container.findChild(hostName.toLowerCase());
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
            File file=new File(applName);
            if (!file.isAbsolute()) {
                file=new File(host.getAppBase()+File.separator+applName);
                if (!file.isAbsolute()) {
               	    file=new File(System.getProperty("catalina.base"),
                                  host.getAppBase()+File.separator+applName);
                }
            }

            if (!file.exists()) {
                logger.log("Cannot find \""+file.getPath()+"\" for appl. \""+
                           applName+"\" host \""+host.getName()+"\"");
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
