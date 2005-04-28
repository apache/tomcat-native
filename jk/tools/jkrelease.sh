#/bin/sh

# You can change JKTAG and JKEXT to desired CVS tag and version
JKTAG="HEAD"
JKEXT="current"
JKVER="-${JKEXT}-src"
JKCVST="jakarta-tomcat-connectors"
JKDIST=${JKCVST}${JKVER}
rm -rf ${JKDIST}
rm -f ${JKDIST}.*
export CVSROOT=:pserver:anoncvs@cvs.apache.org:/home/cvspublic
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/KEYS
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/LICENSE
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/NOTICE
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/common
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/jk/BUILD.txt
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/jk/conf
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/jk/native
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/jk/support
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/jk/tools
cvs export -N -r ${JKTAG} -d ${JKDIST} ${JKCVST}/jk/xdocs
mv ${JKDIST}/${JKCVST}/* ${JKDIST}/
# Remove extra directories and files
rm -f ${JKDIST}/jk/native/build.xml
rm -f ${JKDIST}/jk/conf/jk2.*
rm -f ${JKDIST}/jk/conf/workers2.*
rm -f ${JKDIST}/jk/conf/*.manifest
rm -f ${JKDIST}/jk/conf/*.xml
rm -f ${JKDIST}/jk/native/CHANGES.txt
rm -rf ${JKDIST}/${JKCVST}
rm -rf ${JKDIST}/jk/*/.cvsignore
rm -rf ${JKDIST}/jk/*/*/.cvsignore

# Build documentation.
cd ${JKDIST}/jk/xdocs
ant
# Export text docs
cd ../native
W3MOPTS="-dump -cols 80 -t 4 -S -O iso-8859-1 -T text/html"
w3m ${W3MOPTS} ../build/docs/install/printer/apache1.html >BUILDING
w3m ${W3MOPTS} ../build/docs/install/printer/apache2.html >>BUILDING
w3m ${W3MOPTS} ../build/docs/install/printer/iis.html >>BUILDING
w3m ${W3MOPTS} ../build/docs/printer/changelog.html >CHANGES
w3m ${W3MOPTS} ../build/docs/news/printer/20050101.html >NEWS
w3m ${W3MOPTS} ../build/docs/news/printer/20041100.html >>NEWS
rm -rf ../build
rm -rf ../xdocs/jk2
./buildconf.sh
cd ../../../
tar cvf ${JKDIST}.tar ${JKDIST}
gzip ${JKDIST}.tar
zip -9 -r ${JKDIST}.zip ${JKDIST}
# Create detatched signature
gpg -ba ${JKDIST}.tar.gz
gpg -ba ${JKDIST}.zip
