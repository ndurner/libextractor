BUILD_DIR=/tmp/Extractor-build
RESOURCE_DIR="${BUILD_DIR}/Resources"
COMPONENT_DIR="${BUILD_DIR}/Frameworks"
PACKAGE_DIR="${BUILD_DIR}/Package"
PACKAGE_NAME="${PACKAGE_DIR}/Extractor.pkg"
PACKAGE_VERSION=`grep "PACKAGE_VERSION='[0123456789a-z.]*'" ./configure | cut -d= -f2 | sed "s/'//g"`
DMG_NAME="$BUILD_DIR/Extractor-${PACKAGE_VERSION}.dmg"

PACKAGEMAKER="/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker"

if ! [ -e "${RESOURCE_DIR}" ] ; then
	mkdir -p "${RESOURCE_DIR}"
fi
	
cp contrib/macosx/License.rtf "${RESOURCE_DIR}/"

if ! [ -e "${PACKAGE_DIR}" ] ; then
	mkdir -p "${PACKAGE_DIR}"
fi

# final permissions
chown -R root "${COMPONENT_DIR}"/*
chgrp -R admin "${COMPONENT_DIR}"/*

# create package
if [ -e "${PACKAGE_NAME}" ] ; then
	rm -rf "${PACKAGE_NAME}" 
fi
$PACKAGEMAKER -build -v -p "${PACKAGE_NAME}" -f "${COMPONENT_DIR}" -r "${RESOURCE_DIR}" -i contrib/macosx/Pkg-Info.plist -d contrib/macosx/Pkg-Description.plist

# create disk image
if [ -e "$DMG_NAME" ] ; then
	rm -f "$DMG_NAME"
fi
hdiutil create -srcfolder "${PACKAGE_DIR}" -format UDBZ -volname "Extractor ${PACKAGE_VERSION} Install" -mode 444 "$DMG_NAME"
