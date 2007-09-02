/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.jk.status;

import java.io.IOException;
import java.io.StringReader;

import junit.framework.TestCase;

import org.apache.tomcat.util.digester.Digester;
import org.xml.sax.SAXException;

/**
 * @author Peter Rossbach
 *  
 */
public class JkStatusParserTest extends TestCase {

	public void testDigester() throws IOException, SAXException {
		Digester digester = JkStatusParser.createDigester();
        String example = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
         +   "<jk:status xmlns:jk=\"http://tomcat.apache.org\">"
         +   "<jk:server name=\"localhost\" port=\"80\" software=\"Apache/2.0.58 (Unix) mod_jk/1.2.19\" version=\"1.2.19\" />"
         +   "<jk:balancers>"
         +   "<jk:balancer id=\"0\" name=\"loadbalancer\" type=\"lb\" sticky=\"True\" stickyforce=\"False\" retries=\"2\" recover=\"60\" >"
         +   "<jk:member id=\"0\" name=\"node1\" type=\"ajp13\" host=\"localhost\" port=\"9012\" address=\"127.0.0.1:9012\" activation=\"ACT\" state=\"OK/IDLE\" distance=\"0\" lbfactor=\"1\" lbmult=\"1\" lbvalue=\"0\" elected=\"0\" errors=\"0\" transferred=\"0\" readed=\"0\" busy=\"0\" maxbusy=\"0\" jvm_route=\"node1\" />"
         +   "<jk:member id=\"0\" name=\"node2\" type=\"ajp13\" host=\"localhost\" port=\"9022\" address=\"127.0.0.1:9022\" activation=\"ACT\" state=\"OK/IDLE\" distance=\"0\" lbfactor=\"1\" lbmult=\"1\" lbvalue=\"0\" elected=\"0\" errors=\"0\" transferred=\"0\" readed=\"0\" busy=\"0\" maxbusy=\"0\" jvm_route=\"node2\" />"
         +   "<jk:map type=\"Wildchar\" uri=\"/ClusterTest/*\" context=\"/ClusterTest/*\" />"
         +   "<jk:map type=\"Exact\" uri=\"/ClusterTest\" context=\"/ClusterTest\" />"
         +   "<jk:map type=\"Wildchar\" uri=\"/myapps/*\" context=\"/myapps/*\" />"
         +   "<jk:map type=\"Exact\" uri=\"/myapps\" context=\"/myapps\" />"
         +   "</jk:balancer>"
         +   "</jk:balancers>"
         +   "</jk:status>" ;

		StringReader reader = new StringReader(example);
		JkStatus status = (JkStatus) digester
				.parse(reader);
	    assertNotNull(status);
        assertNotNull(status.getServer());
        assertEquals(1,status.getBalancers().size());
        JkBalancer balancer = (JkBalancer)status.getBalancers().get(0);
        assertEquals(2,balancer.getBalancerMembers().size());
        assertEquals("node1",((JkBalancerMember)balancer.getBalancerMembers().get(0)).getName());
        assertEquals("node2",((JkBalancerMember)balancer.getBalancerMembers().get(1)).getName());
        assertEquals(4,balancer.getBalancerMappings().size());
	}

}
