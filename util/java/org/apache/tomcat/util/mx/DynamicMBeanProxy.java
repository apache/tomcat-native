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

package org.apache.tomcat.util.mx;

import java.io.*;
import java.net.*;
import java.lang.reflect.*;
import java.util.*;
import javax.management.*;

import org.apache.tomcat.util.IntrospectionUtils;

/**
 * DynamicMBean implementation using introspection to manage any
 * component that follows the bean/ant/Interceptor/Valve/Jk2 patterns.
 *
 * The class will wrap any component conforming to those patterns.
 * 
 * @author Costin Manolache
 */
public class DynamicMBeanProxy implements DynamicMBean {
    Object real;
    String name;
    
    Method methods[]=null;

    Hashtable attMap=new Hashtable();

    // key: attribute val: getter method
    Hashtable getAttMap=new Hashtable();

    // key: attribute val: setter method
    Hashtable setAttMap=new Hashtable();

    // key: operation val: invoke method
    Hashtable invokeAttMap=new Hashtable();

    static MBeanServer mserver=null;

    static Hashtable instances=new Hashtable();
    
    /** Create a Dynamic proxy, using introspection to manage a
     *  real tomcat component.
     */
    public DynamicMBeanProxy() {
        
    }

    public void setName(String name ) {
        this.name=name;
    }

    public String getName() {
        if( name!=null ) return name;

        if( real==null ) return null;

        name=real.getClass().getName();
        name=name.substring( name.lastIndexOf( ".") + 1 );
        Integer iInt=(Integer)instances.get(name );
        if( iInt!= null ) {
            int i=iInt.intValue();
            i++;
            instances.put( name, new Integer( i ));
            name=name + "_" + i;
        } else {
            instances.put( name, new Integer( 0 ));
        }
        return name;
    }
    
    public void registerMBean( String domain ) {
        try {
            // XXX use aliases, suffix only, proxy.getName(), etc
            ObjectName oname=new ObjectName( domain + ": name=" +  getName());
            
            getMBeanServer().registerMBean( this, oname );
        } catch( Throwable t ) {
            log.error( "Error creating mbean ", t );
        }
    }

    public static MBeanServer getMBeanServer() {
        if( mserver==null ) {
            if( MBeanServerFactory.findMBeanServer(null).size() > 0 ) {
                mserver=(MBeanServer)MBeanServerFactory.findMBeanServer(null).get(0);
            } else {
                mserver=MBeanServerFactory.createMBeanServer();
            }
        }
        
        return mserver;
    }

    private boolean supportedType( Class ret ) {
        return ret == String.class ||
            ret == Integer.class ||
            ret == Integer.TYPE ||
            ret == java.io.File.class ||
            ret == Boolean.class;
    }
    
    /** Set the managed object.
     *
     * @todo Read an XML ( or .properties ) file containing descriptions,
     *       generated from source comments
     * @todo Also filter methods based on config ( hide methods/attributes )
     * @todo Adapters for notifications ( Interceptor hooks, etc ). 
     */
    public void setReal( Object realBean ) {
        real=realBean;
    }

    private void init() {
        if( methods!=null ) return;
        methods = real.getClass().getMethods();
        for (int j = 0; j < methods.length; ++j) {
            String name=methods[j].getName();
            
            if( name.startsWith( "get" ) ) {
                if( methods[j].getParameterTypes().length != 0 ) {
                    continue;
                }
                if( ! Modifier.isPublic( methods[j].getModifiers() ) )
                    continue;
                Class ret=methods[j].getReturnType();
                if( ! supportedType( ret ) ) {
                    continue;
                }
                name=unCapitalize( name.substring(3));

                getAttMap.put( name, methods[j] );
                // just a marker, we don't use the value 
                attMap.put( name, methods[j] );
            } else if( name.startsWith( "is" ) ) {
                // not used in our code. Add later

            } else if( name.startsWith( "set" ) ) {
                Class params[]=methods[j].getParameterTypes();
                if( params.length != 1 ) {
                    continue;
                }
                if( ! Modifier.isPublic( methods[j].getModifiers() ) )
                    continue;
                Class ret=params[0];
                if( ! supportedType( ret ) ) {
                    continue;
                }
                name=unCapitalize( name.substring(3));
                setAttMap.put( name, methods[j] );
                attMap.put( name, methods[j] );
            } else {
                if( methods[j].getParameterTypes().length != 0 ) {
                    continue;
                }
                if( methods[j].getDeclaringClass() == Object.class )
                    continue;
                if( ! Modifier.isPublic( methods[j].getModifiers() ) )
                    continue;
                invokeAttMap.put( name, methods[j]);
            }
        }
    }

