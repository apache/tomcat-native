## Object needed for mod_jk for Apache
COMMON_OBJECTS= \
${JK}jk_channel_jni${OEXT}\
${JK}jk_channel_socket${OEXT}\
${JK}jk_channel_un${OEXT}\
${JK}jk_config${OEXT}\
${JK}jk_endpoint${OEXT}\
${JK}jk_env${OEXT}\
${JK}jk_handler_logon${OEXT}\
${JK}jk_handler_response${OEXT}\
${JK}jk_logger_file${OEXT}\
${JK}jk_map${OEXT}\
${JK}jk_md5${OEXT}\
${JK}jk_msg_ajp${OEXT}\
${JK}jk_mutex${OEXT}\
${JK}jk_nwmain${OEXT}\
${JK}jk_objCache${OEXT}\
${JK}jk_pool${OEXT}\
${JK}jk_registry${OEXT}\
${JK}jk_requtil${OEXT}\
${JK}jk_shm${OEXT}\
${JK}jk_uriEnv${OEXT}\
${JK}jk_uriMap${OEXT}\
${JK}jk_vm_default${OEXT}\
${JK}jk_worker_ajp13${OEXT}\
${JK}jk_workerEnv${OEXT}\
${JK}jk_worker_jni${OEXT}\
${JK}jk_worker_lb${OEXT}\
${JK}jk_worker_run${OEXT}\
${JK}jk_worker_status${OEXT}

COMMON_APR_OBJECTS= \
${JK}jk_channel_apr_socket${OEXT} \
${JK}jk_pool_apr${OEXT}
