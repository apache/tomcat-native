  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

README for tomcat-connector

$Id$

Please see doc/mod_jk-howto.html for more verbose instructions

* What is tomcat-connector ?

tomcat-connector is a new project to release web-servers connector
for the Tomcat servlet engine.

This project didn't start from null since it reuse the latest code from
mod_jk for the native parts and tomcat 3.3 for the java side of the 
force.

From mod_jk we gain :

* A connector (plugin) for many Web Server, including
  Apache HTTP Server, Netscape/IPLanet NES and Microsoft IIS
  It also support JNI (and seems to used at least by IBM under 
  AS/400 for Apache/WebSphere connectivity).
  
* Fault-tolerance and load-balancing. mod_jk use the concept of 
  workers which handle a particular request, and a special worker
  , the lb worker, is a group (cluster ?) of physical workers.

* Direct access to Tomcat 3.2/3.3/4.0/4.1 servlet engine via ajp13
  protocol. 

* OK, then how do I build web-connector ? Just take a look at BUILDING.txt
