README for WebApp Library and Related Modules
---------------------------------------------

Notice
------

NO, IT DOES NOT RUN WITH WINDOWS (your images don't appear and the
whole thing hangs?) AND SINCE I DON'T USE NEITHER POSSESS A MICROSOFT
WINDOWS BASED MACHINE, THERE ARE NO CURRENT PLANS ON MAKING IT WORK 
OVER THERE (from my side).

If you want to do it (port it to Windows), those are few hints:
- Apache 1.3 on Windows is MultiThreaded
- Therefore the error coming out is a multi-threading issue
- The WARP socket is unique in WebApp
- You need to change that and build up a socket pool
- And add some locking mechanism to prevent race conditions

If you don't know what the above means, simply forget it (and I suggest
you doing a "format c:" and install Solaris 8 - my favorite preference
or throw out your x86 hardware out of in the dumpster and get an
Apple PowerMac with MacOS/X installed). If you are willing to do the
job, you're more than welcome to write to me or to the mailing list
with some patches.

How to obtain the WebApp and Apache Portable Runtime sources:
-------------------------------------------------------------

NOTE: If you downloaded a source distribution from our website or a
mirror (the file is called webapp-module...src.tar.gz) you don't need
to obtain any other file. Please follow this chapter only if you want
to obtain the latest CVS version of the sources.

Check out the module sources from CVS using the following commands:

    cvs -d :pserver:anoncvs@cvs.apache.org:/home/cvspublic login
    (Logging in to anoncvs@cvs.apache.org)
    CVS password: anoncvs
    cvs -d :pserver:anoncvs@cvs.apache.org:/home/cvspublic \
        checkout jakarta-tomcat-connectors/webapp

Once CVS downloads the WebApp module sources, we need to download the
APR (Apache Portable Runtime) sources. To do this simply:

    cd ./jakarta-tomcat-connectors/webapp
    cvs -d :pserver:anoncvs@cvs.apache.org:/home/cvspublic \
        checkout apr

When the APR sources are in place, we need to create the configure
script, configure both APR and the WebApp module and compile:

    ./support/buildconf.sh

To build the sources, now follow the steps in the next chapters.

How to build the tomcat-webapp.jar module from CVS sources:
------------------------------------------------

The tomcat-webapp.jar is build using Ant.
To get Ant see (http://jakarta.apache.org/ant/index.html).

To build the tomcat-webapp.jar you have to do the following:

* Copy build.properties.sample to build.properties

* Edit build.properties to taste.

* Run "ant". It'll build the tomcat-webapp.jar

* Copy the build/lib/tomcat-webapp.jar in the Tomcat server/lib directory.

* If using cygwin make sure of the following:
  - cygwin1.dll is in your PATH variable.
  - grep is installed (GNU grep, egrep and fgrep: Category Base).
  - sed is installed (sed: GNU sed stream editor: Category Base).

How to build the WebApp module from CVS sources:
------------------------------------------------

If you downloaded the CVS sources (as described above) or downloaded a
source distribution of the WebApp module, now all you need to do is build
the binary module for your platform. To do so, start by doing a:

    ./configure --with-apxs
    make

In case your platform needs some flags for APR just put them before the
configure. For example:
   ./support/buildconf.sh
   CC=/usr/bin/cc \
   CFLAGS=-DXTI_SUPPORT \
   ./configure --with-apxs=/opt/apache/bin/apxs

This will configure and build APR, and build the WebApp module for
Apache 1.3. The available options for the configure script are:

    --with-apxs[=FILE]
        Use the APXS Apache 1.3 Extension Tool. If this option is
        not specified, the Apache module will not be built (only the
        APR and WEBAPP libraries will be build).
        The "FILE" parameter specifies the full path for the apxs
        executable. If this is not specified apxs will be searched in
        the current path.

    --with-apr=DIR
        If you already have the APR sources lying around somewhere, and
        want to use them instead of checking them out from CVS, you can
        specify where these can be found.

    --enable-debug
        Enable compiled-in debugging output. Using this option the WebApp
        module, library, and Java counterpart will be built with debugging
        information. This will create a lot of output in your log files,
        and will kill performances, but it's a good starting poing when
        something goes wrong.

Once built, the DSO module will be found in the webapp/apache-1.3 directory.

To install it  copy the mod_webapp.so file in your Apache 1.3 libexec
directory, and add the following lines to httpd.conf:

    LoadModule webapp_module [path to mod_webapp.so]
    AddModule mod_webapp.c

To check out if everything is correctly configured, issue the following:

    apachectl configtest

If the output of the apachectl command doesn't include "Syntax OK", something
went wrong with the build process. Please report that through our bug tracking
database at <http://nagoya.apache.org/bugzilla> or to the Tomcat developers
mailing list <mailto:tomcat-dev@jakarta.apache.org>

Have fun...

    Pier <pier@betaversion.org>
