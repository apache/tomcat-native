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

package org.apache.ajp.tomcat4;

import org.apache.catalina.Logger;

class Ajp13Logger {

    private String name = null;
    private Ajp13Connector connector = null;
    private boolean logStackTrace = false;
    
    Ajp13Logger() {
        name = toString();
    }

    void setConnector(Ajp13Connector connector) {
        this.connector = connector;
    }

    void setName(String name) {
        this.name = name;
    }

    /**
     * Log a message on the Logger associated with our Container (if any)
     *
     * @param message Message to be logged
     */
    void log(String message) {

        if (logStackTrace) {
            log(message, new Throwable());
        } else {
            Logger logger = getLogger();
            
            if (logger != null)
                logger.log(name + " " + message);
            else
                System.out.println(name + " " + message);
        }
    }

    /**
     * Log a message on the Logger associated with our Container (if any)
     *
     * @param message Message to be logged
     * @param throwable Associated exception
     */
    void log(String message, Throwable throwable) {

        Logger logger = getLogger();

	if (logger != null)
	    logger.log(name + " " + message, throwable);
	else {
	    System.out.println(name + " " + message);
	    throwable.printStackTrace(System.out);
	}

    }

    private Logger getLogger() {

        if (connector != null) {
            return connector.getContainer().getLogger();
        } else {
            return null;
        }

    }

}
