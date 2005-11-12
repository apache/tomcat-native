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
package org.apache.jk.status;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.tomcat.util.digester.Digester;

/**
 * <code>
 *  &lt;?xml version="1.0" encoding="UTF-8" ?&gt;
 *  &lt;jk:status xmlns:jk="http://jakarta.apache.org"&gt;
 *     &lt;jk:server name="localhost" port="80" 
 *                software="Apache/2.0.54 (Win32) mod_ssl/2.0.54 OpenSSL/0.9.7g mod_jk/1.2.13-dev" 
 *                version="1.2.12"/&gt;
 *
 *	  &lt;jk:balancers&gt;
 *
 *        &lt;jk:balancer id="0" name="lb" type="lb" 
 *                      sticky="True" stickyforce="False" retries="3" recover="60"&gt;
 *            &lt;jk:member id="0" name="node1" type="ajp13" host="localhost" port="9012" 
 *                       address="127.0.0.1:9012" status="OK" lbfactor="1" lbvalue="1"
 *                       elected="0" readed="0" transferred="0" errors="0" busy="0"/&gt;
 *            &lt;jk:member id="1" name="node2" type="ajp13" host="localhost" port="9022"
 *                       address="127.0.0.1:9022" status="OK" lbfactor="1" lbvalue="1" 
 *                       elected="0" readed="0" transferred="0" errors="0" busy="0"/&gt;
 *            &lt;jk:map type="Wildchar" uri="/ClusterTest/*" context="/ClusterTest/*"/&gt;
 *            &lt;jk:map type="Exact" uri="/ClusterTest" context="/ClusterTest"/&gt;
 *            &lt;jk:map type="Wildchar" uri="/myapps/*" context="/myapps/*"/&gt;
 *            &lt;jk:map type="Exact" uri="/myapps" context="/myapps"/&gt;
 *        &lt;/jk:balancer&gt;
 *     &lt;/jk:balancers&gt;
 *  &lt;/jk:status&gt;
 * </code>
 * @author Peter Rossbach
 * @version $Revision:$ $Date:$
 * @since 5.5.10
 */
public class JkStatusParser {
    private static Log log = LogFactory.getLog(JkStatusParser.class);

    /**
     * The descriptive information about this implementation.
     */
    protected static final String info = "org.apache.jk.status.JkStatusParser/1.0";

    /**
     * The <code>Digester</code> instance used to parse registry descriptors.
     */
    public static Digester digester = createDigester();

    public static Digester getDigester() {
        return digester;
    }

    /**
     * Create and configure the Digester we will be using for setup mod_jk jk status page.
     */
    public static Digester createDigester() {
        long t1 = System.currentTimeMillis();
        // Initialize the digester
        Digester digester = new Digester();
        digester.setValidating(false);
        digester.setClassLoader(JkStatus.class.getClassLoader());

        // parse status
        digester.addObjectCreate("jk:status", "org.apache.jk.status.JkStatus",
                "className");
        digester.addSetProperties("jk:status");

        digester.addObjectCreate("jk:status/jk:server",
                "org.apache.jk.status.JkServer", "className");
        digester.addSetProperties("jk:status/jk:server");
        digester.addSetNext("jk:status/jk:server", "setServer",
                "org.apache.jk.status.JkServer");

        digester.addObjectCreate("jk:status/jk:balancers/jk:balancer",
                "org.apache.jk.status.JkBalancer", "className");
        digester.addSetProperties("jk:status/jk:balancers/jk:balancer");
        digester.addSetNext("jk:status/jk:balancers/jk:balancer",
                "addBalancer", "org.apache.jk.status.JkBalancer");

        digester.addObjectCreate(
                "jk:status/jk:balancers/jk:balancer/jk:member",
                "org.apache.jk.status.JkBalancerMember", "className");
        digester
                .addSetProperties("jk:status/jk:balancers/jk:balancer/jk:member");
        digester.addSetNext("jk:status/jk:balancers/jk:balancer/jk:member",
                "addBalancerMember", "org.apache.jk.status.JkBalancerMember");

        digester.addObjectCreate("jk:status/jk:balancers/jk:balancer/jk:map",
                "org.apache.jk.status.JkBalancerMapping", "className");
        digester.addSetProperties("jk:status/jk:balancers/jk:balancer/jk:map");
        digester.addSetNext("jk:status/jk:balancers/jk:balancer/jk:map",
                "addBalancerMapping", "org.apache.jk.status.JkBalancerMapping");

        long t2 = System.currentTimeMillis();
        if (log.isDebugEnabled())
            log.debug("Digester for apache mod_jk jkstatus page is created "
                    + (t2 - t1));
        return (digester);

    }

}
