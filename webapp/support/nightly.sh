#!/bin/sh

DATE=`date '+%Y%m%d'`
BASE_DIR=/home/pier/WebApp
PREV_DIR=$BASE_DIR/webapp-module-00000000
CURR_REL=webapp-module-$DATE
CURR_DIR=$BASE_DIR/$CURR_REL
TEMP_DIR=$BASE_DIR/temp

TAR_FILE=$BASE_DIR/webapp-module-$DATE.tar
TGZ_FILE=$BASE_DIR/webapp-module-$DATE.tar.gz
ZIP_FILE=$BASE_DIR/webapp-module-$DATE.zip


echo "### Started at `date`"
echo "### Updating CVS tree"
cd $PREV_DIR
cvs update -APd 2> /dev/null | tee $BASE_DIR/log.cvs
cd $BASE_DIR
TEMP=`cat $BASE_DIR/log.cvs`
if test -z "$TEMP" ; then
  echo "> No updates in CVS"
  if test "$1" != "force" ; then
    echo "> Exiting"
    rm -f $BASE_DIR/log.cvs
    exit 0
  else
    echo "> Forced rebuild"
  fi
fi
rm -f $BASE_DIR/log.cvs


echo ""
echo "### Copying tree to new directory"
cp -R $PREV_DIR $CURR_DIR


echo ""
echo "### Running buildconf script"
cd $CURR_DIR
./support/buildconf.sh 2>&1 | sed 's/^/> /g'
cd $BASE_DIR


echo ""
echo "### Running configure"
mkdir $TEMP_DIR
cd $TEMP_DIR
$CURR_DIR/configure \
  --with-apxs=/opt/apache2/bin/apxs \
  --with-ant=$BASE_DIR/ant/ant.sh \
  --with-perl=/usr/bin/perl \
  --enable-java=/opt/tomcat \
  --enable-apidoc-c \
  --enable-apidoc-java \
  --enable-docs \
    | sed 's/^/> /g'
cd $BASE_DIR


echo ""
echo "### Building portable components"
cd $TEMP_DIR
make capi-build ant-build 2>&1 | sed 's/^/> /g'
cd $BASE_DIR


echo ""
echo "### Copying portable components"
set -x
mv $TEMP_DIR/build/docs $CURR_DIR/documentation
mv $TEMP_DIR/build/tomcat-warp.jar $CURR_DIR/tomcat-warp.jar
set +x


echo ""
echo "### Rolling distribution files"
set -x
cd $BASE_DIR
/usr/bin/tar -cf $TAR_FILE $CURR_REL
gzip -9c $TAR_FILE > $TGZ_FILE
rm -f $TAR_FILE
zip -rpq9 $ZIP_FILE $CURR_REL
set +x
