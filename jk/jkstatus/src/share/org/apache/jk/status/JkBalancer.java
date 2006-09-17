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
import java.util.ArrayList;
import java.util.List;

/**
 * @author Peter Rossbach
 * @version $Revision:$ $Date:$
 * @see org.apache.jk.status.JkStatusParser
 */
public class JkBalancer implements Serializable {

    int id ;
    String name ;
    String type ;
    boolean sticky ;
    boolean stickyforce;
    int retries ;
    int recover ;
        
    List members = new ArrayList() ;
    List mappings = new ArrayList() ;
     
    /**
     * @return Returns the id.
     */
    public int getId() {
        return id;
    }
    /**
     * @param id The id to set.
     */
    public void setId(int id) {
        this.id = id;
    }
    /**
     * @return Returns the mappings.
     */
    public List getBalancerMappings() {
        return mappings;
    }
    /**
     * @param mappings The mappings to set.
     */
    public void setBalancerMappings(List mappings) {
        this.mappings = mappings;
    }
    public void addBalancerMapping(JkBalancerMapping mapping) {
        mappings.add(mapping);
    }
    public void removeBalancerMapping(JkBalancerMapping mapping) {
        mappings.remove(mapping);
    }
    /**
     * @return Returns the members.
     */
    public List getBalancerMembers() {
        return members;
    }
    /**
     * @param members The members to set.
     */
    public void setBalancerMembers(List members) {
        this.members = members;
    }
    public void addBalancerMember(JkBalancerMember member) {
        members.add(member);
    }   
    public void removeBalancerMember(JkBalancerMember member) {
        members.remove(member);
    }   
    /**
     * @return Returns the name.
     */
    public String getName() {
        return name;
    }
    /**
     * @param name The name to set.
     */
    public void setName(String name) {
        this.name = name;
    }
    /**
     * @return Returns the recover.
     */
    public int getRecover() {
        return recover;
    }
    /**
     * @param recover The recover to set.
     */
    public void setRecover(int recover) {
        this.recover = recover;
    }
    /**
     * @return Returns the retries.
     */
    public int getRetries() {
        return retries;
    }
    /**
     * @param retries The retries to set.
     */
    public void setRetries(int retries) {
        this.retries = retries;
    }
    /**
     * @return Returns the sticky.
     */
    public boolean isSticky() {
        return sticky;
    }
    /**
     * @param sticky The sticky to set.
     */
    public void setSticky(boolean sticky) {
        this.sticky = sticky;
    }
    /**
     * @return Returns the stickyforce.
     */
    public boolean isStickyforce() {
        return stickyforce;
    }
    /**
     * @param stickyforce The stickyforce to set.
     */
    public void setStickyforce(boolean stickyforce) {
        this.stickyforce = stickyforce;
    }
    /**
     * @return Returns the type.
     */
    public String getType() {
        return type;
    }
    /**
     * @param type The type to set.
     */
    public void setType(String type) {
        this.type = type;
    }

}
