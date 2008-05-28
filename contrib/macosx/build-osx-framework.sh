#!/bin/bash

#
# A script to build Extractor.framework for Mac OS X
# 
# Copyright (C) 2008 Heikki Lindholm
#
# - 64-bit archs won't build on Mac OS X 10.4 (too many missing deps)
# 
# TODO: 
#  - find a cleaner libtool workaround
#  - error checking
#

SDK=MacOSX10.4u.sdk
ORIG_SDK=/Developer/SDKs/${SDK}
FW_NAME=Extractor.framework
FW_BASE_DIR=/tmp/${FW_NAME}
BUILD_DIR=/tmp/Extractor-build
FINAL_FW_BASE_DIR="${BUILD_DIR}/${FW_NAME}"
SDK_PATH="${BUILD_DIR}/${SDK}"
OPT_FLAGS="-O2 -g"

BUILD_ARCHS_LIST="ppc i386"
export MACOSX_DEPLOYMENT_TARGET=10.4

PKGCONFIG_URL=http://pkgconfig.freedesktop.org/releases
PKGCONFIG_NAME=pkg-config-0.23
LIBTOOL_URL=ftp://ftp.gnu.org/gnu/libtool
LIBTOOL_NAME=libtool-2.2.4
GETTEXT_URL=ftp://ftp.gnu.org/gnu/gettext
GETTEXT_NAME=gettext-0.16.1
LIBOGG_URL=http://downloads.xiph.org/releases/ogg
LIBOGG_NAME=libogg-1.1.3
LIBVORBIS_URL=http://downloads.xiph.org/releases/vorbis
LIBVORBIS_NAME=libvorbis-1.1.2
LIBFLAC_URL=http://switch.dl.sourceforge.net/sourceforge/flac
LIBFLAC_NAME=flac-1.2.1
LIBMPEG2_URL=http://libmpeg2.sourceforge.net/files
LIBMPEG2_NAME=mpeg2dec-0.4.1

export PATH=/usr/local/bin:/bin:/sbin:/usr/bin:/usr/sbin:${BUILD_DIR}/toolchain/bin

#
# Fetch necessary packages
#

# $1 = package name
# $2 = base url
fetch_package()
{
	cd contrib
	if [ ! -e "$1.tar.gz" ]
	then
		echo "fetching $1..."
		curl -O --url "$2/$1.tar.gz"
	fi
	if [ ! -e "$1.tar.gz" ]
	then
		echo "Could not fetch package: $1"
		exit 1
	fi
	cd ..
}

fetch_all_packages()
{
	fetch_package "${PKGCONFIG_NAME}" "${PKGCONFIG_URL}"
#	fetch_package "${LIBTOOL_NAME}" "${LIBTOOL_URL}"
	fetch_package "${GETTEXT_NAME}" "${GETTEXT_URL}"
	fetch_package "${LIBOGG_NAME}" "${LIBOGG_URL}"
	fetch_package "${LIBVORBIS_NAME}" "${LIBVORBIS_URL}"
	fetch_package "${LIBFLAC_NAME}" "${LIBFLAC_URL}"
	fetch_package "${LIBMPEG2_NAME}" "${LIBMPEG2_URL}"
}

