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
 * Native implementation of Java-to-Tcl calls
 */

#define USE_TCL8X

#include <ns.h>
#include "jni_nstcl.h"

/* Tcl variable to store ADP output */
#define TCLVARNAME "org.apache.nsjk2.NsTcl.adpout"

/* TLS slot to keep allocated Tcl interpreter for threads that have no active HTTP connection
   Gets initialized from nsjk2.c
*/
Ns_Tls tclInterpData;


/*
 *----------------------------------------------------------------------
 *
 * throwByName --
 *
 *      Throw a named java exception.  Null terminated varargs are passed as
 *      the exception message.
 *
 * Results:
 *      Exception is returned to java.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void throwByName(JNIEnv *env, const char *name, ...)
{
    Ns_DString  dsPtr;
    jclass      cls;
    va_list     ap;
    char       *s;

    va_start(ap, name);
    Ns_DStringInit(&dsPtr);
    Ns_DStringVarAppend(&dsPtr, name, ": ", NULL);

    while ((s = va_arg(ap, char *)) != NULL) {
        Ns_DStringAppend(&dsPtr, s);
    }
    va_end(ap);

    cls = (*env)->FindClass(env, name);
    if (cls == NULL) {
        (*env)->ExceptionClear(env);
        cls = (*env)->FindClass(env, "java/lang/ClassNotFoundException");
    }

    if (cls != NULL) {
        (*env)->ThrowNew(env, cls, dsPtr.string);
        (*env)->DeleteLocalRef(env, cls);
    }

    Ns_DStringFree(&dsPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * checkForException --
 *
 *      Check for an exception in java and clear it.  The exception message
 *      will appear in the aolserver log.
 *
 * Results:
 *      Returns true if exception occured.
 *
 * Side effects:
 *      Exception message will appear in the aolserver log file.
 *
 *----------------------------------------------------------------------
 */
static int checkForException(JNIEnv *env) 
{
  jobject exception = (*env)->ExceptionOccurred(env);

  if (exception != NULL) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, exception);

    return JNI_TRUE;
  }

  return JNI_FALSE;
}


/*
 *----------------------------------------------------------------------
 * Thread cleanup callback. Marks for deletion Tcl interpreter 
 * previously allocated for the thread that is exiting
 *
 * Parameters:
 *   arg - pointer registered for callback (a Tcl_Interp object)
 *
 *----------------------------------------------------------------------
 */
void tclInterpDataDtor (void* arg) {
    Ns_TclDestroyInterp ((Tcl_Interp*) arg);
}


/*
 *----------------------------------------------------------------------
 *
 * GetTclInterp --
 *
 *	Get the connection interpreter if the connection is not null
 *      otherwise allocate a new interpreter. The new interpreter
 *      is not affiliated with any virtual servers since JVM is shared
 *      between all virtual servers. The example of non-server interpreter
 *      would be allocated is a new Java thread in a servlet, created to
 *      run independent of any connection and evaluating Tcl scripts.
 *
 * Results:
 *	Returns an interpreter.
 *
 * Side effects:
 *	A new interpreter is allocated if there is no connection.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Interp *GetTclInterp(Ns_Conn *conn)
{
    Tcl_Interp* interp;  

    if (conn == NULL) {
        /* allocate/fetch and 0-filled chunk of memory */
        interp= (Tcl_Interp*) Ns_TlsGet (&tclInterpData);

        if (interp == NULL) {
            interp = Ns_TclAllocateInterp(NULL);
	    Ns_TlsSet (&tclInterpData, interp);
	}

	return interp;

    } else
        return Ns_GetConnInterp(conn);
}



void handleErr(JNIEnv *env, char *msg) 
{
    checkForException(env);
    throwByName(env, "java/lang/RuntimeException", msg, NULL);
    Ns_Log(Error, msg);
}



/*
 * Class:     org_apache_nsjk2_NsTcl
 * Method:    eval
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 *
 *	JNI method for evaling tcl from within java.
 *
 * Parameters:
 *    cwd - current working directory for ADP context
 *    script - Tcl script to evaluate
 *
 * Results:
 *      ADP output, whose value depends upon existance of HTTP connection.
 *	connection exists: ADP output string is converted to java string and passed back to java.
 *      doesn't: NULL
 *
 * Side effects:
 *	affects the current tcl connection interpreter, stores Tcl result in a variable.
 *
 *----------------------------------------------------------------------
 */

