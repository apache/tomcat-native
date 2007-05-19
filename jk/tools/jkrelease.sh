#!/bin/ksh

# Make sure to set your path so that we can find
# the following binaries:
# cd, mkdir, cp, rm, find
# svn
# ant
# libtoolize, aclocal, autoheader, automake, autoconf
# tar, zip, gzip
# gpg
# And any one of: w3m, elinks, links (links2)

usage() {
    echo "Usage:: $0 -t VERSION [-b BRANCH | -T]"
    echo "        -t: version to package"
    echo "        -b: package from branch BRANCH"
    echo "        -T: package from trunk"
}

while getopts :t:b:T c
do
    case $c in
    t)         tag=$OPTARG;;
    b)         branch=$OPTARG;;
    T)         trunk=trunk;;
    \:)        usage
               exit 2;;
    \?)        usage
               exit 2;;
    esac
done
shift `expr $OPTIND - 1`

SVNROOT="http://svn.apache.org/repos/asf"
SVNPROJ="tomcat/connectors"
JK_CVST="tomcat-connectors"

JK_OWNER="root"
JK_GROUP="bin"

COPY_TOP="KEYS LICENSE NOTICE"
COPY_JK="BUILD.txt native jkstatus support tools xdocs"
COPY_CONF="uriworkermap.properties workers.properties workers.properties.minimal"

#################### NO CHANGE BELOW THIS LINE ##############

if [ -z "$tag" ]
then
    usage
    exit 2
fi
if [ -n "$trunk" -a -n "$branch" ]
then
    usage
    echo "Only one of the options '-b' and '-T' allowed."
    exit 2
fi
if [ -n "$trunk" ]
then
    JK_SVN_URL="${SVNROOT}/${SVNPROJ}/trunk"
    JK_REV=`svn info ${JK_SVN_URL} | awk '$1 == "Revision:" {print $2}'`
    if [ -z "$JK_REV" ]
    then
       echo "No Revision found at '$JK_SVN_URL'"
       exit 3
    fi
    JK_DIST=${JK_CVST}-${tag}-dev-${JK_REV}-src
elif [ -n "$branch" ]
then
    JK_BRANCH=`echo $branch | sed -e 's#/#__#g'`
    JK_SVN_URL="${SVNROOT}/${SVNPROJ}/branches/$branch"
    JK_REV=`svn info ${JK_SVN_URL} | awk '$1 == "Revision:" {print $2}'`
    if [ -z "$JK_REV" ]
    then
       echo "No Revision found at '$JK_SVN_URL'"
       exit 3
    fi
    JK_DIST=${JK_CVST}-${tag}-dev-${JK_BRANCH}-${JK_REV}-src
else
    JK_VER=$tag
    JK_TAG=`echo $tag | sed -e 's#^#JK_#' -e 's#\.#_#g'`
    JK_BRANCH=`echo $tag | sed -e 's#^#jk#' -e 's#\.[0-9][0-9]*$##' -e 's#$#.x#'`
    JK_SVN_URL="${SVNROOT}/${SVNPROJ}/tags/${JK_BRANCH}/${JK_TAG}"
    JK_DIST=${JK_CVST}-${JK_VER}-src
fi

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
ELNKOPTS="-dump -dump-width 80 -dump-charset iso-8859-1 -no-numbering -no-references -no-home"
LNKOPTS="-dump -width 80 -codepage iso-8859-1 -no-g -html-numbered-links 0"
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
    echo "Creating the CHANGES file using '$TOOL' ..."
    ${TOOL} ../docs/miscellaneous/printer/changelog.html > CHANGES 2>/dev/null
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
echo "Creating the NEWS file using '$TOOL' ..."
rm -f NEWS
touch NEWS
for news in `ls -r ../xdocs/news/[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].xml`
do
  print=`echo $news | sed -e 's#xdocs/news#docs/news/printer#' -e 's#\.xml#.html#'`
  echo "Adding $print to NEWS file ..."
  ${TOOL} $print >>NEWS
done
if [ ! -s NEWS ]
then
  echo "Can't convert html to text (NEWS)"
  exit 1
fi

# Generate configure et. al.
./buildconf.sh
cd ../../

# Pack and sign
tar cfz ${JK_DIST}.tar.gz --owner="${JK_OWNER}" --group="${JK_GROUP}" ${JK_DIST}
perl ${JK_DIST}/tools/lineends.pl --cr ${JK_DIST}
zip -9 -r ${JK_DIST}.zip ${JK_DIST}
# Create detatched signature
gpg -ba ${JK_DIST}.tar.gz
gpg -ba ${JK_DIST}.zip
