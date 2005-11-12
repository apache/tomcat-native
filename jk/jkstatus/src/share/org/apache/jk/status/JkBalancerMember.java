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
package org.apache.jk.status;

import java.io.Serializable;

/**
 * @author Peter Rossbach
 * @version $Revision:$ $Date:$
 * @see org.apache.jk.status.JkStatusParser
 */
public class JkBalancerMember implements Serializable {
    
    int id;

    String name;

    String type;

    String host;

    int port;

    String address;

    String status;

    int lbfactor;

    long lbvalue;

    int elected;

    long readed;

    long transferred;

    long errors;

    int busy;

    String redirect;
    
    String domain;
    
    /**
     * @return Returns the address.
     */
    public String getAddress() {
        return address;
    }

    /**
     * @param address
     *            The address to set.
     */
    public void setAddress(String address) {
        this.address = address;
    }

    /**
     * @return Returns the busy.
     */
    public int getBusy() {
        return busy;
    }

    /**
     * @param busy
     *            The busy to set.
     */
    public void setBusy(int busy) {
        this.busy = busy;
    }

    /**
     * @return Returns the elected.
     */
    public int getElected() {
        return elected;
    }

    /**
     * @param elected
     *            The elected to set.
     */
    public void setElected(int elected) {
        this.elected = elected;
    }

    /**
     * @return Returns the errors.
     */
    public long getErrors() {
        return errors;
    }

    /**
     * @param errors
     *            The errors to set.
     */
    public void setErrors(long errors) {
        this.errors = errors;
    }

    /**
     * @return Returns the host.
     */
    public String getHost() {
        return host;
    }

    /**
     * @param host
     *            The host to set.
     */
    public void setHost(String host) {
        this.host = host;
    }

    /**
     * @return Returns the id.
     */
    public int getId() {
        return id;
    }

    /**
     * @param id
     *            The id to set.
     */
    public void setId(int id) {
        this.id = id;
    }

    /**
     * @return Returns the lbfactor.
     */
    public int getLbfactor() {
        return lbfactor;
    }

    /**
     * @param lbfactor
     *            The lbfactor to set.
     */
    public void setLbfactor(int lbfactor) {
        this.lbfactor = lbfactor;
    }

    /**
     * @return Returns the lbvalue.
     */
    public long getLbvalue() {
        return lbvalue;
    }

    /**
     * @param lbvalue
     *            The lbvalue to set.
     */
    public void setLbvalue(long lbvalue) {
        this.lbvalue = lbvalue;
    }

    /**
     * @return Returns the name.
     */
    public String getName() {
        return name;
    }

    /**
     * @param name
     *            The name to set.
     */
    public void setName(String name) {
        this.name = name;
    }

    /**
     * @return Returns the port.
     */
    public int getPort() {
        return port;
    }

    /**
     * @param port
     *            The port to set.
     */
    public void setPort(int port) {
        this.port = port;
    }

    /**
     * @return Returns the readed.
     */
    public long getReaded() {
        return readed;
    }

    /**
     * @param readed
     *            The readed to set.
     */
    public void setReaded(long readed) {
        this.readed = readed;
    }

    /**
     * @return Returns the status.
     */
    public String getStatus() {
        return status;
    }

    /**
     * @param status
     *            The status to set.
     */
    public void setStatus(String status) {
        this.status = status;
    }

    /**
     * @return Returns the transferred.
     */
    public long getTransferred() {
        return transferred;
    }

    /**
     * @param transferred
     *            The transferred to set.
     */
    public void setTransferred(long transferred) {
        this.transferred = transferred;
    }

    /**
     * @return Returns the type.
     */
    public String getType() {
        return type;
    }

    /**
     * @param type
     *            The type to set.
     */
    public void setType(String type) {
        this.type = type;
    }
    
    
    /**
     * @return Returns the domain.
     */
    public String getDomain() {
        return domain;
    }
    /**
     * @param domain The domain to set.
     */
    public void setDomain(String domain) {
        this.domain = domain;
    }
    /**
     * @return Returns the redirect.
     */
    public String getRedirect() {
        return redirect;
    }
    /**
     * @param redirect The redirect to set.
     */
    public void setRedirect(String redirect) {
        this.redirect = redirect;
    }
}
