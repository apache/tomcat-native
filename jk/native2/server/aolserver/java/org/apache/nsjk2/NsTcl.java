/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2002 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */
/*
 * Copyright (c) 2000 Daniel Wickstrom, nsjava 1.0.5
 * Modified by Alexander Leykekh (aolserver@aol.net)
 *
 */

package org.apache.nsjk2;

import java.io.Writer;
import javax.servlet.ServletContext;
import javax.servlet.http.HttpServletRequest;
import java.io.IOException;

/**
 * <P>NsTcl provides a java wrapper around tcl interpreter so that tcl 
 * can be called from within java.
 *
 * @author Daniel Wickstrom
 * @author Alexander Leykekh
 * @see NsTcl
 */

public class NsTcl {

    /**
     * writer to send ADP buffer content to
     *
     */
    protected Writer out = null;

    /**
     * base working directory for ADP includes
     *
     */
    protected String cwd = null;

    /**
     * Create a tcl interpreter connection. Use this form when there is a potential
     * of ADP file being included.
     *
     * @param ctx pass JSP implicit variable "application"
     * @param req pass JSP implicit variable "request"
     * @param out pass JSP implicit variable "out" or a custom Writer object
     * (it is programmer's responsibility to flush such writer object)
     */
    public NsTcl(ServletContext ctx, HttpServletRequest req, Writer out) {
	this.out = out;

	if (ctx!=null && req!=null) {
	    cwd = ctx.getRealPath (req.getServletPath ());

	    // remove last component of the path, leaving only directories, and no final /
	    cwd = cwd.substring (0, cwd.lastIndexOf ('/'));
	}
    }

    /**
     * evaluate a tcl string and return the result. Any output from Tcl commands like
     * ns_adp_puts or ns_adp_include will override normal return value
     *
     * @param str the tcl string to evaluate.
     * @return the result of the tcl operation.
     */
    public String eval(String str) throws RuntimeException, IOException {
	if (str==null || str.length()==0)
	    return null;

	String ret;
	String adpout;
	
	try {
	    adpout = eval (cwd, str);
	}

	finally {
	    ret = flushResult ();
	}

	if (out!=null && adpout!=null)
	    out.write (adpout);

	return ret;
    }

    
    /**
     * Prepare ADP buffer and evaluate Tcl script
     *
     * @param cwd current working directory for ADP context
     * @param str Tcl script to evaluate
     * @return result from Tcl evaluation (not the ADP output from script)
     */
    static protected native String eval (String cwd, String str);

    /**
     * Clean ADP buffer and return its content in a string
     *
     * @return content of ADP buffer, possibly null
     */
    static protected native String flushResult ();

    /**
     * convert array of strings into Tcl list
     * Example:
     * <br><small>
     * <br><t>String elts[] = {"a", null, "b c", "d"};
     * <br><t>NsTcl tcl = new NsTcl();
     * <br><t>String list = tcl.listMerge (elts);
     * <br><t>int tclcount = tcl.eval ("llength [list " +list +"]");
     * </small>
     *
     * @param strings array of strings to merge into list
     * @return Tcl list in a string, null if strings array is null or empty.
     */
    public native String listMerge (String[] strings);

    /**
     * convert Tcl list into array of strings
     * Example:
     * <br><small>
     * <br><t>NsTcl tcl = new NsTcl();
     * <br><t>String names = tcl.eval ("glob *");
     * <br><t>String[] names2 = tcl.listSplit (names);
     * </small>
     *
     * @param list Tcl list in string form
     * @return array of elements, null if list is null or empty
     */
    public native String[] listSplit (String list);

  static 
  {
      try 
      {
	System.loadLibrary("nsjk2");
      } 
      catch (Throwable e) 
	{
	System.err.println("Cannot load library nsjk2. Exception: "+e.getClass().getName()+" Message: "+e.getMessage());
	}
  }  
}
