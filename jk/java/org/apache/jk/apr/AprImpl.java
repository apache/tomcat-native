package org.apache.jk.apr;

/** Implements the interface with the APR library. This is for internal-use
 *  only. The goal is to use 'natural' mappings for user code - for example
 *  java.net.Socket for unix-domain sockets, etc. 
 * 
 */
public class AprImpl {

    private native void initApr();
    
    private void loadNative() {


    }
    
    /** Temp - testing only, will be moved to separate file
     */
    public void main(String args[] ) {
        
    }
        

}
