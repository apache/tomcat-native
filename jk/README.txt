To use the ajp13 connector for tomcat 4:

 1) build ajp.jar
 2) copy ajp.jar to $TOMCAT4_HOME/{build|dist}/server/lib
 3) add the following to server.xml:

     <Connector className="org.apache.ajp.tomcat4.Ajp13Connector"
               port="8009" acceptCount="10" debug="10"/>
