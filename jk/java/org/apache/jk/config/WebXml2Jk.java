/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */
package org.apache.jk.config;

import java.io.*;
import java.net.*;
import java.util.*;
import java.security.*;

import org.apache.tomcat.util.IntrospectionUtils;

import javax.xml.parsers.*;

/* Yes, it's using DOM */
import org.w3c.dom.*;
import org.xml.sax.*;


/* Naming conventions:

JK_CONF_DIR == serverRoot/work  ( XXX /jkConfig ? )

- Each vhost has a sub-dir named after the canonycal name

- For each webapp in a vhost, there is a separate WEBAPP_NAME.jkmap

- In httpd.conf ( or equivalent servers ), in each virtual host you
should "Include JK_CONF_DIR/VHOST/jk_apache.conf". The config
file will contain the Alias declarations and other rules required
for apache operation. Same for other servers. 

- WebXml2Jk will be invoked by a config tool or automatically for each
webapp - it'll generate the WEBAPP.jkmap files and config fragments.

WebXml2Jk will _not_ generate anything else but mappings.
It should _not_ try to guess locations or anything else - that's
another components' job.

*/

/**
 * Read a web.xml file and generate the mappings for jk2.
 * It can be used from the command line or ant.
 * 
 * In order for the web server to serve static pages, all webapps
 * must be deployed on the computer that runs Apache, IIS, etc.
 *
 * Dynamic pages can be executed on that computer or other servers
 * in a pool, but even if the main server doesn't run tomcat,
 * it must have all the static files and WEB-INF/web.xml.
 * ( you could have a script remove everything else, including jsps - if
 *  security paranoia is present ).
 *
 * XXX We could have this in WEB-INF/urimap.properties.
 *
 * @author Costin Manolache
 */
public class WebXml2Jk {
    String vhost="";
    String cpath="";
    String docBase;
    String file;
    String worker="lb"; 

    static final int CONFIG_JK2_URIMAP=1;
    
    static final int CONFIG_JK_MOUNT=2;
    static final int CONFIG_JK_URIWORKER=3;
    static final int CONFIG_JK_NSCONFIG=4;
    int type;

    // -------------------- Settings -------------------- 

    // XXX We can also generate location-independent mappings.
    
    /** Set the canonycal name of the virtual host.
     */
    public void setVhost( String vhost ) {
        this.vhost=vhost; 
    }
    
    /** Set the canonical name of the virtual host.
     */
    public void setContext( String contextPath ) {
        this.cpath=contextPath;
    }

    
    /** Set the base directory where the application is
     *  deployed ( on the web server ).
     */
    public void setDocBase(String docBase ) {
        this.docBase=docBase;
    }

    // Automatically generated.
//     /** The file where the jk2 mapping will be generated
//      */
//     public void setJk2Conf( String outFile ) {
//         file=outFile;
//         type=CONFIG_JK2_URIMAP;
//     }

//     /** Backward compat: generate JkMounts for mod_jk1
//      */
//     public void setJkmountFile( String outFile ) {
//         file=outFile;
//         type=CONFIG_JK_MOUNT;
//     }

    /* By default we map to the lb - in jk2 this is automatically
     * created and includes all  tomcat instances.
     *
     * This is equivalent to the worker in jk1.
     */
    public void setJkGroup(String route ) {
        worker=route;
    }

    // -------------------- Implementation --------------------
    void generateJk2Map(Node webN, PrintWriter out) {

        System.out.println("Generating mappings for servlets " );
        for( Node mapN=getChild( webN, "servlet-mapping" );
             mapN != null; mapN = getNext( mapN ) ) {
            
            Node servN=getChild( mapN, "servlet-name");
            if( servN==null )
                servN=getChild( mapN, "jsp-file");
            Node url=getChild( mapN, "url-pattern");
            
            out.println( "[url:" + vhost + cpath + getContent(url) + "]");
            out.println( "group=" + worker );
            out.println( "#servlet=" +  getContent( servN ));
            out.println( "#cpath=" +  cpath );
            out.println( "#vhost=" +  vhost );
        }

        Node lcN=getChild( webN, "login-config" );
        if( lcN!=null ) {
            System.out.println("Generating mapping for login-config " );
            
            Node authMethN=getChild( lcN, "auth-method");
            String authMeth=getContent( authMethN );
            if( authMeth == null ) authMeth="FORM";

            Node n1=getChild( lcN, "form-login-config");
            String loginPage=getContent( getChild( n1, "form-login-page"));
            String errPage=getContent( getChild( n1, "form-error-page"));

            System.out.println("LOGIN: " + loginPage );
            System.out.println("ERR: " + errPage );
        }

        System.out.println("Generating mappings for security constraints " );
        for( Node mapN=getChild( webN, "security-constraint" );
             mapN != null; mapN = getNext( mapN )) {
            Node wrcN=getChild( mapN, "web-resource-collection");

            Vector methods=new Vector();
            for( Node uN=getChild(wrcN, "http-method");
                 uN!=null; uN=getNext( uN )) {
                methods.addElement( getContent( uN ));
            }

            Vector urls=new Vector();
            for( Node uN=getChild(wrcN, "url-pattern");
                 uN!=null; uN=getNext( uN )) {
                urls.addElement( getContent( uN ));
            }

            // Not used at the moment
            Node acN=getChild( mapN, "auth-constraint");
            Vector roles=new Vector();
            for( Node rN=getChild(acN, "role-name");
                 rN!=null; rN=getNext( rN )) {
                roles.addElement(getContent( rN ));
            }
            generateConstraints( urls, methods, roles );
        }
    }


