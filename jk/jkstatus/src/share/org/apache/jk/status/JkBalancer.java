/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
 * @version $Revision$ $Date$
 * @see org.apache.jk.status.JkStatusParser
 */
public class JkBalancer implements Serializable {

    int id =-1;
    String name ;
    String type ;
    boolean sticky ;
    boolean stickyforce;
    int retries ;
    int recover ;
    String method ;
    String lock ;
    int good = -1 ;
    int degraded = -1;
    int bad = -1 ;
    int busy = -1;
    int max_busy = -1 ;
    int member_count = -1 ;
    int map_count = -1 ;
    int time_to_maintenance_min = -1 ;
    int time_to_maintenance_max = -1 ;
       
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
    public int getRecover_time() {
        return recover;
    }
    /**
     * @param recover The recover to set.
     */
    public void setRecover_time(int recover) {
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
    public boolean isSticky_session() {
        return sticky;
    }
    /**
     * @param sticky The sticky to set.
     */
    public void setSticky_session(boolean sticky) {
        this.sticky = sticky;
    }
    /**
     * @return Returns the stickyforce.
     */
    public boolean isSticky_session_force() {
        return stickyforce;
    }
    /**
     * @param stickyforce The stickyforce to set.
     */
    public void setSticky_session_force(boolean stickyforce) {
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
    /**
     * @return the bad
     * @since mod_jk 1.2.20
     */
    public int getBad() {
        return bad;
    }
    /**
     * @param bad the bad to set
     * @since mod_jk 1.2.20
     */
    public void setBad(int bad) {
        this.bad = bad;
    }
    /**
     * @return the busy
     * @since mod_jk 1.2.20
     */
    public int getBusy() {
        return busy;
    }
    /**
     * @param busy the busy to set
     * @since mod_jk 1.2.20
     */
    public void setBusy(int busy) {
        this.busy = busy;
    }
    /**
     * @return the degraded
     * @since mod_jk 1.2.20
     */
    public int getDegraded() {
        return degraded;
    }
    /**
     * @param degraded the degraded to set
     * @since mod_jk 1.2.20
     */
    public void setDegraded(int degraded) {
        this.degraded = degraded;
    }
    /**
     * @return the good
     * @since mod_jk 1.2.20
     */
    public int getGood() {
        return good;
    }
    /**
     * @param good the good to set
     * @since mod_jk 1.2.20
     */
    public void setGood(int good) {
        this.good = good;
    }
    /**
     * @return the lock
     * @since mod_jk 1.2.20
     */
    public String getLock() {
        return lock;
    }
    /**
     * @param lock the lock to set
     * @since mod_jk 1.2.20
     */
    public void setLock(String lock) {
        this.lock = lock;
    }
    /**
     * @return the max_busy
     * @since mod_jk 1.2.20
     */
    public int getMax_busy() {
        return max_busy;
    }
    /**
     * @param max_busy the max_busy to set
     * @since mod_jk 1.2.20
     */
    public void setMax_busy(int max_busy) {
        this.max_busy = max_busy;
    }
    /**
     * @return the method
     * @since mod_jk 1.2.20
     */
    public String getMethod() {
        return method;
    }
    /**
     * @param method the method to set
     * @since mod_jk 1.2.20
     */
    public void setMethod(String method) {
        this.method = method;
    }
    
    /**
     * @return the member_count
     * @since mod_jk 1.2.20
     */
    public int getMember_count() {
        return member_count;
    }
    
    /**
     * @param member_count the member_count to set
     * @since mod_jk 1.2.20
     */
    public void setMember_count(int member_count) {
        this.member_count = member_count;
    }
    
    /**
     * @return the map_count
     * @since mod_jk 1.2.20
     */
    public int getMap_count() {
        return map_count;
    }

    /**
     * @param map_count the map_count to set
     * @since mod_jk 1.2.20
     */
    public void setMap_count(int map_count) {
        this.map_count = map_count;
    }
    
    /**
     * @return the time_to_maintenance_min
     * @since mod_jk 1.2.21
     */
    public int getTime_to_maintenance_min() {
        return time_to_maintenance_min;
    }

    /**
     * @param time_to_maintenance_min the time_to_maintenance_min to set
     * @since mod_jk 1.2.21
     */
    public void setTime_to_maintenance_min(int time_to_maintenance_min) {
        this.time_to_maintenance_min = time_to_maintenance_min;
    }
    
    /**
     * @return the time_to_maintenance_max
     * @since mod_jk 1.2.21
     */
    public int getTime_to_maintenance_max() {
        return time_to_maintenance_max;
    }

    /**
     * @param time_to_maintenance_max the time_to_maintenance_max to set
     * @since mod_jk 1.2.21
     */
    public void setTime_to_maintenance_max(int time_to_maintenance_max) {
        this.time_to_maintenance_max = time_to_maintenance_max;
    }
    
}
