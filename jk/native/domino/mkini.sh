#!/bin/sh
echo "log_file=$1/logs/domino.log"
echo "log_level=debug"
echo "worker_file=$1/conf/workers.properties"
echo "worker_mount_file=$1/conf/uriworkermap.properties"
echo "tomcat_start=$1/bin/tomcat.sh start"
echo "tomcat_stop=$1/bin/tomcat.sh stop"
