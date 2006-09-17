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
/**
 * @author peter
 *
 */
public class JkBalancerMember implements Serializable {
    
    int id;

    String name;

    /* possible with > 1.2.16 */
    String jvm_route;

    String type;

    String host;

    int port;

    String address;

    /* deprecated with mod_jk 1.2.16*/
    String status;
    
    /* possible with > 1.2.16 */
    String activation; 

    /* possible with > 1.2.16 */
    String state; 
        
    int lbfactor;

    long lbvalue;

    /* possible with > 1.2.16 */
    long lbmult = -1 ;
    
    int elected;

    long readed;

    long transferred;

    long errors;

    long clienterrors = -1;
    
    int busy;
    
    /* possible with > 1.2.16 */
    int maxbusy = -1;
    
    String redirect;
    
    String domain;
    
    /* possible with > 1.2.16 */
    int distance = -1;

    /**
     * @return Returns the jvm_route.
     * @since mod_jk 1.2.19
     */
    public String getJvm_route() {
        return jvm_route;
    }

    /**
     * @param jvm_route The jvm_route to set.
     * @since mod_jk 1.2.19
     */
    public void setJvm_route(String jvm_route) {
        this.jvm_route = jvm_route;
    }

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
     * @return Returns the maxbusy.
     * @since mod_jk 1.2.18
     */
    public int getMaxbusy() {
        return maxbusy;
    }

    /**
     * @param maxbusy The maxbusy to set.
     * @since mod_jk 1.2.18
     */
    public void setMaxbusy(int maxbusy) {
        this.maxbusy = maxbusy;
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
     * @return Returns the clienterrors.
     * @since mod_jk 1.2.19
     */
    public long getClienterrors() {
        return clienterrors;
    }

    /**
     * @param clienterrors The clienterrors to set.
     * @since mod_jk 1.2.19
     */
    public void setClienterrors(long clienterrors) {
        this.clienterrors = clienterrors;
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
     * @return Returns the lbmult.
     * @since mod_jk 1.2.19
     */
    public long getLbmult() {
        return lbmult;
    }

    /**
     * @param lbmult The lbmult to set.
     * @since mod_jk 1.2.19
     */
    public void setLbmult(long lbmult) {
        this.lbmult = lbmult;
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
     * @deprecated since 1.2.16
     */
    public String getStatus() {
        return status;
    }

    /**
     * @param status
     *            The status to set.
     * @deprecated since 1.2.16
     */
    public void setStatus(String status) {
        this.status = status;
    }

    /**
     * @return Returns the activation.
     * @since mod_jk 1.2.19
     */
    public String getActivation() {
        return activation;
    }

    /**
     * @param activation The activation to set.
     * @since mod_jk 1.2.19
     */
    public void setActivation(String activation) {
        this.activation = activation;
    }

    /**
     * @return Returns the state.
     * @since mod_jk 1.2.19
     */
    public String getState() {
        return state;
    }

    /**
     * @param state The state to set.
     * @since mod_jk 1.2.19
     */
    public void setState(String state) {
        this.state = state;
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

    /**
     * @return Returns the distance.
     * @since mod_jk 1.2.18
     */
    public int getDistance() {
        return distance;
    }

    /**
     * @param distance The distance to set.
     * @since mod_jk 1.2.18
     */
    public void setDistance(int distance) {
        this.distance = distance;
    }


}
