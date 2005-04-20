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

package org.apache.tomcat.util.net;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Stack;
import java.util.Vector;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.tomcat.jni.Address;
import org.apache.tomcat.jni.Error;
import org.apache.tomcat.jni.File;
import org.apache.tomcat.jni.Library;
import org.apache.tomcat.jni.Poll;
import org.apache.tomcat.jni.Pool;
import org.apache.tomcat.jni.Socket;
import org.apache.tomcat.jni.Status;
import org.apache.tomcat.util.res.StringManager;
import org.apache.tomcat.util.threads.ThreadWithAttributes;


/**
 * APR tailored thread pool, providing the following services:
 * <ul>
 * <li>Socket acceptor thread</li>
 * <li>Socket poller thread</li>
 * <li>Sendfile thread</li>
 * <li>Worker threads pool</li>
 * </ul>
 *
 * @author Mladen Turk
 * @author Remy Maucherat
 */
public class AprEndpoint {


    // -------------------------------------------------------------- Constants


    protected static Log log = LogFactory.getLog(AprEndpoint.class);

    protected static StringManager sm =
        StringManager.getManager("org.apache.tomcat.util.net.res");


    // ----------------------------------------------------------------- Fields


    /**
     * Synchronization object.
     */
    protected final Object threadSync = new Object();


    /**
     * The acceptor thread.
     */
    protected Thread acceptorThread = null;


    /**
     * The socket poller thread.
     */
    protected Thread pollerThread = null;


    /**
     * The sendfile thread.
     */
    protected Thread sendfileThread = null;


    /**
     * Available processors.
     */
    // FIXME: Stack is synced, which makes it a non optimal choice
    protected Stack workers = new Stack();


    /**
     * All processors which have been created.
     */
    protected Vector created = new Vector();


    /**
     * Running state of the endpoint.
     */
    protected volatile boolean running = false;


    /**
     * Will be set to true whenever the endpoint is paused.
     */
    protected volatile boolean paused = false;


    /**
     * Track the initialization state of the endpoint.
     */
    protected boolean initialized = false;


    /**
     * Current worker threads busy count.
     */
    protected int curThreadsBusy = 0;


    /**
     * Current worker threads count.
     */
    protected int curThreads = 0;


    /**
     * Sequence number used to generate thread names.
     */
    protected int sequence = 0;


    /**
     * Root APR memory pool.
     */
    protected long rootPool = 0;


    /**
     * Server socket "pointer".
     */
    protected long serverSock = 0;


    /**
     * APR memory pool for the server socket.
     */
    protected long serverSockPool = 0;


    // ------------------------------------------------------------- Properties


    /**
     * Maximum amount of worker threads.
     */
    protected int maxThreads = 20;
    public void setMaxThreads(int maxThreads) { this.maxThreads = maxThreads; }
    public int getMaxThreads() { return maxThreads; }


    /**
     * Priority of the acceptor and poller threads.
     */
    protected int threadPriority = Thread.NORM_PRIORITY;
    public void setThreadPriority(int threadPriority) { this.threadPriority = threadPriority; }
    public int getThreadPriority() { return threadPriority; }


    /**
     * Size of the socket poller.
     */
    protected int pollerSize = 768;
    public void setPollerSize(int pollerSize) { this.pollerSize = pollerSize; }
    public int getPollerSize() { return pollerSize; }


    /**
     * Size of the sendfile (= concurrent files which can be served).
     */
    protected int sendfileSize = 256;
    public void setSendfileSize(int sendfileSize) { this.sendfileSize = sendfileSize; }
    public int getSendfileSize() { return sendfileSize; }


    /**
     * Server socket port.
     */
    protected int port;
    public int getPort() { return port; }
    public void setPort(int port ) { this.port=port; }


    /**
     * Address for the server socket.
     */
    protected InetAddress address;
    public InetAddress getAddress() { return address; }
    public void setAddress(InetAddress address) { this.address = address; }


