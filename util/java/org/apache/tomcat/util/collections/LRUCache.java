/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:  
 *       "This product includes software developed by the 
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written 
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */ 
package org.apache.tomcat.util.collections;

import java.util.Hashtable;

/**
 * This class implements a Generic LRU Cache
 *
 *
 * @author Ignacio J. Ortega
 *
 */

public class LRUCache
{
    class CacheNode
    {

        CacheNode prev;
        CacheNode next;
        Object value;
        Object key;

        CacheNode()
        {
        }
    }


    public LRUCache(int i)
    {
        currentSize = 0;
        cacheSize = i;
        nodes = new Hashtable(i);
    }

    public Object get(Object key)
    {
        CacheNode node = (CacheNode)nodes.get(key);
        if(node != null)
        {
            moveToHead(node);
            return node.value;
        }
        else
        {
            return null;
        }
    }

    public void put(Object key, Object value)
    {
        CacheNode node = (CacheNode)nodes.get(key);
        if(node == null)
        {
            if(currentSize >= cacheSize)
            {
                if(last != null)
                    nodes.remove(last.key);
                removeLast();
            }
            else
            {
                currentSize++;
            }
            node = new CacheNode();
        }
        node.value = value;
        node.key = key;
        moveToHead(node);
        nodes.put(key, node);
    }

    public Object remove(Object key) {
        CacheNode node = (CacheNode)nodes.get(key);
        if (node != null) {
            if (node.prev != null) {
                node.prev.next = node.next;
            }
            if (node.next != null) {
                node.next.prev = node.prev;
            }
            if (last == node)
                last = node.prev;
            if (first == node)
                first = node.next;
        }
        return node;
    }

    public void clear()
    {
        first = null;
        last = null;
    }

    private void removeLast()
    {
        if(last != null)
        {
            if(last.prev != null)
                last.prev.next = null;
            else
                first = null;
            last = last.prev;
        }
    }

    private void moveToHead(CacheNode node)
    {
        if(node == first)
            return;
        if(node.prev != null)
            node.prev.next = node.next;
        if(node.next != null)
            node.next.prev = node.prev;
        if(last == node)
            last = node.prev;
        if(first != null)
        {
            node.next = first;
            first.prev = node;
        }
        first = node;
        node.prev = null;
        if(last == null)
            last = first;
    }

    private int cacheSize;
    private Hashtable nodes;
    private int currentSize;
    private CacheNode first;
    private CacheNode last;
}
