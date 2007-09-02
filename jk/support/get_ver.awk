BEGIN {

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


  # fetch mod_jk version numbers from input file and writes them to STDOUT

  while ((getline < ARGV[1]) > 0) {
    if (match ($0, /^#define JK_VERMAJOR [^"]+/)) {
      jk_ver_major = substr($3, 1, length($3));
    }
    else if (match ($0, /^#define JK_VERMINOR [^"]+/)) {
      jk_ver_minor = substr($3, 1, length($3));
    }
    else if (match ($0, /^#define JK_VERFIX [^"]+/)) {
      jk_ver_fix = substr($3, 1, length($3));
    }
    else if (match ($0, /^#define JK_VERISRELEASE [^"]+/)) {
      jk_ver_isrelease = substr($3, 1, length($3));
    }
    else if (match ($0, /^#define JK_VERBETA [^"]+/)) {
      jk_ver_isbeta = substr($3, 1, length($3));
    }
    else if (match ($0, /^#define JK_BETASTRING [^"]+/)) {
      jk_ver_betastr = substr($3, 2, length($3) - 2);
    }
  }
  jk_ver = jk_ver_major "," jk_ver_minor "," jk_ver_fix;
  jk_ver_str = jk_ver_major "." jk_ver_minor "." jk_ver_fix;
  if (jk_ver_isrelease != 1) {
    jk_ver_str = jk_ver_str "-dev";
  }
  if (jk_ver_isbeta == 1) {
    jk_ver_str = jk_ver_str "-beta-" jk_ver_betastr;
  }
  
  # fetch Apache version numbers from input file and writes them to STDOUT

  if (ARGV[2]) {
    if (match (ARGV[2], /ap_release.h/)) {
      while ((getline < ARGV[2]) > 0) {
        if (match ($0, /^#define AP_SERVER_MAJORVERSION "[^"]+"/)) {
          ap_ver_major = substr($3, 2, length($3) - 2);
        }
        else if (match ($0, /^#define AP_SERVER_MINORVERSION "[^"]+"/)) {
          ap_ver_minor = substr($3, 2, length($3) - 2);
        }
        else if (match ($0, /^#define AP_SERVER_PATCHLEVEL/)) {
          ap_ver_str_patch = substr($3, 2, length($3) - 2);
          if (match (ap_ver_str_patch, /[0-9][0-9]*/)) {
            ap_ver_patch = substr(ap_ver_str_patch, RSTART, RLENGTH); 
          }
        }
      }
      ap_ver_str = ap_ver_major "." ap_ver_minor "." ap_ver_str_patch;
    }
    if (match (ARGV[2], /httpd.h/)) {
      while ((getline < ARGV[2]) > 0) {
        if (match ($0, /^#define SERVER_BASEREVISION "[^"]+"/)) {
          ap_ver_str = substr($3, 2, length($3) - 2);
        }
      }
    }
    print "AP_VERSION_STR = " ap_ver_str "";
  }

  print "JK_VERSION = " jk_ver "";
  print "JK_VERSION_STR = " jk_ver_str "";

}