    /**
     * @todo Find if the 'className' is the name of the MBean or
     *       the real class ( I suppose first )
     * @todo Read (optional) descriptions from a .properties, generated
     *       from source
     * @todo Deal with constructors
     *       
     */
    public MBeanInfo getMBeanInfo() {
        if( methods==null ) {
            init();
        }
        try {
            MBeanAttributeInfo attributes[]=new MBeanAttributeInfo[attMap.size()];

            Enumeration en=attMap.keys();
            int i=0;
            while( en.hasMoreElements() ) {
                String name=(String)en.nextElement();
                attributes[i++]=new MBeanAttributeInfo(name, "Attribute " + name ,
                                                       (Method)getAttMap.get(name),
                                                       (Method)setAttMap.get(name));
            }
            
            MBeanOperationInfo operations[]=new MBeanOperationInfo[invokeAttMap.size()];
            
            en=invokeAttMap.keys();
            i=0;
            while( en.hasMoreElements() ) {
                String name=(String)en.nextElement();
                Method m=(Method)invokeAttMap.get(name);
                if( m!=null && name != null ) {
                    operations[i++]=new MBeanOperationInfo(name, m);
                } else {
                    System.out.println("Null arg " + name + " " + m );
                }
            }
            
            if( log.isDebugEnabled() )
                log.debug(real.getClass().getName() +  " getMBeanInfo()");
            
            return new MBeanInfo( real.getClass().getName(), /* ??? */
                                  "MBean for " + getName(),
                                  attributes,
                                  new MBeanConstructorInfo[0],
                                  operations,
                                  new MBeanNotificationInfo[0]);
        } catch( Exception ex ) {
            ex.printStackTrace();
            return null;
        }
    }

    static final Object[] NO_ARGS_PARAM=new Object[0];
    
    public Object getAttribute(String attribute)
        throws AttributeNotFoundException, MBeanException, ReflectionException
    {
        if( methods==null ) init();
        Method m=(Method)getAttMap.get( attribute );
        if( m==null ) throw new AttributeNotFoundException(attribute);

        try {
            if( log.isDebugEnabled() )
                log.debug(real.getClass().getName() +  " getAttribute " + attribute);
            return m.invoke(real, NO_ARGS_PARAM );
        } catch( IllegalAccessException ex ) {
            ex.printStackTrace();
            throw new MBeanException( ex );
        } catch( InvocationTargetException ex1 ) {
            ex1.printStackTrace();
            throw new MBeanException( ex1 );
        }
    }
    
    public void setAttribute(Attribute attribute)
        throws AttributeNotFoundException, InvalidAttributeValueException, MBeanException, ReflectionException
    {
        if( methods==null ) init();
        Method m=(Method)setAttMap.get( attribute.getName() );
        if( m==null ) throw new AttributeNotFoundException(attribute.getName());

        try {
            log.info(real.getClass().getName() +  "setAttribute " + attribute.getName());
            m.invoke(real, new Object[] { attribute.getValue() } );
        } catch( IllegalAccessException ex ) {
            ex.printStackTrace();
            throw new MBeanException( ex );
        } catch( InvocationTargetException ex1 ) {
            ex1.printStackTrace();
            throw new MBeanException( ex1 );
        }
    }
    
    /**
     * Invoke a method. Only no param methods are supported at the moment
     * ( init, start, execute, etc ) ( that's the most common pattern we have
     *  in tomcat/ant/etc )
     *
     * @todo Implement invoke for methods with more arguments.
     */
    public Object invoke(String method, Object[] arguments, String[] params)
        throws MBeanException, ReflectionException
    {
        if( methods==null ) init();
        Method m=(Method)invokeAttMap.get( method );
        if( m==null ) return null;

        try {
            log.info(real.getClass().getName() +  "invoke " + m.getName());
            return m.invoke(real, NO_ARGS_PARAM );
        } catch( IllegalAccessException ex ) {
            throw new MBeanException( ex );
        } catch( InvocationTargetException ex1 ) {
            throw new MBeanException( ex1 );
        }
    }


    // -------------------- Auxiliary methods --------------------
    
    public AttributeList setAttributes(AttributeList attributes) {
        Iterator attE=attributes.iterator();
        while( attE.hasNext() ) {
            Attribute att=(Attribute)attE.next();

            try {
                setAttribute( att );
            } catch( Exception ex ) {
                ex.printStackTrace();
            }
        }
        return attributes;
    }

    public AttributeList getAttributes(String[] attributes) {
        AttributeList al=new AttributeList();
        if( attributes==null ) return null;
        
        for( int i=0; i<attributes.length; i++ ) {
            try {
                Attribute att=new Attribute( attributes[i], getAttribute( attributes[i] ));
                al.add( att );
            } catch( Exception ex ) {
                ex.printStackTrace();
            }
        }
        return al;
    }
    

    // -------------------- Utils --------------------

    public static String unCapitalize(String name) {
	if (name == null || name.length() == 0) {
	    return name;
	}
	char chars[] = name.toCharArray();
	chars[0] = Character.toLowerCase(chars[0]);
	return new String(chars);
    }

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( DynamicMBeanProxy.class );

}