#
# build native tools needed for building other packages
#
build_toolchain()
{
	if [ ! -e "${BUILD_DIR}/toolchain/bin/dictionary-builder" ]
	then
		./configure --prefix="${BUILD_DIR}/toolchain"	\
				--disable-gsf			\
				--disable-gnome
		make install
		cp src/plugins/printable/dictionary-builder	\
			"${BUILD_DIR}/toolchain/bin"
		make clean
	fi

	if [ ! -e "${BUILD_DIR}/toolchain/bin/msgfmt" ]
	then
		cd contrib

		tar xzf "${GETTEXT_NAME}.tar.gz"
		cd "${GETTEXT_NAME}"
		./configure --prefix="${BUILD_DIR}/toolchain"	\
				--disable-java			\
				--disable-native-java		\
				--without-emacs
		make install
		cd ..
		rm -rf "${GETTEXT_NAME}"

		cd ..
	fi

	if [ ! -e "${BUILD_DIR}/toolchain/bin/pkg-config" ]
	then
		cd contrib

		tar xzf "${PKGCONFIG_NAME}.tar.gz"
		cd "${PKGCONFIG_NAME}"
		./configure --prefix="${BUILD_DIR}/toolchain"
		make install
		cd ..
		rm -rf "${PKGCONFIG_NAME}"

		cd ..
	fi

#	if [ ! -e "${BUILD_DIR}/toolchain/bin/libtool" ]
#	then
#		cd contrib

#		tar xzf "${LIBTOOL_NAME}.tar.gz"
#		cd "${LIBTOOL_NAME}"
		# Make libfool unable to find ANY .la files, because they
		# invariably cause linking havoc with sysroot although
		# they really shouldn't
#		cp libltdl/config/ltmain.sh libltdl/config/ltmain.sh.orig
		# ugh, for the love of...
#		chmod +w libltdl/config/ltmain.sh
		# XXX works for the tested version only
#		cat libltdl/config/ltmain.sh.orig | \
#			sed "s/found=yes/found=no/g" > libltdl/config/ltmain.sh
#		./configure --prefix="${BUILD_DIR}/toolchain"
#		make install
#		cd ..
#		rm -rf "${LIBTOOL_NAME}"
#	fi
}

#
# prepare SDK
#
prepare_sdk() 
{
	if [ ! -e "${BUILD_DIR}" ]
	then
		mkdir -p "${BUILD_DIR}"
	fi

	if [ ! -e "${SDK_PATH}" ]
	then
		cp -ipPR "${ORIG_SDK}" "${BUILD_DIR}"
	fi

	if [ ! -e "${SDK_PATH}" ]
	then
		echo "error preparing sdk"
		exit 1
	fi
}

prepare_package()
{
	prepare_retval=0
	if [ ! -e "${BUILD_DIR}/built-$1-${ARCH_NAME}" ]
	then
		cd contrib
		if [ ! -e "$1" ]
		then
			if ! ( tar xzf "$1.tar.gz" )
			then
				echo "error extracting $1"
				prepare_retval=1
			fi
		fi
		cd ..
		if [ $prepare_retval -eq 1 ] 
		then
			exit 1
		fi
	fi
}

# $1 = package name
# $2 = configure options
build_package()
{
	build_retval=0
	if [ ! -e "${BUILD_DIR}/built-$1-${ARCH_NAME}" ]
	then
		echo "building $1 for ${ARCH_NAME}..."
		cd contrib
		cd "$1"
		CC="${ARCH_CC}"
		CXX="${ARCH_CXX}"
		CPPFLAGS="${ARCH_CPPFLAGS}"
		CFLAGS="${OPT_FLAGS} -no-cpp-precomp ${ARCH_CFLAGS}"
		CXXFLAGS="${CFLAGS}"
		LDFLAGS="${ARCH_LDFLAGS}"
		if ! ( ./configure CC="${CC}"				\
			CXX="${CXX}"					\
			CPPFLAGS="${CPPFLAGS}"				\
			CFLAGS="${CFLAGS}"				\
			CXXFLAGS="${CXXFLAGS}"				\
			LDFLAGS="${LDFLAGS}"				\
			$2 &&						\
			make DESTDIR="${SDK_PATH}" install &&		\
			touch "${BUILD_DIR}/built-$1-${ARCH_NAME}" )
		then
			echo "error building $1 for ${ARCH_NAME}"
			build_retval=1
		fi
		cd ..
		rm -rf "$1"
		rm -v `find "${SDK_PATH}" -name "*.la"`
		unset CC
		unset CXX
		unset CPPFLAGS
		unset CFLAGS
		unset CXXFLAGS
		unset LDFLAGS
		cd ..
		if [ $build_retval -eq 1 ] 
		then
			exit 1
		fi
	fi
}

