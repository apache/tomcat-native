/*
 * Copyright 2002,2005 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.jk.status;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;

import org.apache.catalina.ant.AbstractCatalinaTask;
import org.apache.tools.ant.BuildException;

/**
 * Ant task that implements the <code>/jkstatus?cmd=reset&amp;w=loadbalancer</code> command, supported by the
 * mod_jk status (1.2.15) application.
 * 
 * @author Peter Rossbach
 * @version $Revision: 1.3 $
 * @since 5.5.13
 */
public class JkStatusResetTask extends AbstractCatalinaTask {

    private String workerLb;

    /**
     *  
     */
    public JkStatusResetTask() {
        super();
        setUrl("http://localhost/jkstatus");
    }

    /**
     * @return Returns the workerLb.
     */
    public String getWorkerLb() {
        return workerLb;
    }

    /**
     * @param workerLb
     *            The workerLb to set.
     */
    public void setWorkerLb(String workerLb) {
        this.workerLb = workerLb;
    }

    /**
     * Execute the requested operation.
     * 
     * @exception BuildException
     *                if an error occurs
     */
    public void execute() throws BuildException {

        super.execute();
        checkParameter();
        StringBuffer sb = createLink();
        execute(sb.toString(), null, null, -1);

    }

    /**
     * Create jkstatus reset link
     * <ul>
     * <li><b>load balance example:
     * </b>http://localhost/jkstatus?cmd=reset&w=loadbalancer&mime=txt</li>
     * </ul>
     * 
     * @return create jkstatus link
     */
    private StringBuffer createLink() {
        // Building URL
        StringBuffer sb = new StringBuffer();
        try {
            sb.append("?cmd=reset");
            sb.append("&w=");
            sb.append(URLEncoder.encode(workerLb, getCharset()));
            sb.append("&mime=txt");

        } catch (UnsupportedEncodingException e) {
            throw new BuildException("Invalid 'charset' attribute: "
                    + getCharset());
        }
        return sb;
    }

    /**
     * check correct lb and worker pararmeter
     */
    protected void checkParameter() {
        if (workerLb == null) {
            throw new BuildException("Must specify 'workerLb' attribute");
        }
    }
}