JNIEXPORT jstring JNICALL Java_org_apache_nsjk2_NsTcl_eval (JNIEnv* env, 
							    jclass classNsTcl, 
							    jstring cwd, 
							    jstring script)
{
    Tcl_Interp* interp;
    jstring     jstr=NULL;
    const char* cscript=NULL;
    const char* ccwd=NULL;
    int         result;
    Ns_Conn*    conn = Ns_GetConn();
    Ns_DString  tclcmd;
    const char* ctclcmd;

    if (script == NULL)
	return NULL;

    /* Obtain Tcl interp for this thread */
    interp = GetTclInterp(conn);
    if (interp == NULL) {
	handleErr (env, "Cannot get Tcl interpreter");
	return NULL;
    }
    
    cscript = (*env)->GetStringUTFChars(env, script, JNI_FALSE);
    if(cscript == NULL) {
	handleErr(env,"couldn't get script");
	return NULL;
    }

    /* If conn==NULL, it means Tcl script is being evaluated in the interpreter
       which doesn't have ADP environment. This helps simplify Tcl command
    */

    if (conn!=NULL && cwd!=NULL) {
	ccwd = (*env)->GetStringUTFChars (env, cwd, JNI_FALSE);
	if (ccwd == NULL) {
	    handleErr (env, "couldn't get cwd arg");
	    (*env)->ReleaseStringUTFChars(env, script, cscript);
	    return NULL;
	}
    }
	    
    /* Create ADP command string if connection exists */
    if (conn != NULL) {
	Ns_DStringInit (&tclcmd);
	Ns_DStringAppend (&tclcmd, "ns_adp_parse -savedresult " TCLVARNAME);
    
	if (ccwd != NULL)
	    Ns_DStringVarAppend (&tclcmd, " -cwd ", ccwd, NULL);

	Ns_DStringVarAppend (&tclcmd, " -string {<% ", cscript, " %>}", NULL);

	ctclcmd = Ns_DStringValue (&tclcmd);

    } else {
	ctclcmd = cscript;
    }
    
    result = Tcl_Eval(interp, ctclcmd);

    /* cleanup */
    if (conn != NULL)
	Ns_DStringFree (&tclcmd);

    if (cscript != NULL)
        (*env)->ReleaseStringUTFChars(env, script, cscript);

    if (ccwd != NULL)
        (*env)->ReleaseStringUTFChars(env, cwd, ccwd);

    if (result == TCL_ERROR) {
        handleErr (env, interp->result);
	Tcl_ResetResult(interp);
	return NULL;
    }

    /* If no current connection, defer return of result to flushResult method */
    if (conn!=NULL && interp->result!=NULL && interp->result[0]!='\0') {
        jstr = (*env)->NewStringUTF(env, interp->result);
	if (jstr == NULL) {
	    handleErr(env, "out of memory");
	    return NULL;
	}
    }

    return jstr;
}


/*
 * Class:     org_apache_nsjk2_NsTcl
 * Method:    flushResult
 * Signature: ()Ljava/lang/String;
 *
 * Returns:
 *    Result from Tcl command, which is
 *    if connection exists: value of special Tcl variable as Java string, unset the variable
 *    no connection: regular Tcl result
 *
 */
JNIEXPORT jstring JNICALL Java_org_apache_nsjk2_NsTcl_flushResult (JNIEnv* env, 
								   jclass classNsTcl)
{
    jstring     jstr = NULL;
    Tcl_Interp* interp;
    const char* tclval;
    Ns_Conn*    conn = Ns_GetConn ();

    interp = GetTclInterp (conn);
    if (interp == NULL) {
	handleErr (env, "Cannot get Tcl interpreter");
	return NULL;
    }

    if (conn != NULL) {
	tclval = Tcl_GetVar (interp, TCLVARNAME, 0);
	/* don't care if variable doesn't exist - means no result returned */
	if (tclval != NULL) {
		
	    if (tclval[0] != '\0') {
		jstr = (*env)->NewStringUTF (env, tclval);
		if (jstr == NULL)
		    handleErr (env, "out of memory");
	    }

	    Tcl_UnsetVar (interp, TCLVARNAME, 0);
	}

    } else if (interp->result!=NULL && interp->result[0]!='\0') {
        jstr = (*env)->NewStringUTF(env, interp->result);
	if (jstr == NULL) {
	    handleErr(env, "out of memory");
	    return NULL;
	}
    }

    return jstr;
}


