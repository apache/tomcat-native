README for WebApp Library and Related Modules
---------------------------------------------

Prerequisites:
--------------

1) The Apache Portable Runtime library:
      The APR library is an abstraction layer among several operating systems,
      and it's used to simplify the porting operation of the WebApp Library and
      its related modules to different environments.

      This doesn't mean that WebApp modules will require the Apache Web Server
      2.0 to run, but only that both WebApp and Apache 2.0 are built on the
      same OS abstraction layer (if Apache 2.0 can run on your operating system
      also WebApp can)
      
      There is not yet a final release of APR, but you can download the lastest
      development snapshot from <http://apr.apache.org/>

2) The Web Server of your choice:
      Even if right now the only implemented web server plug-in is mod_webapp
      for Apache 1.3, WebApp Library Modules can be written for any web server.
      Currently, for Apache 1.3, you will need a fully installed version of
      the web server with DSO (loadable modules) support and APXS, the Apache
      modules compilation utility.

Configuration:
--------------

If you are building this from CVS, you will need to first execute 
./buildconf.sh to build the "configure" script. This assumes that you have 
autoconf 2.13 installed on your machine already.

Simply issue a "./configure --help" to see all the supported AutoConf 
parameters. Example:

./configure --with-apr=/usr/local --with-apxs