#
# build dependencies
#
build_dependencies()
{
	ARCH_CC="gcc -arch ${ARCH_NAME}"
	ARCH_CXX="g++ -arch ${ARCH_NAME}"
	ARCH_CPPFLAGS="-I${SDK_PATH}/${FW_DIR}/include -isysroot ${SDK_PATH}"
	ARCH_CFLAGS="-arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
	ARCH_LDFLAGS="-L${SDK_PATH}/${FW_DIR}/lib -arch ${ARCH_NAME} -isysroot ${SDK_PATH} -Wl,-syslibroot,${SDK_PATH}"

#	prepare_package "${GETTEXT_NAME}"
#	build_package "${GETTEXT_NAME}"			\
#			"${ARCH_HOSTSETTING}		\
#			--prefix="${FW_DIR}"		\
#			--enable-shared			\
#			--disable-java			\
#			--disable-native-java		\
#			--without-emacs			\
#			--with-libiconv-prefix=${SDK_PATH}/usr"

	prepare_package "${LIBOGG_NAME}"
	build_package "${LIBOGG_NAME}"			\
			"${ARCH_HOSTSETTING}		\
			--prefix="${FW_DIR}"		\
			--disable-shared			\
			--enable-static"
 
	prepare_package "${LIBVORBIS_NAME}"
	build_package "${LIBVORBIS_NAME}"		\
			"${ARCH_HOSTSETTING}		\
			--prefix="${FW_DIR}"		\
			--disable-shared			\
			--enable-static			\
			--disable-oggtest"

	prepare_package "${LIBFLAC_NAME}"
	build_package "${LIBFLAC_NAME}"			\
			"${ARCH_HOSTSETTING}		\
			--prefix="${FW_DIR}"		\
			--disable-shared		\
			--enable-static			\
			--disable-asm-optimizations	\
			--disable-cpplibs		\
			--disable-oggtest		\
			--with-libiconv-prefix=${SDK_PATH}/usr"

	prepare_package "${LIBMPEG2_NAME}"
	build_package "${LIBMPEG2_NAME}"		\
			"${ARCH_HOSTSETTING}		\
			--prefix="${FW_DIR}"		\
			--disable-shared		\
			--enable-static"

}

#
# build libextractor
#
build_extractor()
{
	build_retval=0
	if [ ! -e "${BUILD_DIR}/built-Extractor-${ARCH_NAME}" ]
	then
		echo "building libextractor for ${ARCH_NAME}..."
		ARCH_CC="gcc -arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
		ARCH_CXX="g++ -arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
		ARCH_CPPFLAGS="-isysroot ${SDK_PATH} -I${SDK_PATH}/${FW_DIR}/include"
		ARCH_CFLAGS="-arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
		ARCH_LDFLAGS="-arch ${ARCH_NAME} -isysroot ${SDK_PATH} -Wl,-syslibroot,${SDK_PATH} -L${FW_DIR}/lib"
		CFLAGS="${OPT_FLAGS} -no-cpp-precomp ${ARCH_CFLAGS}"
		CPPFLAGS="${ARCH_CPPFLAGS}"
		CXXFLAGS="${CFLAGS}"
		LDFLAGS="${ARCH_LDFLAGS}"
		if ! ( ./configure CC="${ARCH_CC}"		\
			CXX="${ARCH_CXX}"			\
			CPPFLAGS="${CPPFLAGS}"			\
			CFLAGS="${CFLAGS}"			\
			CXXFLAGS="${CXXFLAGS}"			\
			LDFLAGS="${LDFLAGS}"			\
			"${ARCH_HOSTSETTING}"			\
			--prefix="${FW_DIR}"			\
			--enable-shared				\
			--disable-gsf				\
			--disable-gnome				\
			--with-libiconv-prefix=${SDK_PATH}/usr )
		then
			build_retval=1
		fi
		# XXX unbelievably fragile!!!
		cp ./libtool ./libtool.tmp
		cat ./libtool.tmp | \
			sed "s/found=yes/found=no/g;" | \
			sed "s|eval depdepl=\"\$tmp\/lib\$tmp_libs.dylib\"|if test \"x\$tmp\" = \"x\/usr\/lib\" ; then\\
eval depdepl=\"${SDK_PATH}\/\$tmp\/lib\$tmp_libs.dylib\"\\
else\\
eval  depdepl=\"\$tmp\/lib\$tmp_libs.dylib\"\\
fi|g" > ./libtool
		rm ./libtool.tmp
		#rm libtool
		#ln -s "${BUILD_DIR}/toolchain/bin/libtool" ./libtool
		#cp -Pp libtool libtool.orig
		#cat libtool.orig | sed "s|sys_lib_search_path_spec=\"[^\"]*\"|sys_lib_search_path_spec=\"${SDK_PATH}/usr/lib\"|g" > libtool
		# use native dictionary-builder instead of the cross-built one
		find ./ -type f -name "Makefile" |	\
			xargs perl -pi -w -e "s#./dictionary-builder #${BUILD_DIR}/toolchain/bin/dictionary-builder #g;"
		if ! ( make DESTDIR="${SDK_PATH}" install &&	\
			touch "${BUILD_DIR}/built-Extractor-${ARCH_NAME}" )
		then
			build_retval=1
		fi
		unset CPPFLAGS
		unset CFLAGS
		unset CXXFLAGS
		unset LDFLAGS
		if [ $build_retval -eq 1 ] 
		then
			exit 1
		fi
	fi
}

