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

    // -------------------- Settings -------------------- 

    // XXX We can also generate location-independent mappings.
    
    /** Set the canonycal name of the virtual host.
     */
    public void setHost( String vhost ) {
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
    public void setGroup(String route ) {
        worker=route;
    }

    // -------------------- Generators --------------------
    public static interface MappingGenerator {
        void setWebXmlReader(WebXml2Jk wxml );

        /** Start section( vhost declarations, etc )
         */
        void generateStart(PrintWriter out );

        void generateEnd(PrintWriter out );
        
        void generateServletMapping( PrintWriter out,
                                     String servlet, String url );
        void generateFilterMapping( PrintWriter out, String servlet, String url );

        void generateLoginConfig( PrintWriter out, String loginPage,
                                  String errPage, String authM );

        void generateErrorPage( PrintWriter out, int err, String location );
        
        void generateMimeMapping( PrintWriter out, String ext, String type );
    
        void generateWelcomeFiles( PrintWriter out, Vector wf );

        void generateConstraints( PrintWriter out, Vector urls, Vector methods, Vector roles, boolean isSSL );
    }    
    
    // -------------------- Implementation --------------------
    void generate(PrintWriter out, MappingGenerator gen, Node webN ) {

        log.info("Generating mappings for servlets " );
        for( Node mapN=getChild( webN, "servlet-mapping" );
             mapN != null; mapN = getNext( mapN ) ) {
            
            String serv=getChildContent( mapN, "servlet-name");
            String url=getChildContent( mapN, "url-pattern");
            
            gen.generateServletMapping(  out, serv, url );
        }

        log.info("Generating mappings for filters " );
        for( Node mapN=getChild( webN, "filter-mapping" );
             mapN != null; mapN = getNext( mapN ) ) {
            
            String filter=getChildContent( mapN, "filter-name");
            String url=getChildContent( mapN, "url-pattern");

            gen.generateFilterMapping(  out, filter, url );
        }


        for( Node mapN=getChild( webN, "error-page" );
             mapN != null; mapN = getNext( mapN ) ) {
            String errorCode= getChildContent( mapN, "error-code" );
            String location= getChildContent( mapN, "location" );

            if( errorCode!=null && ! "".equals( errorCode ) ) {
                try {
                    int err=new Integer( errorCode ).intValue();
                    gen.generateErrorPage( out, err, location );
                } catch( Exception ex ) {
                    log.error( "Format error " + location, ex);
                }
            }
        }

        
        Node n0=getChild( webN, "welcome-file-list" );
        if( n0!=null ) {
            Vector wF=new Vector();
            for( Node mapN=getChild( webN, "welcome-file" );
                 mapN != null; mapN = getNext( mapN ) ) {
                wF.addElement( getContent(mapN));
            }
            gen.generateWelcomeFiles( out, wF );
        }

        for( Node mapN=getChild( webN, "mime-mapping" );
             mapN != null; mapN = getNext( mapN ) ) {
            String ext=getChildContent( mapN, "extension" );
            String type=getChildContent( mapN, "mime-type" );

            gen.generateMimeMapping( out, ext, type );
        }
        
        Node lcN=getChild( webN, "login-config" );
        if( lcN!=null ) {
            log.info("Generating mapping for login-config " );
            
            String authMeth=getContent( getChild( lcN, "auth-method"));
            String realm=getContent( getChild( lcN, "realm-name"));
            if( authMeth == null ) authMeth="BASIC";

            Node n1=getChild( lcN, "form-login-config");
            String loginPage= getChildContent( n1, "form-login-page");
            String errPage= getChildContent( n1, "form-error-page");

            gen.generateLoginConfig( out, loginPage, errPage, authMeth );
        }

        log.info("Generating mappings for security constraints " );
        for( Node mapN=getChild( webN, "security-constraint" );
             mapN != null; mapN = getNext( mapN )) {

            Vector methods=new Vector();
            Vector urls=new Vector();
            Vector roles=new Vector();
            boolean isSSL=false;
            
            Node wrcN=getChild( mapN, "web-resource-collection");
            for( Node uN=getChild(wrcN, "http-method");
                 uN!=null; uN=getNext( uN )) {
                methods.addElement( getContent( uN ));
            }
            for( Node uN=getChild(wrcN, "url-pattern");
                 uN!=null; uN=getNext( uN )) {
                urls.addElement( getContent( uN ));
            }

            // Not used at the moment
            Node acN=getChild( mapN, "auth-constraint");
            for( Node rN=getChild(acN, "role-name");
                 rN!=null; rN=getNext( rN )) {
                roles.addElement(getContent( rN ));
            }

            Node ucN=getChild( mapN, "user-data-constraint");
            String transp=getContent(getChild( ucN, "transport-guarantee"));
            if( transp!=null ) {
                if( "INTEGRAL".equalsIgnoreCase( transp ) ||
                    "CONFIDENTIAL".equalsIgnoreCase( transp ) ) {
                    isSSL=true;
                }
            }
            for( Node rN=getChild(acN, "role-name");
                 rN!=null; rN=getNext( rN )) {
                roles.addElement(getContent( rN ));
            }

            gen.generateConstraints( out, urls, methods, roles, isSSL );
        }
    }

    // -------------------- DOM utils --------------------

    /** Get the content of a node
     */
    public static String getContent(Node n ) {
        if( n==null ) return null;
        Node n1=n.getFirstChild();
        // XXX Check if it's a text node

        String s1=n1.getNodeValue();
        return s1.trim();
    }
    
    /** Get the first child
     */
    public static Node getChild( Node parent, String name ) {
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

    /** Get the first child's content ( i.e. it's included TEXT node )
     */
    public static String getChildContent( Node parent, String name ) {
        Node first=parent.getFirstChild();
        if( first==null ) return null;
        for (Node node = first; node != null;
             node = node.getNextSibling()) {
            //System.out.println("getNode: " + name + " " + node.getNodeName());
            if( name.equals( node.getNodeName() ) ) {
                return getContent( node );
            }
        }
        return null;
    }

    /** Get the node in the list of siblings
     */
    public static Node getNext( Node current ) {
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

    public static class NullResolver implements EntityResolver {
        public InputSource resolveEntity (String publicId,
                                                   String systemId)
            throws SAXException, IOException
        {
            System.out.println("ResolveEntity: " + publicId + " " + systemId);
            return new InputSource(new StringReader(""));
        }
    }
    
    public static Document readXml(File xmlF)
        throws SAXException, IOException, ParserConfigurationException
    {
        if( ! xmlF.exists() ) {
            log.error("No xml file " + xmlF );
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

            MappingGenerator generator=new GeneratorJk2();
            
            generator.setWebXmlReader( this );
            generate( out, generator, webN );
            

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
                System.out.println("  -docBase DIR        The location of the webapp. Required");
                System.out.println("  -group GROUP        Group, if you have multiple tomcats with diffrent content. " );
                System.out.println("                      The default is 'lb', and should be used in most cases");
                System.out.println("  -host HOSTNAME      Canonical hostname - for virtual hosts");
                System.out.println("  -context /CPATH     Context path where the app will be mounted");
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

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( WebXml2Jk.class );
}
