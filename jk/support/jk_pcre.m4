dnl
dnl Copyright 1999-2004 The Apache Software Foundation
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License");
dnl you may not use this file except in compliance with the License.
dnl You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.
dnl



AC_DEFUN(
  [JK_PCRE],
  [
    AC_ARG_WITH(pcre,
      [  --with-pcre              Build pcre support],
      [
		case "${withval}" in
		  y | yes | true) use_pcre=true ;;
		  n | no | false) use_pcre=false ;;
	    *) use_pcre=true ;;
	      esac

		if ${TEST} ${use_pcre} ; then
		  HAS_PCRE="-I${includedir} -DHAS_PCRE"
		  PCRE_LIBS="-L${libdir} -lpcre -lpcreposix"
		fi
      ])
  ])

dnl vi:set sts=2 sw=2 autoindent:

