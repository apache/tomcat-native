#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/un.h> /* for sockaddr_un */
#include <unistd.h>

#include <jni.h>

/*
 * Native routines for
 * package org.apache.jk.common
 */

/**
 * createSocketNative: Creates a AF_UNIX socketserver.
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocketServer_createSocketNative (
    JNIEnv      *env,
    jobject     ignored,
    jstring     filename
) {
    int         sd;
    const char  *real_filename;
    jboolean    flag;
    struct sockaddr_un unix_addr;
    mode_t omask;
    int rc;

    // Convert java encoding in machine one.
    real_filename = (*env)->GetStringUTFChars (env, filename, &flag);
    if (real_filename == 0)
        return -EINVAL;

    // remove the exist socket.
    if (unlink(real_filename) < 0 && errno != ENOENT) {
        // The socket cannot be remove... Well I hope that no problems ;-)
    }

    // create the socket.
    sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd<0)
        return -errno;

    memset(&unix_addr, 0, sizeof(unix_addr));
    unix_addr.sun_family = AF_UNIX;
    strcpy(unix_addr.sun_path, real_filename);

    omask = umask(0117); /* so that only Apache can use socket */
    rc = bind(sd, (struct sockaddr *)&unix_addr, sizeof(unix_addr));
    umask(omask); /* can't fail, so can't clobber errno */
    if (rc<0)
       return -errno;

    if (listen(sd, 100) < 0) {
        return -errno;
    }

    (*env)->ReleaseStringUTFChars (env, filename, real_filename);

    return sd;
}

/**
 * accept(). Accepts incomming calls on socket.
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocketServer_acceptNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        sd
) {
    struct sockaddr_un unix_addr;
    int sd2;
    int len;

    len = sizeof(unix_addr);
    sd2 = accept(sd, (struct sockaddr *)&unix_addr, &len);
    if (sd2<0)
        return - errno;
    return(sd2);
    
}

/**
 * set the socket timeout (read/write) or accept() ?
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocketServer_setSoTimeoutNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        sd
) {
    // setsockopt or setitimer?
}

/**
 * close the socket (remove the file?)
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocketServer_closeNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        sd
) {
    close(sd);
}

/**
 * setSoLinger
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocket_setSoLingerNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        sd,
    jint        l_onoff,
    jint        l_linger
) {
    struct linger {
        int   l_onoff;    /* linger active */
        int   l_linger;   /* how many seconds to linger for */
        } lin;
    lin.l_onoff = l_onoff;
    lin.l_linger = l_linger;
    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin))<0)
        return -errno;
    return 0;
}

/**
 * read from the socket.
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocketIn_readNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        fd,
    jbyteArray  buf,
    jint        len
) {
    int retval;
    jbyte *buffer;
    jboolean    isCopy;

    buffer = (*env)->GetByteArrayElements (env, buf, &isCopy);

    retval = read(fd,buffer,len);

    (*env)->ReleaseByteArrayElements (env, buf, buffer, 0);

    return retval;
}

/**
 * write to the socket.
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_common_UnixSocketOut_writeNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        fd,
    jbyteArray  buf,
    jint        len
) {
    int retval;
    jbyte *buffer;
    jboolean    isCopy;

    buffer = (*env)->GetByteArrayElements (env, buf, &isCopy);

    retval = write(fd,buffer,len);

    (*env)->ReleaseByteArrayElements (env, buf, buffer, 0);

    return retval;

}
