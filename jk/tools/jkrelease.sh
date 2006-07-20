#!/bin/ksh

# Make sure to set your path so that we can find
# the following binaries:
# cd, mkdir, cp, rm, find
# svn
# ant
# libtoolize, aclocal, autoheader, automake, autoconf
# tar, zip, gzip
# gpg
# And any one of: w3m, elinks, links

export ANT_HOME=/usr/local/ant
export JAVA_HOME=/usr/local/jdk1.4.2

# You need to change the version numbers
JK_VERMAJOR="1"
JK_VERMINOR="2"
JK_VERFIX="19"
ASFROOT="http://svn.apache.org/repos/asf"
JK_CVST="tomcat-connectors"

JK_OWNER="asf"
JK_GROUP="asf"

COPY_TOP="KEYS LICENSE NOTICE"
COPY_JK="BUILD.txt native support tools xdocs"
COPY_CONF="uriworkermap.properties workers.properties workers.properties.minimal"

JK_VER="${JK_VERMAJOR}.${JK_VERMINOR}.${JK_VERFIX}"
JK_BRANCH="jk${JK_VERMAJOR}.${JK_VERMINOR}.x"
JK_TAG="JK_${JK_VERMAJOR}_${JK_VERMINOR}_${JK_VERFIX}"

JK_DIST=${JK_CVST}-${JK_VER}-src
JK_SVN_URL="${ASFROOT}/tomcat/connectors/tags/${JK_BRANCH}/${JK_TAG}"

#################### NO CHANGE BELOW THIS LINE ##############

umask 022

rm -rf ${JK_DIST}
rm -rf ${JK_DIST}.tmp

mkdir -p ${JK_DIST}.tmp
svn export "${JK_SVN_URL}/jk" ${JK_DIST}.tmp/jk
for item in ${COPY_TOP}
do
    svn export "${JK_SVN_URL}/${item}" ${JK_DIST}.tmp/${item}
done

# Build documentation.
cd ${JK_DIST}.tmp/jk/xdocs
ant
cd ../../..

# Copying things into source distribution
srcdir=${JK_DIST}.tmp
targetdir=${JK_DIST}
mkdir -p ${targetdir}
for item in ${COPY_TOP}
do
    echo "Copying $item from ${srcdir} ..."
    cp -pr ${srcdir}/$item ${targetdir}/
done

srcdir=${JK_DIST}.tmp/jk
targetdir=${JK_DIST}
mkdir -p ${targetdir}
for item in ${COPY_JK}
do
    echo "Copying $item from ${srcdir} ..."
    cp -pr ${srcdir}/$item ${targetdir}/
done

srcdir=${JK_DIST}.tmp/jk/build
targetdir=${JK_DIST}
mkdir -p ${targetdir}
for item in docs
do
    echo "Copying $item from ${srcdir} ..."
    cp -pr ${srcdir}/$item ${targetdir}/
done

srcdir=${JK_DIST}.tmp/jk/conf
targetdir=${JK_DIST}/conf
mkdir -p ${targetdir}
for item in ${COPY_CONF}
do
    echo "Copying $item from ${srcdir} ..."
    cp -pr ${srcdir}/$item ${targetdir}/
done

# Remove extra directories and files
targetdir=${JK_DIST}
rm -rf ${targetdir}/xdocs/jk2
rm -rf ${targetdir}/native/CHANGES.txt
rm -rf ${targetdir}/native/build.xml
find ${JK_DIST} -name .cvsignore -exec rm -rf \{\} \; 
find ${JK_DIST} -name CVS -exec rm -rf \{\} \; 
find ${JK_DIST} -name .svn -exec rm -rf \{\} \; 

cd ${JK_DIST}/native

# Check for links, elinks or w3m
W3MOPTS="-dump -cols 80 -t 4 -S -O iso-8859-1 -T text/html"
LNKOPTS="-dump"
ELNKOPTS="--dump --no-numbering --no-home"
failed=true
for tool in `echo "w3m elinks links"`
do
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
      elinks)
        TOOL="elinks $ELNKOPTS"
        ;;
    esac
    rm -f CHANGES
    ${TOOL} ../docs/printer/changelog.html > CHANGES 2>/dev/null
    if [ -f CHANGES -a -s CHANGES ]
    then
      failed=false
      break
    fi
  fi
done
if [ ${failed} = "true" ]
then
  echo "Can't convert html to text (CHANGES)"
  exit 1
fi

# Export text docs
${TOOL} ../docs/printer/changelog.html >CHANGES
${TOOL} ../docs/news/printer/20060101.html >NEWS
${TOOL} ../docs/news/printer/20050101.html >>NEWS
${TOOL} ../docs/news/printer/20041100.html >>NEWS

# Generate configure et. al.
./buildconf.sh
cd ../../

# Pack and sign
tar cvf ${JK_DIST}.tar --owner="${JK_OWNER}" --group="${JK_GROUP}" ${JK_DIST}
gzip ${JK_DIST}.tar
zip -9 -r ${JK_DIST}.zip ${JK_DIST}
# Create detatched signature
gpg -ba ${JK_DIST}.tar.gz
gpg -ba ${JK_DIST}.zip
