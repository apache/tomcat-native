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

public class Constants {
    /** Our package name. */
    public static final String PACKAGE="org.apache.catalina.connector.warp";

    /** Compile-in debug flag. */
    public static final boolean DEBUG=true;

    /**
     * The WARP protocol major version.
     */
    public static final int VERS_MAJOR=0;

    /**
     * The WARP protocol minor version.
     */
    public static final int VERS_MINOR=9;

    /**
     * INVALID: The packet type hasn't been set yet.
     */
    public static final int TYPE_INVALID=-1;

    /**
     * ERROR: The last operation didn't completed correctly.
     * <br>
     * Payload description:<br>
     * [string] An error message.<br>
     */
    public static final int TYPE_ERROR=0x00;

    /**
     * FATAL: A protocol error occourred, the connection must be closed.
     * <br>
     * Payload description:<br>
     * [string] An error message.<br>
     */
    public static final int TYPE_FATAL=0xff;

    /**
     * CONF_WELCOME: The server issues this packet when a connection is
     * opened. The server awaits for configuration information.
     * <br>
     * Payload description:<br>
     * [ushort] Major protocol version.<br>
     * [ushort] Minor protocol version.<br>
     * [integer] The server unique-id.<br>
     */
    public static final int TYPE_CONF_WELCOME=0x01;

    /**
     * CONF_HOST: The client attempts to configure a new virtual host.
     * <br>
     * Payload description:<br>
     * [string] The virtual host name.<br>
     * [ushort] The virtual host port.<br>
     */
    public static final int TYPE_CONF_HOST=0x02;

    /**
     * CONF_HOSTID: The server replies to a CONF_HOST message with the host
     * identifier of the configured host.
     * <br>
     * Payload description:<br>
     * [integer] The virtual unique id for this server.<br>
     */
    public static final int TYPE_CONF_HOSTID=0x03;

    /**
     * CONF_APPL: The client attempts to configure a new web application.
     * <br>
     * Payload description:<br>
     * [string] The application name (jar file or directory).<br>
     * [string] The application deployment path.<br>
     * [integer] The virtual host unique id for this server.<br>
     */
    public static final int TYPE_CONF_APPL=0x04;

    /**
     * CONF_APPLID: The server replies to a CONF_APPL message with the web
     * application identifier of the configured application.
     * <br>
     * Payload description:<br>
     * [integer] The web application unique id for this server.<br>
     */
    public static final int TYPE_CONF_APPLID=0x05;

    /**
     * CONF_DONE: Client issues this message when all configurations have been
     * processed.
     * <br>
     * No payload:<br>
     */
    public static final int TYPE_CONF_DONE=0x06;
}
