This directory contains both the native and java-side code for
ajp connectors.

Building
========

* Install tomcat(3.3, 4.0, 4.1). Install Apache(1.3, 2.0). Any
combination is fine, but j-t-c developers should have them all.

* Copy build.properties.sample to build.properties

* Edit build.properties to taste. In particular it's important to set
the paths to tomcat and apache.

* Run "ant". It'll build modules for all the detected containers.

* If your system have a current working version of libtool, run "ant native". 
  This will build the native connectors for the detected servers ( both jk
  and jk2).

  Alternatively, you can build using configure/make/dsp/apxs.

* Run "ant install". This will copy the files in the right location
  in your server/container. Optional: on unix systems you can create
  symlinks ( XXX script will be provided ) and avoid copying. 

Setting tomcat 3.3
==================

Restart tomcat. 

Setting tomcat 4.0
==================

To use the ajp13 connector for tomcat 4:

XXX Can we automate this ?

 1) add the following to server.xml:

     <Connector className="org.apache.ajp.tomcat4.Ajp13Connector"
               port="8009" acceptCount="10" debug="0"/>

Setting tomcat 4.1
==================

Restart tomcat. 

( XXX this is not completely implemented. For now we'll use the same 
mechanism as in 4.0 - i.e. add <Connector> in server.xml.  )

Configuring JK1
===============



Configuring JK2
===============


Building the tests
==================
( probably not working - the best test is to run watchdog/tester/3.3 test webapp )
( XXX is it ok to remove this ? )

* Download junit from http://www.junit.org (version 3.7 or greater is required).

* "ant test"