    /**
     * Handling of accepted sockets.
     */
    protected Handler handler = null;
    public void setHandler(Handler handler ) { this.handler=handler; }
    public Handler getHandler() { return handler; }


    /**
     * Allows the server developer to specify the backlog that
     * should be used for server sockets. By default, this value
     * is 100.
     */
    protected int backlog = 100;
    public void setBacklog(int backlog) { if (backlog > 0) this.backlog = backlog; }
    public int getBacklog() { return backlog; }


    /**
     * Socket TCP no delay.
     */
    protected boolean tcpNoDelay = false;
    public boolean getTcpNoDelay() { return tcpNoDelay; }
    public void setTcpNoDelay(boolean tcpNoDelay) { this.tcpNoDelay = tcpNoDelay; }


    /**
     * Socket linger.
     */
    protected int soLinger = 100;
    public int getSoLinger() { return soLinger; }
    public void setSoLinger(int soLinger) { this.soLinger = soLinger; }


    /**
     * Socket timeout.
     */
    protected int soTimeout = -1;
    public int getSoTimeout() { return soTimeout; }
    public void setSoTimeout(int soTimeout) { this.soTimeout = soTimeout; }


    /**
     * Timeout on first request read before going to the poller, in ms.
     */
    protected int firstReadTimeout = 100;
    public int getFirstReadTimeout() { return firstReadTimeout; }
    public void setFirstReadTimeout(int firstReadTimeout) { this.firstReadTimeout = firstReadTimeout; }


    /**
     * Poll interval, in microseconds. The smaller the value, the more CPU the poller
     * will use, but the more responsive to activity it will be.
     */
    protected int pollTime = 100000;
    public int getPollTime() { return pollTime; }
    public void setPollTime(int pollTime) { this.pollTime = pollTime; }


    /**
     * The default is true - the created threads will be
     *  in daemon mode. If set to false, the control thread
     *  will not be daemon - and will keep the process alive.
     */
    protected boolean daemon = true;
    public void setDaemon(boolean b) { daemon = b; }
    public boolean getDaemon() { return daemon; }


    /**
     * Name of the thread pool, which will be used for naming child threads.
     */
    protected String name = "TP";
    public void setName(String name) { this.name = name; }
    public String getName() { return name; }


    /**
     * Use endfile for sending static files.
     */
    protected boolean useSendfile = false;
    public void setUseSendfile(boolean useSendfile) { this.useSendfile = useSendfile; }
    public boolean getUseSendfile() { return useSendfile; }


    /**
     * Number of keepalive sockets.
     */
    protected int keepAliveCount = 0;
    public int getKeepAliveCount() { return keepAliveCount; }


    /**
     * Number of sendfile sockets.
     */
    protected int sendfileCount = 0;
    public int getSendfileCount() { return sendfileCount; }


    /**
     * The socket poller.
     */
    protected Poller poller = null;
    public Poller getPoller() { return poller; }


    /**
     * The static file sender.
     */
    protected Sendfile sendfile = null;
    public Sendfile getSendfile() { return sendfile; }


    /**
     * Dummy maxSpareThreads property.
     */
    public int getMaxSpareThreads() { return 0; }


    /**
     * Dummy minSpareThreads property.
     */
    public int getMinSpareThreads() { return 0; }


    // --------------------------------------------------------- Public Methods


    /**
     * Return the APR memory pool for the server socket, to be used by handler
     * which would need to allocate things like pollers, while having
     * consistent resource handling.
     *
     * @return the id for the server socket pool
     */
    public long getServerSocketPool() {
        return serverSockPool;
    }


    /**
     * Return the amount of threads that are managed by the pool.
     *
     * @return the amount of threads that are managed by the pool
     */
    public int getCurrentThreadCount() {
        return curThreads;
    }


    /**
     * Return the amount of threads currently busy.
     *
     * @return the amount of threads currently busy
     */
    public int getCurrentThreadsBusy() {
        return curThreadsBusy;
    }


    /**
     * Return the state of the endpoint.
     *
     * @return true if the endpoint is running, false otherwise
     */
    public boolean isRunning() {
        return running;
    }