/*
 * Class:     org_apache_nsjk2_NsTcl
 * Method:    listMerge
 * Signature: ([Ljava/lang/String;)Ljava/lang/String;
 *
 * JNI method for merging array of strings into properly formatted Tcl list
 *
 * Results: Tcl list in a string, null if null or empty array.
 */

JNIEXPORT jstring JNICALL Java_org_apache_nsjk2_NsTcl_listMerge (JNIEnv* env, 
								 jobject jvm, 
								 jobjectArray strings) {

    jsize len = (strings==NULL? 0 : (*env)->GetArrayLength (env, strings));
    jstring jstrrefs[len];
    const char* cstrarray[len];
    jsize i;
    char* liststr;
    jstring res = NULL;

    if (len != 0) {
        // Convert Java array into C array
        for (i=0; i<len; i++) {
	    jstrrefs[i] = (*env)->GetObjectArrayElement (env, strings, i);
	    cstrarray[i] = (jstrrefs[i]==NULL? NULL : (*env)->GetStringUTFChars (env, jstrrefs[i], JNI_FALSE));
	}

	liststr = Tcl_Merge (len, cstrarray);

	if (liststr != NULL)
	    res = (*env)->NewStringUTF (env, liststr);

	if (liststr==NULL || res==NULL)
	    handleErr (env, "out of memory");

	// Release references
	if (liststr != NULL)
	    Tcl_Free (liststr);

	for (i=0; i<len; i++) {
	    if (jstrrefs[i] != NULL) {
	        (*env)->ReleaseStringUTFChars (env, jstrrefs[i], cstrarray[i]);
		(*env)->DeleteLocalRef (env, jstrrefs[i]);
	    }
	}
    }

    return res;
}



/*
 * Class:     org_apache_nsjk2_NsTcl
 * Method:    listSplit
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 *
 * JNI method for splitting Tcl list in a string into array of strings
 *
 * Results: array of strings, null if null or empty argument
 */
JNIEXPORT jobjectArray JNICALL Java_org_apache_nsjk2_NsTcl_listSplit (JNIEnv* env, 
								      jobject jvm, 
								      jstring list) {
    const char* cstrlist = NULL;
    Tcl_Interp* interp;
    jclass clsString = NULL;
    int argc =0;
    const char** argv = NULL;
    jobjectArray jarray = NULL;
    jsize i;
    jstring jstr;
    int tclrc = TCL_ERROR;

    if (list==NULL || (*env)->GetStringUTFLength (env, list)==0)
        return NULL;

    clsString = (*env)->FindClass (env, "java/lang/String"); 

    if (clsString == NULL)
        handleErr (env, "Cannot find class java.lang.String");

    else {
        cstrlist = (*env)->GetStringUTFChars (env, list, JNI_FALSE);
	interp = GetTclInterp(Ns_GetConn());
	tclrc = Tcl_SplitList (interp, cstrlist, &argc, &argv);

	if (tclrc == TCL_ERROR) {
	    handleErr (env, interp->result);
	    Tcl_ResetResult (interp);
	}
    }

    if (tclrc == TCL_OK) {
	jarray = (*env)->NewObjectArray (env, argc, clsString, NULL);

	if (jarray == NULL)
	    handleErr (env, "out of memory");

	else {
	    for (i=0; i<argc; i++) {
		jstr = (*env)->NewStringUTF (env, argv[i]);
		if (jstr == NULL) {
		    handleErr (env, "out of memory");
		    break;
		}

		(*env)->SetObjectArrayElement (env, jarray, i, jstr);
	    }
	}

    }
	
    if (cstrlist != NULL)
        (*env)->ReleaseStringUTFChars (env, list, cstrlist);

    if (clsString != NULL)
        (*env)->DeleteLocalRef (env, clsString);

    if (argv != NULL)
        Tcl_Free ((char*) argv);

    return jarray;
}
