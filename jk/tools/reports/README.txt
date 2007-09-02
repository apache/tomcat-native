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

This directory contains perl scripts which can be used to generate
statistics for tomcat requests and errors logged by mod_jk.

See the comments in the scripts for more details.

A great deal of statistical data is generated but at this time
only long term trend graphs are being created and no reports.
This is only a start.  Many more graphs and reports could be
generated from the data. Please consider contributing back any
new reports or graphs you create.  Thanks.

Requires the following perl modules and libraries:

GD 1.8.x graphics library http://www.boutell.com/gd/.
GD 1.4.x perl module.
GD Graph perl module.
GD TextUtil perl module.
StatisticsDescriptive perl module.