    /**
     * Return the state of the endpoint.
     *
     * @return true if the endpoint is paused, false otherwise
     */
    public boolean isPaused() {
        return paused;
    }


    // ----------------------------------------------- Public Lifecycle Methods


    /**
     * Initialize the endpoint.
     */
    public void init() throws Exception {

        if (initialized)
            return;

        try {
            // Initialize APR
            Library.initialize(null);
            // Create the root APR memory pool
            rootPool = Pool.create(0);
            // Create the pool for the server socket
            serverSockPool = Pool.create(rootPool);
            // Create the APR address that will be bound
            String addressStr = null;
            if (address == null) {
                addressStr = null;
            } else {
                addressStr = "" + address;
            }
            long inetAddress = Address.info(addressStr, Socket.APR_INET,
                                       port, 0, rootPool);
            // Create the APR server socket
            serverSock = Socket.create(Socket.APR_INET, Socket.SOCK_STREAM,
                                       Socket.APR_PROTO_TCP, rootPool);
            // Bind the server socket
            Socket.bind(serverSock, inetAddress);
            // Start listening on the server socket
            Socket.listen(serverSock, backlog);
        } catch (Exception e) {
            // FIXME: proper logging
            throw e;
        }
        initialized = true;

    }


    public void start() throws Exception {
        // Initialize socket if not done before
        if (!initialized) {
            init();
        }
        if (!running) {
            running = true;
            paused = false;

            // Start acceptor thread
            acceptorThread = new Thread(new Acceptor(), getName() + "-Acceptor");
            acceptorThread.setPriority(getThreadPriority());
            acceptorThread.setDaemon(true);
            acceptorThread.start();

            // Start poller thread
            poller = new Poller();
            poller.init();
            pollerThread = new Thread(poller, getName() + "-Poller");
            pollerThread.setPriority(getThreadPriority());
            pollerThread.setDaemon(true);
            pollerThread.start();

            // Start sendfile thread
            sendfile = new Sendfile();
            sendfile.init();
            sendfileThread = new Thread(sendfile, getName() + "-Sendfile");
            sendfileThread.setPriority(getThreadPriority());
            sendfileThread.setDaemon(true);
            sendfileThread.start();
        }
    }


    public void pause() {
        if (running && !paused) {
            paused = true;
            unlockAccept();
        }
    }

    public void resume() {
        if (running) {
            paused = false;
        }
    }

    public void stop() {
        if (running) {
            running = false;
            unlockAccept();
            poller.destroy();
            sendfile.destroy();
            acceptorThread = null;
            pollerThread = null;
            sendfileThread = null;
        }
    }

