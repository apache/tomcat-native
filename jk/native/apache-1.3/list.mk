## Object needed for mod_jk for Apache-1.3
APACHE_OBJECTS= ${JK}/jk_ajp12_worker${OEXT} ${JK}/jk_connect${OEXT} \
                ${JK}/jk_msg_buff${OEXT} ${JK}/jk_util${OEXT} \
                ${JK}/jk_ajp13${OEXT} ${JK}/jk_pool${OEXT} \
                ${JK}/jk_worker${OEXT} ${JK}/jk_ajp13_worker${OEXT} \
                ${JK}/jk_lb_worker${OEXT} ${JK}/jk_sockbuf${OEXT} \
                ${JK}/jk_map${OEXT} ${JK}/jk_uri_worker_map${OEXT} \
                ${JK}/jk_ajp14${OEXT} ${JK}/jk_ajp14_worker${OEXT} \
                ${JK}/jk_md5${OEXT} ${JK}/jk_jni_worker${OEXT} \
                ${JK}/jk_ajp_common${OEXT}
