README for WebApp Library and Related Modules
---------------------------------------------

How to build the WebApp module from CVS sources:
------------------------------------------------

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

    ./buildconf.sh
    ./configure --with-apxs
    make

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

Once built, the DSO module will be built in the webapp/apache-1.3 directory.
To install it copy the mod_webapp.so file in your Apache 1.3 libexec
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

    Pier <pier.fumagalli@sun.com>
