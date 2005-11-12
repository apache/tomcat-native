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
                       +  "<jk:status xmlns:jk=\"http://jakarta.apache.org\">"
                       +  "<jk:server name=\"localhost\" port=\"80\" software=\"Apache/2.0.54 (Win32) mod_ssl/2.0.54 OpenSSL/0.9.7g mod_jk/1.2.13-dev\" version=\"1.2.12\" />"
                       +  "<jk:balancers>"
                       +  "<jk:balancer id=\"0\" name=\"lb\" type=\"lb\" sticky=\"True\" stickyforce=\"False\" retries=\"3\" recover=\"60\" >"
                       +  "<jk:member id=\"0\" name=\"node1\" type=\"ajp13\" host=\"localhost\" port=\"9012\" address=\"127.0.0.1:9012\" status=\"OK\" lbfactor=\"1\" lbvalue=\"1\" elected=\"0\" readed=\"0\" transferred=\"0\" errors=\"0\" busy=\"0\" />"
                       +  "<jk:member id=\"1\" name=\"node2\" type=\"ajp13\" host=\"localhost\" port=\"9022\" address=\"127.0.0.1:9022\" status=\"OK\" lbfactor=\"1\" lbvalue=\"1\" elected=\"0\" readed=\"0\" transferred=\"0\" errors=\"0\" busy=\"0\" />"
                       +  "<jk:map type=\"Wildchar\" uri=\"/ClusterTest/*\" context=\"/ClusterTest/*\" />"
                       +  "<jk:map type=\"Exact\" uri=\"/ClusterTest\" context=\"/ClusterTest\" />"
                       +  "<jk:map type=\"Wildchar\" uri=\"/myapps/*\" context=\"/myapps/*\" />"
                       +  "<jk:map type=\"Exact\" uri=\"/myapps\" context=\"/myapps\" />"
                       +  "</jk:balancer>"
                       +  "</jk:balancers>"
                       +  "</jk:status>" ;
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