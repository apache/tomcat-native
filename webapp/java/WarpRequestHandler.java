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

public class WarpRequestHandler {

    /* ==================================================================== */
    /* Constructor                                                          */
    /* ==================================================================== */

    public WarpRequestHandler() {
        super();
    }

    public boolean handle(WarpConnection connection, WarpPacket packet)
    throws IOException {
        WarpLogger logger=new WarpLogger(this);
        WarpConnector connector=connection.getConnector();
        logger.setContainer(connector.getContainer());
        WarpRequest request=new WarpRequest();
        WarpResponse response=new WarpResponse();
        response.setRequest(request);
        response.setConnection(connection);
        response.setPacket(packet);

        // Prepare the Proceed packet
        packet.reset();
        packet.setType(Constants.TYPE_CONF_PROCEED);
        connection.send(packet);

        // Loop for configuration packets
        while (true) {
            connection.recv(packet);

            switch (packet.getType()) {
                case Constants.TYPE_REQ_INIT: {
                    int id=packet.readInteger();
                    String meth=packet.readString();
                    String ruri=packet.readString();
                    String args=packet.readString();
                    String prot=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request ID="+id+" \""+meth+" "+ruri+
                                     "?"+args+" "+prot+"\"");

                    request.recycle();
                    response.recycle();
                    response.setRequest(request);
                    response.setConnection(connection);
                    response.setPacket(packet);

                    request.setMethod(meth);
                    request.setRequestURI(ruri);
                    request.setQueryString(args);
                    request.setProtocol(prot);
                    Context ctx=connector.applicationContext(id);
                    if (ctx!=null) {
                        request.setContext(ctx);
                        request.setContextPath(ctx.getPath());
                        request.setHost((Host)ctx.getParent());
                    }
                    break;
                }

                case Constants.TYPE_REQ_CONTENT: {
                    String ctyp=packet.readString();
                    int clen=packet.readInteger();
                    if (Constants.DEBUG)
                        logger.debug("Request content type="+ctyp+" length="+
                                     clen);
                    if (ctyp.length()>0) request.setContentType(ctyp);
                    if (clen>0) request.setContentLength(clen);
                    break;
                }

                case Constants.TYPE_REQ_SCHEME: {
                    String schm=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request scheme="+schm);
                    request.setScheme(schm);
                    break;
                }

                case Constants.TYPE_REQ_AUTH: {
                    String user=packet.readString();
                    String auth=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request user="+user+" auth="+auth);
                    request.setAuthType(auth);
                    // What to do for user name?
                    break;
                }

                case Constants.TYPE_REQ_HEADER: {
                    String hnam=packet.readString();
                    String hval=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request header "+hnam+": "+hval);
                    request.addHeader(hnam,hval);
                    break;
                }

                case Constants.TYPE_REQ_SERVER: {
                    String host=packet.readString();
                    String addr=packet.readString();
                    int port=packet.readUnsignedShort();
                    if (Constants.DEBUG)
                        logger.debug("Server detail "+host+":"+port+
                                     " ("+addr+")");
                    request.setServerName(host);
                    request.setServerPort(port);
                    break;
                }

                case Constants.TYPE_REQ_CLIENT: {
                    String host=packet.readString();
                    String addr=packet.readString();
                    int port=packet.readUnsignedShort();
                    if (Constants.DEBUG)
                        logger.debug("Client detail "+host+":"+port+
                                     " ("+addr+")");
                    request.setRemoteHost(host);
                    request.setRemoteAddr(addr);
                    break;
                }

                case Constants.TYPE_REQ_PROCEED: {
                    if (Constants.DEBUG)
                        logger.debug("Request is about to be processed");
                    try {
                        connector.getContainer().invoke(request,response);
                    } catch (Exception e) {
                        logger.log(e);
                    }
                    request.finishRequest();
                    response.finishResponse();
                    if (Constants.DEBUG)
                        logger.debug("Request has been processed");
                    break;
                }

                default: {
                    String msg="Invalid packet "+packet.getType();
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
}
