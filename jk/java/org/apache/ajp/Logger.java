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

package org.apache.ajp;

/**
 * A simple logger class used by classes in this package.
 * The intention is for this class to be overridden so that
 * log messages from classes in this package can be adapted
 * to loggers used by the connector implementations for various
 * servlet containers.
 *
 * @author Kevin Seguin [seguin@apache.org]
 */
public class Logger {
    
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog(Logger.class );
    
    /**
     * Log the given message.
     * @param msg The message to log.
     */
    public void log(String msg) {
        if (log.isDebugEnabled())
            log.debug("[Ajp13] " + msg);
    }
    
    /**
     * Log the given message and error.
     * @param msg The message to log.
     * @param t The error to log.
     */
    public void log(String msg, Throwable t) {
        if (log.isDebugEnabled())
            log.debug("[Ajp13] " + msg, t);
    }
}