    public void destroy() throws Exception {
        if (running) {
            stop();
        }
        Pool.destroy(serverSockPool);
        serverSockPool = 0;
        // Close server socket
        Socket.close(serverSock);
        // Close all APR memory pools and resources
        Pool.destroy(rootPool);
        rootPool = 0;
        initialized = false ;
    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Get a sequence number used for thread naming.
     */
    protected int getSequence() {
        return sequence++;
    }


    /**
     * Unlock the server socket accept using a bugus connection.
     */
    protected void unlockAccept() {
        java.net.Socket s = null;
        try {
            // Need to create a connection to unlock the accept();
            if (address == null) {
                s = new java.net.Socket("127.0.0.1", port);
            } else {
                s = new java.net.Socket(address, port);
                // setting soLinger to a small value will help shutdown the
                // connection quicker
                s.setSoLinger(true, 0);
            }
        } catch(Exception e) {
            if (log.isDebugEnabled()) {
                log.debug(sm.getString("endpoint.debug.unlock", "" + port), e);
            }
        } finally {
            if (s != null) {
                try {
                    s.close();
                } catch (Exception e) {
                    // Ignore
                }
            }
        }
    }


    /**
     * Set options on a newly accepted socket.
     *
     * @param socket "pointer" to the accepted socket
     */
    protected void setSocketOptions(long socket) {
        if (soLinger >= 0)
            Socket.optSet(socket, Socket.APR_SO_LINGER, soLinger);
        if (tcpNoDelay)
            Socket.optSet(socket, Socket.APR_TCP_NODELAY, (tcpNoDelay ? 1 : 0));
        if (soTimeout > 0)
            Socket.timeoutSet(socket, soTimeout * 1000);
    }


    protected boolean processSocket(long socket, long pool) {
        // Process the connection
        int step = 1;
        boolean result = true;
        try {

            // 1: Set socket options: timeout, linger, etc
            setSocketOptions(socket);

            // 2: SSL handshake
            step = 2;
            // FIXME: SSL implementation so that Bill is happy
            /*
            if (getServerSocketFactory() != null) {
                getServerSocketFactory().handshake(s);
            }
            */

            // 3: Process the connection
            step = 3;
            result = getHandler().process(socket, pool);

        } catch (Throwable t) {
            if (step == 2) {
                if (log.isDebugEnabled()) {
                    log.debug(sm.getString("endpoint.err.handshake"), t);
                }
            } else {
                log.error(sm.getString("endpoint.err.unexpected"), t);
            }
            // Tell to close the socket
            result = false;
        }
        return result;
    }


    /**
     * Create (or allocate) and return an available processor for use in
     * processing a specific HTTP request, if possible.  If the maximum
     * allowed processors have already been created and are in use, return
     * <code>null</code> instead.
     */
    protected Worker createWorkerThread() {

        synchronized (workers) {
            if (workers.size() > 0) {
                curThreadsBusy++;
                return ((Worker) workers.pop());
            }
            if ((maxThreads > 0) && (curThreads < maxThreads)) {
                curThreadsBusy++;
                return (newWorkerThread());
            } else {
                if (maxThreads < 0) {
                    curThreadsBusy++;
                    return (newWorkerThread());
                } else {
                    return (null);
                }
            }
        }

    }


    /**
     * Create and return a new processor suitable for processing HTTP
     * requests and returning the corresponding responses.
     */
    protected Worker newWorkerThread() {

        Worker workerThread = new Worker();
        workerThread.start();
        created.addElement(workerThread);
        return (workerThread);

    }


    /**
     * Return a new worker thread, and block while to worker is available.
     */
    protected Worker getWorkerThread() {
        // Allocate a new worker thread
        Worker workerThread = createWorkerThread();
        while (workerThread == null) {
            try {
                // Wait a little for load to go down: as a result,
                // no accept will be made until the concurrency is
                // lower than the specified maxThreads, and current
                // connections will wait for a little bit instead of
                // failing right away.
                Thread.sleep(100);
            } catch (InterruptedException e) {
                // Ignore
            }
            workerThread = createWorkerThread();
        }
        return workerThread;
    }


    /**
     * Recycle the specified Processor so that it can be used again.
     *
     * @param processor The processor to be recycled
     */
    protected void recycleWorkerThread(Worker workerThread) {
        synchronized (workers) {
            workers.push(workerThread);
            curThreadsBusy--;
        }
    }


    // --------------------------------------------------- Acceptor Inner Class


    /**
     * Server socket acceptor thread.
     */
    protected class Acceptor implements Runnable {


        /**
         * The background thread that listens for incoming TCP/IP connections and
         * hands them off to an appropriate processor.
         */
        public void run() {

            // Loop until we receive a shutdown command
            while (running) {

                // Loop if endpoint is paused
                while (paused) {
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        // Ignore
                    }
                }

                // Allocate a new worker thread
                Worker workerThread = getWorkerThread();

                // Accept the next incoming connection from the server socket
                long socket = 0;
                long pool = 0;
                try {
                    pool = Pool.create(serverSockPool);
                    socket = Socket.accept(serverSock, pool);
                    // Hand this socket off to an appropriate processor
                    workerThread.assign(socket, pool);
                } catch (Exception e) {
                    // FIXME: proper logging
                    e.printStackTrace();
                }

                // The processor will recycle itself when it finishes

            }

            // Notify the threadStop() method that we have shut ourselves down
            synchronized (threadSync) {
                threadSync.notifyAll();
            }

        }

    }


