# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# That an extract of what is in APR.
#

# Compile commands
#VPATH=.:../common
COMPILE      = $(CC) $(CFLAGS)
LT_COMPILE   = $(LIBTOOL) --mode=compile $(COMPILE) -c $< -o $@

# Implicit rules for creating outputs from input files

.SUFFIXES:
.SUFFIXES: .c .lo .o .slo .s

.c.o:
	$(COMPILE) -c $<

.s.o:
	$(COMPILE) -c $<

.c.lo:
	$(LT_COMPILE)

.s.lo:
	$(LT_COMPILE)

.c.slo:
	$(SH_COMPILE)

