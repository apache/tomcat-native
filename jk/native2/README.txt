README for JK2
--------------

The  newest  JK2  is  a  rewrite of  JK.  The  native  part  has been  completly
restructured and the configuration has been  simplified a lot. Even if it  works
with Apache 1.3,  JK2 has been  developed with Apache  2.0 in mind,  and thus is
better suited for multi-threaded servers like IIS, NES/iPlanet.

JK2 has a  better separation between  protocol and physical  layer. As such  JK2
support fast unix-socket, and could be extended to support others communications
channels. Better it's suited for JNI and JDK 1.4 fast IO APIs.


How to obtain the JK2 and Apache Portable Runtime sources:
----------------------------------------------------------

NOTE: If you downloaded a source distribution from our website or a mirror  (the
file is called jakarta-tomcat-connectors-jk2-X.X.X.src.tar.gz) you don't need to
obtain any other file. Please follow this chapter only if you want to obtain the
latest CVS version of the sources.

Check out the module sources from CVS using the following commands:

    cvs -d :pserver:anoncvs@cvs.apache.org:/home/cvspublic login
    (Logging in to anoncvs@cvs.apache.org)
    CVS password: anoncvs
    cvs -d :pserver:anoncvs@cvs.apache.org:/home/cvspublic \
        checkout jakarta-tomcat-connectors

Once  CVS downloads  the jakarta-tomcat-connectors  module sources,  we need  to
download the  APR (Apache  Portable Runtime)  sources. To  do this  simply use a
release version of APR:

    - You can download it from http://www.apache.org/dist/apr/.
    - The file is called the file is apr-X.X.X.tar.gz

You will also need the APR-UTIL (a companion library to APR). To do this  simply
use a release version of APR:

    - You can download it from http://www.apache.org/dist/apr/.
    - The file is called the file is apr-util-X.X.X.tar.gz

When the APR sources are in place,  we need to create the configure scripts.  It
is done by running the command:

    ./support/buildconf.sh


WARNING:
---------

The JK2  should be  considered initial-release  quality code.   It has  not been
subjected to the  same stresses on  its stability and  security that the  mod_jk
releases  have  enjoyed,  so  there is  a  greater  possibility  of undiscovered
vulnerabilities to stability or security.