    // ----------------------------------------------------- Poller Inner Class


    /**
     * Poller class.
     */
    public class Poller implements Runnable {

        protected long serverPollset = 0;
        protected long pool = 0;
        protected long[] desc;

        protected synchronized void init() {
            pool = Pool.create(serverSockPool);
            try {
                serverPollset = Poll.create(pollerSize, pool, 0, soTimeout * 1000);
            } catch (Error e) {
                if (Status.APR_STATUS_IS_EINVAL(e.getError())) {
                    try {
                        // Use WIN32 maximum poll size
                        // FIXME: Add WARN level logging about this, as scalability will
                        // be limited
                        pollerSize = 62;
                        serverPollset = Poll.create(pollerSize, pool, 0, soTimeout * 1000);
                    } catch (Error err) {
                        // FIXME: more appropriate logging
                        err.printStackTrace();
                    }
                } else {
                    // FIXME: more appropriate logging
                    e.printStackTrace();
                }
            }
            desc = new long[pollerSize * 4];
            keepAliveCount = 0;
        }

        protected void destroy() {
            Pool.destroy(pool);
        }

        public void add(long socket, long pool) {
            synchronized (this) {
                int rv = Poll.add(serverPollset, socket, pool, Poll.APR_POLLIN);
                if (rv == Status.APR_SUCCESS) {
                    keepAliveCount++;
                } else {
                    // Can't do anything: close the socket right away
                    Pool.destroy(pool);
                }
            }
        }

        public void remove(long socket) {
            synchronized (this) {
                int rv = Poll.remove(serverPollset, socket);
                if (rv == Status.APR_SUCCESS) {
                    keepAliveCount--;
                }
            }
        }

        /**
         * The background thread that listens for incoming TCP/IP connections and
         * hands them off to an appropriate processor.
         */
        public void run() {

            // Loop until we receive a shutdown command
            while (running) {
                long maintainTime = 0;
                // Loop if endpoint is paused
                while (paused) {
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        // Ignore
                    }
                }

                while (keepAliveCount < 1) {
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        // Ignore
                    }
                }

                try {
                    // Pool for the specified interval
                    int rv = Poll.poll(serverPollset, pollTime, desc, true);
                    if (rv > 0) {
                        keepAliveCount -= rv;
                        for (int n = 0; n < rv; n++) {
                            // Check for failed sockets
                            if (((desc[n*4] & Poll.APR_POLLHUP) == Poll.APR_POLLHUP)
                                    || ((desc[n*4] & Poll.APR_POLLERR) == Poll.APR_POLLERR)) {
                                // Close socket and clear pool
                                Pool.destroy(desc[n*4+2]);
                                continue;
                            }
                            // Hand this socket off to a worker
                            getWorkerThread().assign(desc[n*4+1], desc[n*4+2]);
                        }
                        maintainTime += pollTime;
                    } else if (rv < 0) {
                        // FIXME: Log with WARN at least
                        // Handle poll critical failure
                        Pool.clear(serverSockPool);
                        synchronized (this) {
                            destroy();
                            init();
                        }
                    }
                    if (rv == 0 || maintainTime > 1000000L) {
                        synchronized (this) {
                            rv = Poll.maintain(serverPollset, desc, true);
                            maintainTime = 0;
                        }
                        if (rv > 0) {
                            keepAliveCount -= rv;
                            for (int n = 0; n < rv; n++) {
                                // Close socket and clear pool
                                Pool.destroy(desc[n*4+2]);
                            }
                        }
                    }
                } catch (Throwable t) {
                    // FIXME: Proper logging
                    t.printStackTrace();
                }

            }

