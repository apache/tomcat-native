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
public class WarpRequestHandler extends WarpHandler {

    /** The WarpReader associated with this WarpConnectionHandler. */
    private WarpReader reader=new WarpReader();
    /** The WarpPacket used to write data. */
    private WarpPacket packet=new WarpPacket();
    /** Wether we had an error in the request header. */
    private boolean headererr=false;

    /** The WarpRequest object associated with this request handler. */
    protected WarpRequest request=null;
    /** The WarpRequest object associated with this request handler. */
    protected WarpResponse response=null;    

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
        String arg1=null;
        String arg2=null;
        int valu=-1;

        this.reader.reset(buffer);
        this.packet.reset();
        try {
            switch (type) {
                // The Request method
                case WarpConstants.TYP_REQINIT_MET:
                    arg1=reader.readString();
                    if (DEBUG) this.debug("Request Method "+arg1);
                    this.request.setMethod(arg1);
                    break;

                // The Request URI
                case WarpConstants.TYP_REQINIT_URI:
                    arg1=reader.readString();
                    if (DEBUG) this.debug("Request URI "+arg1);
                    this.request.setRequestURI(arg1);
                    break;

                // The Request query arguments
                case WarpConstants.TYP_REQINIT_ARG:
                    arg1=reader.readString();
                    if (DEBUG) this.debug("Request Query Argument "+arg1);
                    this.request.setQueryString(arg1);
                    break;

                // The Request protocol
                case WarpConstants.TYP_REQINIT_PRO:
                    arg1=reader.readString();
                    if (DEBUG) this.debug("Request Protocol "+arg1);
                    this.request.setProtocol(arg1);
                    break;

                // A request header
                case WarpConstants.TYP_REQINIT_HDR:
                    arg1=reader.readString();
                    arg2=reader.readString();
                    if (DEBUG) this.debug("Request Header "+arg1+": "+arg2);
                    this.request.addHeader(arg1,arg2);
                    break;

                // A request variable
                case WarpConstants.TYP_REQINIT_VAR:
                    valu=reader.readShort();
                    arg1=reader.readString();
                    if (DEBUG) this.debug("Request Variable ["+valu+"]="+arg1);
                    break;

                // The request header is finished, run the servlet (whohoo!)
                case WarpConstants.TYP_REQINIT_RUN:
                    if (DEBUG) this.debug("Invoking request");
                    // Check if we can accept (or not) this request
                    this.packet.reset();
                    this.send(WarpConstants.TYP_REQINIT_ACK,this.packet);
                    WarpInputStream win=new WarpInputStream(this);
                    WarpOutputStream wout=new WarpOutputStream(this,response);
                    this.request.setStream(win);
                    this.response.setStream(wout);
                    this.response.setRequest(this.request);
                    try {
                        engine.invoke(this.request, this.response);
                    } catch (Exception e) {
                        this.log(e);
                    }
                    try {
                        this.response.flushBuffer();
                        this.response.finishResponse();
                        wout.flush();
                        wout.close();
                    } catch (Exception e) {
                        if (DEBUG) this.debug(e);
                    }
                    this.packet.reset();
                    this.packet.writeString("End of request");
                    this.send(WarpConstants.TYP_REQUEST_ACK,this.packet);
                    if (DEBUG) this.debug("End of request");
                    return(false);

                // Other packet types
                default:
                    this.log("Wrong packet type "+type);
                    return(true);
            }
            return(true);
        } catch (IOException e) {
            this.log(e);
            return(false);
        }
    }
}
