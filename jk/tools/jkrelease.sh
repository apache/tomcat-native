#/bin/sh

# You can change JKTAG and JKVER to desired CVS tag.
JKTAG="HEAD"
JKEXT="1.2.9"
JKVER="-${JKEXT}-src"
export CVSROOT=:pserver:anoncvs@cvs.apache.org:/home/cvspublic
cvs export -r ${JKTAG} -d jakarta-tomcat-connectors${JKVER} jakarta-tomcat-connectors
# Remove all files that are not part of jk release
rm -rf jakarta-tomcat-connectors${JKVER}/ajp
rm -rf jakarta-tomcat-connectors${JKVER}/coyote
rm -rf jakarta-tomcat-connectors${JKVER}/http11
rm -rf jakarta-tomcat-connectors${JKVER}/jk/java
rm -rf jakarta-tomcat-connectors${JKVER}/jk/jkant
rm -rf jakarta-tomcat-connectors${JKVER}/jk/native2
rm -rf jakarta-tomcat-connectors${JKVER}/jk/test
rm -rf jakarta-tomcat-connectors${JKVER}/jni
rm -rf jakarta-tomcat-connectors${JKVER}/juli
rm -rf jakarta-tomcat-connectors${JKVER}/naming
rm -rf jakarta-tomcat-connectors${JKVER}/procrun
rm -rf jakarta-tomcat-connectors${JKVER}/util
rm -rf jakarta-tomcat-connectors${JKVER}/webapp

# Build documentation.
cd jakarta-tomcat-connectors${JKVER}/jk/xdocs
ant
cd ../native
./buildconf.sh
cd ../../../
tar cvf jakarta-tomcat-connectors${JKVER}.tar jakarta-tomcat-connectors${JKVER}
gzip jakarta-tomcat-connectors${JKVER}.tar
zip -9 -r jakarta-tomcat-connectors${JKVER}.zip jakarta-tomcat-connectors${JKVER}
# Create detatched signature
gpg -ba jakarta-tomcat-connectors${JKVER}.tar.gz
gpg -ba jakarta-tomcat-connectors${JKVER}.zip
