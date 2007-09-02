dnl
dnl Licensed to the Apache Software Foundation (ASF) under one or more
dnl contributor license agreements.  See the NOTICE file distributed with
dnl this work for additional information regarding copyright ownership.
dnl The ASF licenses this file to You under the Apache License, Version 2.0
dnl (the "License"); you may not use this file except in compliance with
dnl the License.  You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.
dnl

dnl copied from httpd-2.0/os/config.m4
dnl OS changed to OS_APACHE and OS_DIR to OS_APACHE_DIR

AC_MSG_CHECKING(for target platform)

#PLATFORM=`${CONFIG_SHELL-/bin/sh} $ac_config_guess`
PLATFORM=$host

case "$PLATFORM" in
*beos*)
  OS_APACHE="beos"
  OS_APACHE_DIR=$OS_APACHE
  ;;
*pc-os2_emx*)
  OS_APACHE="os2"
  OS_APACHE_DIR=$OS_APACHE
  ;;
bs2000*)
  OS_APACHE="unix"
  OS_APACHE_DIR=bs2000  # only the OS_APACHE_DIR is platform specific.
  ;;
*)
  OS_APACHE="unix"
  OS_APACHE_DIR=$OS_APACHE;;
esac

AC_MSG_RESULT($OS_APACHE)
