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
