package org.apache.tomcat.util.threads;

import java.util.Hashtable;

/** Special thread that allows storing of attributes and notes.
 *  A guard is used to prevent untrusted code from accessing the
 *  attributes.
 *
 *  This avoids hash lookups and provide something very similar
 * with ThreadLocal ( but compatible with JDK1.1 and faster on
 * JDK < 1.4 ).
 *
 * The main use is to store 'state' for monitoring ( like "processing
 * request 'GET /' ").
 */
public class ThreadWithAttributes extends Thread {
    private Object control;
    public static int MAX_NOTES=16;
    private Object notes[]=new Object[MAX_NOTES];
    private Hashtable attributes=new Hashtable();
    private String currentStage;
    private Object param;

    public ThreadWithAttributes(Object control, Runnable r) {
        super(r);
        this.control=control;
    }

    /** Notes - for attributes that need fast access ( array )
     * The application is responsible for id management
     */
    public final void setNote( Object control, int id, Object value ) {
        if( this.control != control ) return;
        notes[id]=value;
    }

    /** Information about the curent performed operation
     */
    public final String getCurrentStage(Object control) {
        if( this.control != control ) return null;
        return currentStage;
    }

    /** Information about the current request ( or the main object
     * we are processing )
     */
    public final Object getParam(Object control) {
        if( this.control != control ) return null;
        return param;
    }

    public final void setCurrentStage(Object control, String currentStage) {
        if( this.control != control ) return;
        this.currentStage = currentStage;
    }

    public final void setParam( Object control, Object param ) {
        if( this.control != control ) return;
        this.param=param;
    }

    public final Object getNote(Object control, int id ) {
        if( this.control != control ) return null;
        return notes[id];
    }

    /** Generic attributes. You'll need a hashtable lookup -
     * you can use notes for array access.
     */
    public final Hashtable getAttributes(Object control) {
        return attributes;
    }
}