    public void generateConstraints( Vector urls, Vector methods, Vector roles ) {

    }
    
    // -------------------- DOM utils --------------------

        /** Get the content of a node
         */
    public String getContent(Node n ) {
        if( n==null ) return null;
        Node n1=n.getFirstChild();
        // XXX Check if it's a text node

        String s1=n1.getNodeValue();
        return s1.trim();
    }
    
    /** Get the first child
     */
    Node getChild( Node parent, String name ) {
        Node first=parent.getFirstChild();
        if( first==null ) return null;
        for (Node node = first; node != null;
             node = node.getNextSibling()) {
            //System.out.println("getNode: " + name + " " + node.getNodeName());
            if( name.equals( node.getNodeName() ) ) {
                return node;
            }
        }
        return null;
    }

    /** Get the node in the list of siblings
     */
    Node getNext( Node current ) {
        Node first=current.getNextSibling();
        String name=current.getNodeName();
        if( first==null ) return null;
        for (Node node = first; node != null;
             node = node.getNextSibling()) {
            //System.out.println("getNode: " + name + " " + node.getNodeName());
            if( name.equals( node.getNodeName() ) ) {
                return node;
            }
        }
        return null;
    }

    static class NullResolver implements EntityResolver {
        public InputSource resolveEntity (String publicId,
                                                   String systemId)
            throws SAXException, IOException
        {
            System.out.println("ResolveEntity: " + publicId + " " + systemId);
            return new InputSource(new StringReader(""));
        }
    }
    
    Document readXml(File xmlF)
        throws SAXException, IOException, ParserConfigurationException
    {
        if( ! xmlF.exists() ) {
            System.out.println("No xml file " + xmlF );
            return null;
        }
        DocumentBuilderFactory dbf =
            DocumentBuilderFactory.newInstance();

        dbf.setValidating(false);
        dbf.setIgnoringComments(false);
        dbf.setIgnoringElementContentWhitespace(true);
        //dbf.setCoalescing(true);
        //dbf.setExpandEntityReferences(true);

        DocumentBuilder db = null;
        db = dbf.newDocumentBuilder();
        db.setEntityResolver( new NullResolver() );
        
        // db.setErrorHandler( new MyErrorHandler());

        Document doc = db.parse(xmlF);
        return doc;
    }
    
    // -------------------- Main and ant wrapper --------------------
    
    public void execute() {
        try {
            if( docBase== null ) {
                System.out.println("No docbase ");
            }
            File docbF=new File(docBase);
            File wXmlF=new File( docBase, "WEB-INF/web.xml");

            Document wXmlN=readXml(wXmlF);
            if( wXmlN == null ) return;

            if( file==null ) {
                file=docBase + "WEB-INF/jk2.properties";
            }
            PrintWriter out=new PrintWriter( new FileWriter( file ));

            Node webN = wXmlN.getDocumentElement();
            if( webN==null ) {
                System.out.println("Can't find web-app");
                return;
            }

            generateJk2Map( webN, out );
            

        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }



    public static void main(String args[] ) {
        try {
            if( args.length == 1 &&
                ( "-?".equals(args[0]) || "-h".equals( args[0])) ) {
                System.out.println("Usage: ");
                System.out.println("  WebXml2Jk [OPTIONS]");
                System.out.println();
                return;
            }

            WebXml2Jk w2jk=new WebXml2Jk();

            /* do ant-style property setting */
            IntrospectionUtils.processArgs( w2jk, args, new String[] {},
                                            null, new Hashtable());
            w2jk.execute();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }

    }
    
}
