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

package org.apache.naming.util;

import java.io.File;
import java.io.IOException;
import java.io.StringReader;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

// moved from jk2 config package. 

/**
 *
 * @author Costin Manolache
 */
public class DomXml {
    String file;
    String name;

    // -------------------- Settings -------------------- 

    /** 
     */
    public void setFile( String file ) {
        this.file=file; 
    }

    /** 
     */
    public void setName( String name ) {
        this.name=name; 
    }

    // -------------------- Implementation --------------------
    Node domN;
    
    /** Return the top level node
     */
    public Node getNode() {
        return domN;
    }

    // -------------------- ant wrapper --------------------
    
    public void execute() {
        try {
            if( file== null) {
                log.error("No file attribute");
                return;
            }

            File docF=new File(file);

            Document doc=readXml(docF);
            if( doc == null ) return;

            domN = doc.getDocumentElement();
            if( domN==null ) {
                log.error("Can't find the root node");
                return;
            }

        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( DomXml.class );

    
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
        if( parent==null ) return null;
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
            if( log.isTraceEnabled()) 
                log.trace("ResolveEntity: " + publicId + " " + systemId);
            return new InputSource(new StringReader(""));
        }
    }

    public void saveXml( Node n, File xmlF ) {
        
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

}