finalize_arch_build()
{
	if [ ! -e "${SDK_PATH}/${FW_BASE_DIR}-${ARCH_NAME}" ]
	then
		mv "${SDK_PATH}/${FW_BASE_DIR}" "${SDK_PATH}/${FW_BASE_DIR}-${ARCH_NAME}"
	fi
}

create_directory_for()
{
	dst_dir=$(dirname "$1")
	if [ ! -e "${dst_dir}" ]
	then
		echo "MKDIR ${dst_dir}"
		if ! ( mkdir -p "${dst_dir}" )
		then
			echo "failed to create directory: ${dst_dir}"
			exit 1
		fi
	fi
}

install_executable_to_framework()
{
	src_name="$1"
	src_files=""
	dst_file="${FINAL_FW_DIR}/${src_name}"
	for arch in $BUILD_ARCHS_LIST 
	do
		tmpfile="${SDK_PATH}/${FW_BASE_DIR}-${arch}/${FW_VERSION_DIR}/${src_name}"
		if [ -h "${tmpfile}" ]
		then
			install_file_to_framework $1
		elif [ -f "${tmpfile}" ]
		then
			src_files="${tmpfile} ${src_files}"
		else
			echo "no such file: ${tmpfile}"
			exit 1
		fi
	done
	if [ "x${src_files}" != "x" ]
	then
		create_directory_for "${dst_file}"
		if [ ! -e "${dst_file}" ] && [ ! -h "${dst_file}" ]
		then
			echo "LIPO ${dst_file}"
			lipo -create -o "${dst_file}" ${src_files}
		fi
	fi
}

install_file_to_framework()
{
	src_name="$1"
	for arch in $BUILD_ARCHS_LIST 
	do
		src_file="${SDK_PATH}/${FW_BASE_DIR}-${arch}/${FW_VERSION_DIR}/${src_name}"
		dst_file="${FINAL_FW_DIR}/${src_name}"
		create_directory_for "${dst_file}"
		if [ ! -e "${dst_file}" ] && [ ! -h "${dst_file}" ]
		then
			if [ -h "${src_file}" ]
			then
				echo "CP ${dst_file}"
				cp -PpR "${src_file}" "${dst_file}"
			elif [ -f "${src_file}" ]
			then
				echo "INSTALL ${dst_file}"
				install "${src_file}" "${dst_file}"
			else
				echo "no such file: ${src_file}"
				exit 1
			fi
		else
			if [ -f "${src_file}" ] && [ -f "${dst_file}" ]
			then
				diff "${src_file}" "${dst_file}"
			fi
		fi
	done
}

