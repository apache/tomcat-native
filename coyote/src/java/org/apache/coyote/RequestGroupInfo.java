package org.apache.coyote;

import java.util.ArrayList;

/** This can be moved to top level ( eventually with a better name ).
 *  It is currently used only as a JMX artifact, to agregate the data
 *  collected from each RequestProcessor thread.
 */
public class RequestGroupInfo {
    ArrayList processors=new ArrayList();

    public void addRequestProcessor( RequestInfo rp ) {
        processors.add( rp );
    }

    public long getMaxTime() {
        long maxTime=0;
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            if( maxTime < rp.getMaxTime() ) maxTime=rp.getMaxTime();
        }
        return maxTime;
    }

    // Used to reset the times
    public void setMaxTime(long maxTime) {
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            rp.setMaxTime(maxTime);
        }
    }

    public long getProcessingTime() {
        long time=0;
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            time += rp.getProcessingTime();
        }
        return time;
    }

    public void setProcessingTime(long totalTime) {
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            rp.setProcessingTime( totalTime );
        }
    }

    public int getRequestCount() {
        int requestCount=0;
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            requestCount += rp.getRequestCount();
        }
        return requestCount;
    }

    public void setRequestCount(int requestCount) {
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            rp.setRequestCount( requestCount );
        }
    }

    public int getErrorCount() {
        int requestCount=0;
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            requestCount += rp.getErrorCount();
        }
        return requestCount;
    }

    public void setErrorCount(int errorCount) {
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            rp.setErrorCount( errorCount);
        }
    }

    public long getBytesReceived() {
        long bytes=0;
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            bytes += rp.getBytesReceived();
        }
        return bytes;
    }

    public void setBytesReceived(long bytesReceived) {
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            rp.setBytesReceived( bytesReceived );
        }
    }

    public long getBytesSent() {
        long bytes=0;
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            bytes += rp.getBytesSent();
        }
        return bytes;
    }

    public void setBytesSent(long bytesSent) {
        for( int i=0; i<processors.size(); i++ ) {
            RequestInfo rp=(RequestInfo)processors.get( i );
            rp.setBytesSent( bytesSent );
        }
    }

    public void resetCounters() {
        this.setBytesReceived(0);
        this.setBytesSent(0);
        this.setRequestCount(0);
        this.setProcessingTime(0);
        this.setMaxTime(0);
        this.setErrorCount(0);
    }
}
