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

package org.apache.tomcat.util.http.mapper;

import java.util.Arrays;

import org.apache.tomcat.util.http.mapper.Mapper.Host;
import org.apache.tomcat.util.buf.CharChunk;

/**
 * Map set for a collection of hosts.
 * 
 * @author George Sexton
 * @author Peter Rossbach
 */
class HostMap {

    private Host[] hosts=new Host[0];
    private boolean enableWildCardMatching=false;

    public HostMap(){
    }

    /**
     * Copy an existing HostMap into a new one.
     */
    public HostMap(HostMap hmCurrent){
        hosts=hmCurrent.toHostArray();
        setEnableWildCardMatching(hmCurrent.getEnableWildCardMatching());
    }

    /**
     * Enable that host wildcards are match 
     * @param value
     */
    public void setEnableWildCardMatching(final boolean value){
        enableWildCardMatching=value;
    }
    
    public boolean getEnableWildCardMatching(){
        return enableWildCardMatching;
    }

    /**
     * Return the names of all mapped hosts as a string array.
     */
    public String[] getHosts(){
        Host[] h=hosts;
        String[] result=new String[h.length];
        for (int i=0; i < h.length; i++) {
            result[i]=h[i].name;
        }
        Arrays.sort(result);
        return result;
    }

    /**
     * return a count of mapped hosts.
     */
    public int getCount(){
        return hosts.length;
    }

    /**
     * Return the set of hosts this map contains as an array.
     */
    public Host[] toHostArray(){
        Host[] hCurrent=hosts;
        Host[] hResult=new Host[hCurrent.length];
        System.arraycopy(hCurrent,0,hResult,0,hCurrent.length);
        return hResult;
    }

    /**
     * Add a new host to the map.
     * 
     * @param h The new Host object to add.
     */
    public void addHost(Host h){
        CharChunk cc=new CharChunk();
        cc.setChars(h.name.toLowerCase().toCharArray(),0,h.name.length());
        h.hash=cc.hash();
        Host[] hosts=this.hosts;
        Host[] newHosts=new Host[hosts.length+1];
        System.arraycopy(hosts,0,newHosts,1,hosts.length);
        newHosts[0]=h;
        Arrays.sort(newHosts);
        this.hosts=newHosts;
    }

    /**
     * Remove a specified host (an all aliases).
     * @param name The name of the host to remove.
     */
    public void removeHost(String name){
        Host hFound=getHost(name);
        if (hFound!=null) {
            int iCurrent=0;
            Host[] hostsNew=new Host[hosts.length];
            for (int i=0; i < hosts.length; i++) {
                if (hFound.object!=hosts[i].object) {
                    hostsNew[iCurrent++]=hosts[i];
                }
            }
            Host[] hTemp=new Host[iCurrent];
            if (iCurrent>0) {
                System.arraycopy(hostsNew,0,hTemp,0,iCurrent);
            }
            hosts=hTemp;
        }
    }

    /**
     * Return true if the specified host is contained
     * in the map.
     * 
     * @param hostName the host name to test.
     */
    public boolean contains(String hostName){
        return getHost(hostName)!=null;
    }

    /**
     * Return a host mapping record. 
     * @param hostName the name of the host to return.
     * @see getHost(CharChunk)
     * 
     */
    public Host getHost(String hostName){
        CharChunk cc=new CharChunk();
        char[] c=hostName.toCharArray();
        cc.setChars(c,0,c.length);
        return getHost(cc);
    }

    /**
     * Binary search algorithm used internally to quickly find
     * a host.
     */
    private static int findPos(Host[] hosts, final int iHash){

        int lo=0,hi=hosts.length-1,mid;
        while (lo<=hi) {
            mid=(lo+hi)/2;
            if (iHash==hosts[mid].hash) {
                return mid;
            } else if (hosts[mid].hash>iHash) {
                hi=mid-1;
            } else {
                lo=mid+1;
            }
        }
        return -1;
    }

    /**
     * Find a specific host in the map.
     * 
     * This variant uses a CharChunk which the
     * Mapper uses internally for processing.
     * <BR><BR>
     * If a match is not found, and wild card matching is enabled, 
     * the function will attempt to find a wild-card match. I.E. 
     * a host record (possibly an alias) of *.domain.unit If a 
     * wild-card record is found, and the Host record 
     * hostMatchesRemaining property is non-zero,
     * then <B>hostName</B> will automatically be added to the map
     * using the hostRecord that was found.
     * 
     * @param hostName the (case insensitive)
     * name of the host to return.
     */
    public Host getHost(CharChunk hostName){
        Host[] h=hosts;
        final int iHash=hostName.hashIgnoreCase();
        int iPos=findPos(h,iHash);
        if (iPos>=0) {
            while (iPos>0 && hosts[iPos-1].hash==iHash) {
                iPos--;
            }
            while (hosts[iPos].hash==iHash) {
                if (hostName.equalsIgnoreCase(h[iPos].name)) {
                    return h[iPos];
                }
                iPos++;
            }
        }
        Host hResult=null;
        if (enableWildCardMatching) {

            /*
            Go over the host list, and look for wild card entries
            and if we find a wild-card entry that matches add it
            to the host map.
    
            Because this code adds the found host name to the
            host array, this lookup is only done once per 
            unresolved host name.
    
            The only area this may cause problems is where you
            are using the DefaultHost property to map lots of
            hosts. Since the code only looks at the first character
            of each mapped host, this should still be pretty 
            efficient. This minor inefficiency is probably more
            than made up by the caching of the default host object
            that I added above.
    
            */
            Host hFound=null;
            String sName=hostName.toString().toLowerCase();
            for (int i=0; i < hosts.length; i++) {
                if (hosts[i].name.charAt(0)=='*') {
                    if (sName.endsWith(hosts[i].name.substring(1).toLowerCase())) {
                        // This is a match.
                        hFound=hosts[i];
                        break;
                    }
                }
            }
            if (hFound!=null && (hFound.aliasMatchesRemaining>0)) {
                //
                //  We got a hit on the wild card. Copy the entry and
                //  add the found name into the Host table as another
                //  alias.
                //
                hFound.aliasMatchesRemaining--;

                hResult=new Host();
                hResult.name=sName;
                hResult.contextList=hFound.contextList;
                hResult.object=hFound.object;
                addHost(hResult);
            }
        }
        return hResult;

    }

}
