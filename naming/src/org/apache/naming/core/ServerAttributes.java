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

package org.apache.naming.core;

import javax.naming.directory.BasicAttributes;

/**
 * Extended version for Attribute. All our dirContexts should return objects
 * of this type. Methods that take attribute param should use this type
 * of objects for performance.
 *
 * This is an extension of the 'note' in tomcat 3.3. Each attribute will have an
 * 'ID' ( small int ) and an associated namespace. The attributes are recyclable.
 *
 * The attribute is designed for use in server env, where performance is important.
 *
 * @author Costin Manolache
 */
public class ServerAttributes extends BasicAttributes
{
    
    // no extra methods yet.     
}
