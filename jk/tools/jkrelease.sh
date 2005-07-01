#/bin/sh

# You can change JKTAG and JKEXT to desired CVS tag and version
JKTAG="HEAD"
JKEXT="current"
JKVER="-${JKEXT}-src"
JKCVST="jakarta-tomcat-connectors"
JKDIST=${JKCVST}${JKVER}
rm -rf ${JKDIST}
rm -f ${JKDIST}.*
#
# To use a proxy you need  a recent version of cvs (I have tried with 1.12.9)
if [ -z "$http_proxy" ]
then
  CVSROOT=:pserver:anoncvs@cvs.apache.org:/home/cvspublic
else
  PRSTRING=`echo $http_proxy | tr '/' ' ' | tr ':' ' '`
  HOST=`echo $PRSTRING | awk ' { print $2 } '`
  PORT=`echo $PRSTRING | awk ' { print $3 } '`
  CVSROOT=":pserver;proxy=$HOST;proxyport=$PORT:anoncvs@cvs.apache.org:/home/cvspublic"
fi
export CVSROOT
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

# Check for links or w3m
W3MOPTS="-dump -cols 80 -t 4 -S -O iso-8859-1 -T text/html"
LNKOPTS="-dump"
failed=true
for tool in `echo "w3m links"`
do
  echo "tool: ${tool}"
  found=false
  for dir in `echo ${PATH} | sed 's!^:!.:!;s!:$!:.!;s!::!:.:!g;s!:! !g'`
  do
    if [ -x ${dir}/${tool} ]
    then
      found=true
      break
    fi
  done

  # Try to run it 
  if ${found}
  then
    case ${tool} in
      w3m)
        TOOL="w3m $W3MOPTS"
        ;;
      links)
        TOOL="links $LNKOPTS"
        ;;
    esac
    rm -f BUILDING
    (cd ../native; ${TOOL} ../build/docs/install/printer/apache1.html 2>/dev/null ) > BUILDING
    ls -lt  BUILDING
    if [ -f BUILDING -a -s BUILDING ]
    then
      failed=false
      break
    fi
  fi
done
if ${failed}
then
  echo "Can't convert html to text (BUILDING)"
  exit 1
fi

# Export text docs
cd ../native
${TOOL} ../build/docs/install/printer/apache1.html >BUILDING
${TOOL} ../build/docs/install/printer/apache2.html >>BUILDING
${TOOL} ../build/docs/install/printer/iis.html >>BUILDING
${TOOL} ../build/docs/printer/changelog.html >CHANGES
${TOOL} ../build/docs/news/printer/20050101.html >NEWS
${TOOL} ../build/docs/news/printer/20041100.html >>NEWS
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