copy_file_to_framework()
{
	src_file="$1"
	dst_file="${FINAL_FW_DIR}/$2"
	if [ ! -e "$dst_file" ]
	then
		create_directory_for "$dst_file"
		install "$src_file" "$dst_file"
	fi
}

make_framework_link()
{
	link_target="$1"
	link_name="$2"
	orig_dir=$(pwd)
	cd "${FINAL_FW_DIR}"
	echo "LN $link_name"
	ln -sf "$link_target" "$link_name"
	cd "${orig_dir}"
}

make_framework_version_links()
{
	orig_dir=$(pwd)
	cd "${FINAL_FW_BASE_DIR}/Versions"
	ln -sf "${FW_VERSION}" "Current"
	cd "${FINAL_FW_BASE_DIR}"
	ln -sf "Versions/Current/Headers" "Headers"
	ln -sf "Versions/Current/Extractor" "Extractor"
	ln -sf "Versions/Current/PlugIns" "PlugIns"
	ln -sf "Versions/Current/Resources" "Resources"
	cd "${orig_dir}"
}

FW_VERSION=`grep "LIB_VERSION_CURRENT=[0123456789]*" ./configure | cut -d= -f2`
FW_VERSION_DIR="Versions/${FW_VERSION}"
FW_DIR="${FW_BASE_DIR}/${FW_VERSION_DIR}"
FINAL_FW_DIR="${FINAL_FW_BASE_DIR}/${FW_VERSION_DIR}"
export PKG_CONFIG_PATH=${FW_DIR}/lib/pkgconfig
ORIG_DIR=$(pwd)

# prepare build env
fetch_all_packages
prepare_sdk
build_toolchain

# build deps and libextractor for all archs
for arch in $BUILD_ARCHS_LIST
do
  ARCH_NAME=$arch
  case "$arch" in
    "ppc")
      ARCH_HOSTSETTING="--host=powerpc-apple-darwin8"
      ;;
    "ppc64")
      ARCH_HOSTSETTING="--host=powerpc64-apple-darwin8"
      ;;
    "i386")
      ARCH_HOSTSETTING="--host=i686-apple-darwin8"
      ;;
    "x86_64")
      ARCH_HOSTSETTING="--host=x86_64-apple-darwin8"
      ;;
    *)
      echo "unknown architecture ${arch}"
      exit 1
      ;;
  esac
  build_dependencies
  build_extractor
  finalize_arch_build
done

# build framework structure
first_arch=$(echo "$BUILD_ARCHS_LIST" | cut -d ' ' -f 1)
cd "${SDK_PATH}/${FW_BASE_DIR}-${first_arch}/${FW_VERSION_DIR}"
install_executable_to_framework 'bin/extract'
for tfn in lib/libextractor*dylib
do
	install_executable_to_framework "$tfn"
done
for tfn in lib/libextractor/libextractor*so
do
	install_executable_to_framework "$tfn"
done
install_file_to_framework 'include/extractor.h'
install_file_to_framework 'share/man/man1/extract.1'
install_file_to_framework 'share/man/man3/libextractor.3'
for tfn in $(find ./share/locale -name 'libextractor*')
do
	install_file_to_framework "$tfn"
done
cd "${ORIG_DIR}"
copy_file_to_framework "./contrib/macosx/Info.plist" "Resources/Info.plist"
copy_file_to_framework "./contrib/macosx/English.lproj/InfoPlist.strings" "Resources/English.lproj/InfoPlist.strings"
make_framework_link "lib/libextractor.dylib" "Extractor"
make_framework_link "lib" "Libraries"
make_framework_link "lib/libextractor" "PlugIns"
make_framework_link "include" "Headers"
make_framework_version_links

echo "done."
