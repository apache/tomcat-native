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
 
import java.security.cert.X509Certificate;
import java.security.cert.CertificateFactory;
 
import java.io.ByteArrayInputStream;
 
/*
 * Certificates handling.
 */
 
public class WarpCertificates {
    X509Certificate jsseCerts[] = null;
    /**
     * Create the certificate using the String.
     */
    public WarpCertificates(String certString) {
        if (certString == null) return;

        byte[] certData = certString.getBytes();
        ByteArrayInputStream bais = new ByteArrayInputStream(certData);
 
        // Fill the first element.
        try {
            CertificateFactory cf =
                    CertificateFactory.getInstance("X.509");
            X509Certificate cert = (X509Certificate)
                    cf.generateCertificate(bais);
            jsseCerts =  new X509Certificate[1];
            jsseCerts[0] = cert;
        } catch(java.security.cert.CertificateException e) {
            // Certificate convertion failed.
            return;
        }
    }
    public X509Certificate [] getCertificates() {
        return jsseCerts;
    }
}
