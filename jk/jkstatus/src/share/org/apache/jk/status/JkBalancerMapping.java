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

/**
 * @author Peter Rossbach
 * @version $Revision$ $Date$
 * @see org.apache.jk.status.JkStatusParser
 */
public class JkBalancerMapping implements Serializable {
    int id =-1 ;
	String type ;
    String uri;
    String context ;
    String source ;
    
    /**
	 * @return the id
	 */
	public int getId() {
		return id;
	}
	/**
	 * @param id the id to set
	 */
	public void setId(int id) {
		this.id = id;
	}
	/**
     * @return Returns the context.
     * @deprecated mod_jk 1.2.20
     */
    public String getContext() {
        return context;
    }
    /**
     * @param context The context to set.
     * @deprecated mod_jk 1.2.20
     */
    public void setContext(String context) {
        this.context = context;
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
     * @return Returns the uri.
     */
    public String getUri() {
        return uri;
    }
    /**
     * @param uri The uri to set.
     */
    public void setUri(String uri) {
        this.uri = uri;
    }
	/**
	 * @return the source
     * @since mod_jk 1.2.20
 	 */
	public String getSource() {
		return source;
	}
	/**
	 * @param source the source to set
	 * @since mod_jk 1.2.20
     */
	public void setSource(String source) {
		this.source = source;
	}
    
 }
