/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
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

import java.io.IOException;

/**
 *
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public class WarpConnectionHandler extends WarpHandler {
    /** The WarpReader associated with this WarpConnectionHandler. */
    private WarpReader reader=new WarpReader();
    /** The WarpPacket used to write data. */
    private WarpPacket packet=new WarpPacket();
    /** The current request id. */
    private int request=WarpConstants.RID_MIN;

    /**
     * Process a WARP packet.
     * <br>
     * This method is the one which will actually perform the operation of
     * analyzing the packet and doing whatever needs to be done.
     * <br>
     * This method will return true if another packet is expected for this
     * RID, or it will return false if this was the last packet for this RID.
     * When we return false this handler is unregistered, and the Thread
     * started in the init() method is terminated.
     *
     * @param type The WARP packet type.
     * @param buffer The WARP packet payload.
     * @return If more packets are expected for this RID, true is returned,
     *         false if this was the last packet.
     */
    public boolean process(int type, byte buffer[]) {
        WarpConnector connector=this.getConnector();
        WarpEngine engine=(WarpEngine)connector.getContainer();

        this.reader.reset(buffer);
        this.packet.reset();
        try {
            switch (type) {
                case WarpConstants.TYP_CONINIT_HST: {
                    String name=reader.readString()+"."+reader.readShort();
                    
                    // Retrieve this host id
                    WarpHost host=engine.setupChild(name);
                    if (host==null) {
                        this.log("Cannot retrieve host instance");
                        this.send(WarpConstants.TYP_CONINIT_ERR,this.packet);
                        return(false);
                    }

                    int hid=host.getHostID();
                    if (DEBUG) this.debug("Host "+name+" has ID="+hid);

                    // Send the HOST ID back to the WARP client
                    this.packet.reset();
                    this.packet.writeShort(hid);
                    this.send(WarpConstants.TYP_CONINIT_HID,this.packet);
                    break;
                }

                case WarpConstants.TYP_CONINIT_APP: {
                    int hid=reader.readShort();
                    String name=reader.readString();
                    String path=reader.readString();

                    // Retrieve this host (based on the host ID)
                    WarpHost host=(WarpHost)engine.findChild(hid);
                    if (host==null) {
                        this.log("Host ID "+hid+" not found");
                        this.packet.reset();
                        this.send(WarpConstants.TYP_CONINIT_ERR,this.packet);
                        return(false);
                    }

                    // Retrieve this application (based on the path)
                    WarpContext cont=(WarpContext)host.findChild('/'+name);
                    if (cont==null) {
                        this.log("Application "+name+" with path "+path+
                                 " not found");
                        this.packet.reset();
                        this.send(WarpConstants.TYP_CONINIT_ERR,this.packet);
                        return(false);
                    }

                    // Ok, we found the right application
                    int aid=cont.getApplicationID();
                    cont.setPath('/'+name);
                    cont.setDisplayName(name);
                    if (DEBUG) this.debug("Application "+name+" mapped in "+
                                          host.getName()+path+" has ID "+aid);

                    // Send the APPLICATION ID back to the WARP client
                    this.packet.reset();
                    this.packet.writeShort(aid);
                    this.send(WarpConstants.TYP_CONINIT_AID,this.packet);
                    break;
                }

                case WarpConstants.TYP_CONINIT_REQ: {
                    // Get the Host and Application IDs
                    int hid=reader.readShort();
                    int aid=reader.readShort();
                    if (DEBUG) this.debug("Request for HID="+hid+" AID="+aid);

                    // Create a new WarpRequestHandler and register it with
                    // an unique RID.
                    int rid=this.request;
                    WarpConnection c=this.getConnection();
                    WarpRequestHandler h=new WarpRequestHandler();

                    // TODO: Iterate until a valid RID is found
                    c.registerHandler(h,rid);
                    this.request=rid+1;
                    h.setConnection(c);
                    h.setRequestID(rid);
                    
                    // Send the RID back to the WARP client
                    this.packet.reset();
                    this.packet.writeShort(rid);
                    this.send(WarpConstants.TYP_CONINIT_RID,this.packet);
                    
                    // Create new Request and Response objects.
                    h.request=(WarpRequest)connector.createRequest();
                    h.response=(WarpResponse)connector.createResponse();
                    h.request.setRequestID(rid);
                    h.request.setRequestedHostID(hid);
                    h.request.setRequestedApplicationID(aid);
                    h.request.setWarpRequestHandler(h);
                    h.response.setWarpRequestHandler(h);

                    // Start the request handler
                    try {
                        h.start();
                    } catch (Exception e) {
                        this.log(e);
                        h.stop();
                    }
                    break;
                }

                default: {
                    this.log("Wrong packet type "+type+". Closing connection");
                    // Post an error message back to the WARP client
                    this.packet.reset();
                    this.send(WarpConstants.TYP_CONINIT_ERR,this.packet);
                    return(false);
                }
            }
            return(true);
        } catch (IOException e) {
            this.log(e);
            return(false);
        }
    }
}