            // Notify the threadStop() method that we have shut ourselves down
            synchronized (threadSync) {
                threadSync.notifyAll();
            }

        }

    }


    // ----------------------------------------------------- Worker Inner Class


    /**
     * Server processor class.
     */
    protected class Worker implements Runnable {


        protected Thread thread = null;
        protected boolean available = false;
        protected long socket = 0;
        protected long pool = 0;


        /**
         * Process an incoming TCP/IP connection on the specified socket.  Any
         * exception that occurs during processing must be logged and swallowed.
         * <b>NOTE</b>:  This method is called from our Connector's thread.  We
         * must assign it to our own thread so that multiple simultaneous
         * requests can be handled.
         *
         * @param socket TCP socket to process
         */
        protected synchronized void assign(long socket, long pool) {

            // Wait for the Processor to get the previous Socket
            while (available) {
                try {
                    wait();
                } catch (InterruptedException e) {
                }
            }

            // Store the newly available Socket and notify our thread
            this.socket = socket;
            this.pool = pool;
            available = true;
            notifyAll();

        }


        /**
         * Await a newly assigned Socket from our Connector, or <code>null</code>
         * if we are supposed to shut down.
         */
        protected synchronized long await() {

            // Wait for the Connector to provide a new Socket
            while (!available) {
                try {
                    wait();
                } catch (InterruptedException e) {
                }
            }

            // Notify the Connector that we have received this Socket
            long socket = this.socket;
            available = false;
            notifyAll();

            return (socket);

        }


        /**
         * The background thread that listens for incoming TCP/IP connections and
         * hands them off to an appropriate processor.
         */
        public void run() {

            // Process requests until we receive a shutdown signal
            while (running) {

                // Wait for the next socket to be assigned
                long socket = await();
                if (socket == 0)
                    continue;

                // Process the request from this socket
                if (!processSocket(socket, pool)) {
                    // Close socket and pool
                    Pool.destroy(pool);
                    pool = 0;
                }

                // Finish up this request
                recycleWorkerThread(this);

            }

            // Tell threadStop() we have shut ourselves down successfully
            synchronized (this) {
                threadSync.notifyAll();
            }

        }


        /**
         * Start the background processing thread.
         */
        public void start() {
            thread = new ThreadWithAttributes(AprEndpoint.this, this);
            thread.setName(getName() + "-" + (++curThreads));
            thread.setDaemon(true);
            thread.start();
        }


    }


    // ----------------------------------------------- SendfileData Inner Class


    /**
     * SendfileData class.
     */
    public static class SendfileData {
        // File
        public String fileName;
        public long fd;
        public long fdpool;
        // Range information
        public long start;
        public long end;
        // Socket pool
        public long pool;
        // Position
        public long pos;
    }


    // --------------------------------------------------- Sendfile Inner Class


    /**
     * Sendfile class.
     */
    public class Sendfile implements Runnable {

        protected long sendfilePollset = 0;
        protected long pool = 0;
        protected long[] desc;
        protected HashMap sendfileData;

        protected void init() {
            pool = Pool.create(serverSockPool);
            try {
                sendfilePollset = Poll.create(sendfileSize, pool, 0, soTimeout * 1000);
            } catch (Error e) {
                if (Status.APR_STATUS_IS_EINVAL(e.getError())) {
                    try {
                        // Use WIN32 maximum poll size
                        // FIXME: Add WARN level logging about this, as scalability will
                        // be limited
                        sendfileSize = 62;
                        sendfilePollset = Poll.create(sendfileSize, pool, 0, soTimeout * 1000);
                    } catch (Error err) {
                        // FIXME: more appropriate logging
                        err.printStackTrace();
                    }
                } else {
                    // FIXME: more appropriate logging
                    e.printStackTrace();
                }
            }
            desc = new long[sendfileSize * 4];
            sendfileData = new HashMap(sendfileSize);
        }

        protected void destroy() {
            sendfileData.clear();
            Pool.destroy(pool);
        }

        public void add(long socket, SendfileData data) {
            // Initialize fd from data given
            try {
                data.fdpool = Pool.create(data.pool);
                data.fd = File.open
                    (data.fileName, File.APR_FOPEN_READ
                     | File.APR_FOPEN_SENDFILE_ENABLED | File.APR_FOPEN_BINARY,
                     0, data.fdpool);
                data.pos = data.start;
            } catch (Error e) {
                // FIXME: more appropriate logging
                e.printStackTrace();
                return;
            }
            synchronized (this) {
                sendfileData.put(new Long(socket), data);
                int rv = Poll.add(sendfilePollset, socket, 0, Poll.APR_POLLOUT);
                if (rv == Status.APR_SUCCESS) {
                    sendfileCount++;
                } else {
                    // FIXME: Log with a WARN at least, as the request will
                    // fail from the user perspective
                    // Can't do anything: close the socket right away
                    Pool.destroy(data.pool);
                }
            }
        }

        public void remove(long socket) {
            synchronized (this) {
                int rv = Poll.remove(sendfilePollset, socket);
                if (rv == Status.APR_SUCCESS) {
                    sendfileCount--;
                }
                sendfileData.remove(new Long(socket));
            }
        }

        /**
         * The background thread that listens for incoming TCP/IP connections and
         * hands them off to an appropriate processor.
         */
        public void run() {

            // Loop until we receive a shutdown command
            while (running) {

                // Loop if endpoint is paused
                while (paused) {
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        // Ignore
                    }
                }

                while (sendfileCount < 1) {
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        // Ignore
                    }
                }

                try {
                    // Pool for the specified interval
                    int rv = Poll.poll(sendfilePollset, pollTime, desc, false);
                    if (rv > 0) {
                        for (int n = 0; n < rv; n++) {
                            // Get the sendfile state
                            SendfileData state =
                                (SendfileData) sendfileData.get(new Long(desc[n*4+1]));
                            // Problem events
                            if (((desc[n*4] & Poll.APR_POLLHUP) == Poll.APR_POLLHUP)
                                    || ((desc[n*4] & Poll.APR_POLLERR) == Poll.APR_POLLERR)) {
                                // Close socket and clear pool
                                remove(desc[n*4+1]);
                                // Destroy file descriptor pool, which should close the file
                                Pool.destroy(state.fdpool);
                                // Close the socket, as the reponse would be incomplete
                                Pool.destroy(state.pool);
                                continue;
                            }
                            // Write some data using sendfile
                            int nw = Socket.sendfilet(desc[n*4+1], state.fd,
                                                      null, null, state.pos,
                                                      (int) (state.end - state.pos), 0, 0);
                            if (nw < 0) {
                                // Close socket and clear pool
                                remove(desc[n*4+1]);
                                // Destroy file descriptor pool, which should close the file
                                Pool.destroy(state.fdpool);
                                // Close the socket, as the reponse would be incomplete
                                Pool.destroy(state.pool);
                                continue;
                            }
                            state.pos = state.pos + nw;
                            if (state.pos >= state.end) {
                                remove(desc[n*4+1]);
                                // Destroy file descriptor pool, which should close the file
                                Pool.destroy(state.fdpool);
                                // If all done hand this socket off to a worker for
                                // processing of further requests
                                getWorkerThread().assign(desc[n*4+1], state.pool);
                            }
                        }
                    } else if (rv < 0) {
                        // Handle poll critical failure
                        // FIXME: Log with WARN at least
                        Pool.clear(serverSockPool);
                        synchronized (this) {
                            destroy();
                            init();
                        }
                    }
                } catch (Throwable t) {
                    // FIXME: Proper logging
                    t.printStackTrace();
                }

            }

            // Notify the threadStop() method that we have shut ourselves down
            synchronized (threadSync) {
                threadSync.notifyAll();
            }

        }

    }


    // -------------------------------------- ConnectionHandler Inner Interface


    /**
     * Bare bones interface used for socket processing. Per thread data is to be
     * stored in the ThreadWithAttributes extra folders, or alternately in
     * thread local fields.
     */
    public interface Handler {
        public boolean process(long socket, long pool);
    }


}
