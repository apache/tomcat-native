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

import org.apache.catalina.Container;
import org.apache.catalina.Host;
import org.apache.catalina.Request;
import org.apache.catalina.core.StandardEngine;

public class WarpEngine extends StandardEngine {
    public Container map(Request request, boolean update) {
        if (Constants.DEBUG) this.log("Mapping request");
        if (request instanceof WarpRequest) {
            WarpRequest wreq = (WarpRequest)request;
            Host host = wreq.getHost();
            if (update) {
                if (request.getRequest().getServerName() == null) {
                    request.setServerName(host.getName());
                } else {
                    request.setServerName
                        (request.getRequest().getServerName());
                }
            }
            return (host);
        } else {
            return (super.map(request,update));
        }
    }
}
