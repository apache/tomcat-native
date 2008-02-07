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

This directory contains both the native and java-side code for
ajp connectors.

Building
========

* The java part is included in each tomcat build.

Building native part:

* You need to install the web server and go the native subdirectory
  and process the instructions in its BUILDING.txt.

* Read the BUILD.txt 

Building the doc:

* Go to the subdirection xdocs and use ant
  ant
  The documentation will be in build/docs.
