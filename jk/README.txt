This directory contains both the native and java-side code for
ajp connectors.

Building
========
Build tomcat-util.jar in jakarta-tomcat-connectors/util.
Copy build.properties.sample to build.properties.
Edit build.properties to taste.
Build ajp.jar by running ant.  The default target creates ./build/lib/ajp.jar.

Tomcat 4
========
To use the ajp13 connector for tomcat 4:

 1) build tomcat-util.jar (in ../util)
 1) build ajp.jar (by running ant)
 2) copy ajp.jar, tomcat-util.jar to $TOMCAT4_HOME/{build|dist}/server/lib
 3) add the following to server.xml:

     <Connector className="org.apache.ajp.tomcat4.Ajp13Connector"
               port="8009" acceptCount="10" debug="0"/>
