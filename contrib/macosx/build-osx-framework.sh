#!/bin/bash

#
# A script to build Extractor.framework for Mac OS X
#
# Copyright (C) 2008 Heikki Lindholm
#
# Run from the libextractor top source dir, e.g. 
# > ./contrib/macosx/build-osx-framework.sh
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
FW_BASE_DIR=/Library/Frameworks/${FW_NAME}
BUILD_DIR=/tmp/Extractor-build
FINAL_FW_BASE_DIR="${BUILD_DIR}/Frameworks/${FW_NAME}"
SDK_PATH="${BUILD_DIR}/${SDK}"
OPT_FLAGS="-O2 -force_cpusubtype_ALL"

BUILD_ARCHS_LIST="ppc i386"
export MACOSX_DEPLOYMENT_TARGET=10.4

GNUMAKE_URL=ftp://ftp.gnu.org/pub/gnu/make
GNUMAKE_NAME=make-3.81
LIBTOOL_URL=ftp://ftp.gnu.org/gnu/libtool
LIBTOOL_NAME=libtool-2.2.4
GETTEXT_URL=ftp://ftp.gnu.org/gnu/gettext
GETTEXT_NAME=gettext-0.16.1
LIBOGG_URL=ftp://downloads.xiph.org/pub/xiph/releases/ogg
LIBOGG_NAME=libogg-1.1.3
LIBVORBIS_URL=ftp://downloads.xiph.org/pub/xiph/releases/vorbis
LIBVORBIS_NAME=libvorbis-1.2.0
LIBFLAC_URL=http://kent.dl.sourceforge.net/sourceforge/flac
LIBFLAC_NAME=flac-1.2.1
LIBMPEG2_URL=http://libmpeg2.sourceforge.net/files
LIBMPEG2_NAME=libmpeg2-0.5.1

export PATH=${BUILD_DIR}/toolchain/bin:/usr/local/bin:/bin:/sbin:/usr/bin:/usr/sbin

#
# Fetch necessary packages
#

# $1 = package name
# $2 = base url
fetch_package()
{
	if ! cd contrib 
	then
		echo "missing 'contrib' dir"
		exit 1
	fi
	if [ ! -e "$1.tar.bz2" ] && [ ! -e "$1.tar.gz" ]
	then
		echo "fetching $1..."
		if ! ( curl -f -L -O --url "$2/$1.tar.bz2" )
		then
			if ! ( curl -f -L -O --url "$2/$1.tar.gz" )
			then
				echo "error fetching $1"
				exit 1
			fi
		fi
	fi
	cd ..
}

fetch_all_packages()
{
	fetch_package "${GNUMAKE_NAME}" "${GNUMAKE_URL}"
	fetch_package "${GETTEXT_NAME}" "${GETTEXT_URL}"
	fetch_package "${LIBOGG_NAME}" "${LIBOGG_URL}"
	fetch_package "${LIBVORBIS_NAME}" "${LIBVORBIS_URL}"
	fetch_package "${LIBFLAC_NAME}" "${LIBFLAC_URL}"
	fetch_package "${LIBMPEG2_NAME}" "${LIBMPEG2_URL}"
}

# $1 = package name
# $2 = configure options
build_toolchain_package()
{
	local build_retval=0
	echo "building toolchain: $1..."
	if ! cd contrib
	then
		echo "missing 'contrib' dir"
		exit 1
	fi
	if [ -e "$1.tar.bz2" ]
	then
		if ! ( tar xjf "$1.tar.bz2" )
		then
			echo "error extracting $1"
			exit 1
		fi
	elif [ -e "$1.tar.gz" ]
	then
		if ! ( tar xzf "$1.tar.gz" )
		then
			echo "error extracting $1"
			exit 1
		fi
	else
		echo "no such package $1"
		exit 1
	fi
	CPPFLAGS="-I${BUILD_DIR}/toolchain/include"
	LDFLAGS="-L${BUILD_DIR}/toolchain/lib"
	if ! ( cd $1 && CPPFLAGS="${CPPFLAGS}"				\
			LDFLAGS="${LDFLAGS}"				\
			./configure --prefix="${BUILD_DIR}/toolchain"	\
			$2 &&						\
		make install )
	then
		echo "error building $1 for toolchain"
		build_retval=1
	fi
	unset CPPFLAGS
	unset LDFLAGS
	rm -rf "$1"
	cd ..
	if [ $build_retval -eq 1 ] 
	then
		exit 1
	fi
}

#
# build native tools needed for building other packages
#
build_toolchain()
{
	
	if [ ! -e "${BUILD_DIR}/toolchain/bin/make" ]
	then
		build_toolchain_package ${GNUMAKE_NAME} ""
	fi

	if [ ! -e "${BUILD_DIR}/toolchain/bin/msgfmt" ]
	then
		build_toolchain_package "${GETTEXT_NAME}"	\
			"--disable-java				\
			--disable-native-java			\
			--without-emacs"
	fi

	if [ ! -e "${BUILD_DIR}/toolchain/bin/dictionary-builder" ]
	then
		echo "building toolchain: dictionary-builder..."
		if ! ( make clean &&				\
			./configure --prefix="${BUILD_DIR}/toolchain"	\
					--disable-gsf			\
					--disable-gnome			\
					--disable-exiv2			\
					--enable-printable &&		\
			make install &&
			cp src/plugins/printable/dictionary-builder	\
				"${BUILD_DIR}/toolchain/bin" )
		then
			exit 1
		fi
	fi
}

#
# prepare SDK
#
prepare_sdk() 
{
	if [ ! -e "${BUILD_DIR}" ]
	then
		if ! ( mkdir -p "${BUILD_DIR}" )
		then
			echo "error creating build dir"
			exit 1
		fi
	fi

	if [ ! -e "${SDK_PATH}" ]
	then
		echo "copying SDK to build dir..."
		if ! ( cp -ipPR "${ORIG_SDK}" "${BUILD_DIR}" )
		then
			echo "error preparing SDK"
			exit 1
		fi
	fi

	if [ -h "${SDK_PATH}/Library/Frameworks" ]
	then
		if ! ( rm -f "${SDK_PATH}/Library/Frameworks" )
		then
			echo "error removing SDK 'Frameworks' symlink"
			exit 1
		fi
		if ! ( mkdir -p "${SDK_PATH}/Library/Frameworks" )
		then
			echo "error creating SDK 'Frameworks' directory"
			exit 1
		fi
	fi
}

prepare_package()
{
	local prepare_retval=0
	if [ ! -e "${BUILD_DIR}/built-$1-${ARCH_NAME}" ]
	then
		if ! cd contrib 
		then
			echo "missing 'contrib' dir"
			exit 1
		fi

		if [ ! -e "$1" ]
		then
			if [ -e "$1.tar.bz2" ]
			then
				if ! ( tar xjf "$1.tar.bz2" )
				then
					echo "error extracting $1"
					prepare_retval=1
				fi
			elif [ -e "$1.tar.gz" ]
			then
				if ! ( tar xzf "$1.tar.gz" )
				then
					echo "error extracting $1"
					prepare_retval=1
				fi
			else
				echo "no such package $1"
				prepare_retval=1
			fi
		fi
		for patchfile in $( ls $1-patch-* 2> /dev/null | sort )
		do
			echo "applying $patchfile..."
			if ! ( cd $1 && cat "../$patchfile" | patch -p1 )
			then
				echo "error patching $1"
				prepare_retval=1
			fi
		done

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
	local build_retval=0
	if [ ! -e "${BUILD_DIR}/built-$1-${ARCH_NAME}" ]
	then
		echo "building $1 for ${ARCH_NAME}..."
		if ! cd contrib
		then
			echo "missing 'contrib' dir"
			exit 1
		fi
		CC="${ARCH_CC}"
		CXX="${ARCH_CXX}"
		CPPFLAGS="${ARCH_CPPFLAGS}"
		CFLAGS="${OPT_FLAGS} -no-cpp-precomp -fno-common -fPIC ${ARCH_CFLAGS}"
		CXXFLAGS="${CFLAGS}"
		LDFLAGS="${ARCH_LDFLAGS}"
		if ! ( cd "$1" && CC="${CC}"				\
			CXX="${CXX}"					\
			CPPFLAGS="${CPPFLAGS}"				\
			CFLAGS="${CFLAGS}"				\
			CXXFLAGS="${CXXFLAGS}"				\
			LDFLAGS="${LDFLAGS}"				\
			./configure $2 &&				\
			make DESTDIR="${SDK_PATH}" install &&		\
			touch "${BUILD_DIR}/built-$1-${ARCH_NAME}" )
		then
			echo "error building $1 for ${ARCH_NAME}"
			build_retval=1
		fi
		cp -v "$1/config.log" "${BUILD_DIR}/config.log-$1-${ARCH_NAME}"
		cp -v "$1/config.h" "${BUILD_DIR}/config.h-$1-${ARCH_NAME}"
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
#	prepare_package "${GETTEXT_NAME}"
#	build_package "${GETTEXT_NAME}"			\
#			"${ARCH_HOSTSETTING}		\
#			--prefix="${FW_DIR}"		\
#			--with-pic			\
#			--disable-shared		\
#			--enable-static			\
#			--disable-java			\
#			--disable-native-java		\
#			--without-emacs			\
#			--with-libiconv-prefix=${SDK_PATH}/usr"

	prepare_package "${LIBOGG_NAME}"
	build_package "${LIBOGG_NAME}"			\
			"${ARCH_HOSTSETTING}		\
			ac_cv_func_memcmp_working=yes	\
			--prefix="${FW_DIR}"		\
			--with-pic			\
			--disable-shared		\
			--enable-static"
 
	prepare_package "${LIBVORBIS_NAME}"
	build_package "${LIBVORBIS_NAME}"		\
			"${ARCH_HOSTSETTING}		\
			ac_cv_func_memcmp_working=yes	\
			--prefix="${FW_DIR}"		\
			--with-pic			\
			--disable-shared		\
			--enable-static			\
			--disable-oggtest"

	prepare_package "${LIBFLAC_NAME}"
	build_package "${LIBFLAC_NAME}"			\
			"${ARCH_HOSTSETTING}		\
			--prefix="${FW_DIR}"		\
			--with-pic			\
			--disable-shared		\
			--enable-static			\
			--disable-debug			\
			--disable-asm-optimizations	\
			--disable-cpplibs		\
			--disable-oggtest		\
			--with-libiconv-prefix=${SDK_PATH}/usr"

	prepare_package "${LIBMPEG2_NAME}"
	build_package "${LIBMPEG2_NAME}"		\
			"${ARCH_HOSTSETTING}		\
			--prefix="${FW_DIR}"		\
			--with-pic			\
			--disable-shared		\
			--enable-static			\
			--disable-debug"

}

#
# build libextractor
#
build_extractor()
{
	local build_retval=0
	if [ ! -e "${BUILD_DIR}/built-Extractor-${ARCH_NAME}" ]
	then
		echo "building libextractor for ${ARCH_NAME}..."
		ARCH_LDFLAGS="-arch ${ARCH_NAME} -isysroot ${SDK_PATH} -Wl,-syslibroot,${SDK_PATH} -L${FW_DIR}/lib"
		CFLAGS="${OPT_FLAGS} -no-cpp-precomp ${ARCH_CFLAGS}"
		CPPFLAGS="${ARCH_CPPFLAGS}"
		CXXFLAGS="${CFLAGS}"
		LDFLAGS="${ARCH_LDFLAGS}"
		if ! ( CC="${ARCH_CC}"				\
			CXX="${ARCH_CXX}"			\
			CPPFLAGS="${CPPFLAGS}"			\
			CFLAGS="${CFLAGS}"			\
			CXXFLAGS="${CXXFLAGS}"			\
			LDFLAGS="${LDFLAGS}"			\
			NM="/usr/bin/nm -p"			\
			ac_cv_func_memcmp_working=yes		\
			ac_cv_func_mmap_fixed_mapped=yes	\
			ac_cv_func_stat_empty_string_bug=no	\
			./configure "${ARCH_HOSTSETTING}"	\
			--prefix="${FW_DIR}"			\
			--enable-shared				\
			--enable-framework			\
			--disable-gsf				\
			--disable-gnome				\
			--enable-ffmpeg				\
			--with-ffmpeg-arch="unknown"		\
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
		# use native dictionary-builder instead of the cross-built one
		find ./ -type f -name "Makefile" |	\
			xargs perl -pi -w -e "s#./dictionary-builder #${BUILD_DIR}/toolchain/bin/dictionary-builder #g;"
		# add linking to libiconv where libintl is used
		find ./ -type f -name "Makefile" |	\
			xargs perl -pi -w -e "s#-lintl#-lintl -liconv#g;"
		if ! ( test $build_retval = 0 && make clean &&		\
			make DESTDIR="${SDK_PATH}" install )
		then
			build_retval=1
		fi
# XXX version info for fw
		if ! ( test $build_retval = 0 && \
			gcc -dynamiclib -install_name "${FW_DIR}/Extractor" \
			-compatibility_version 1 -current_version 1.0 \
			-o "${SDK_PATH}/${FW_DIR}/Extractor" \
			${LDFLAGS} \
			-L"${SDK_PATH}/${FW_DIR}/lib" \
			-sub_library libextractor \
			-lextractor && \
			touch "${BUILD_DIR}/built-Extractor-${ARCH_NAME}" )
		then
			build_retval=1
		fi

		cp -v config.log "${BUILD_DIR}/config.log-Extractor-${ARCH_NAME}"
		cp -v config.h "${BUILD_DIR}/config.h-Extractor-${ARCH_NAME}"
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
		if ! ( mv "${SDK_PATH}/${FW_BASE_DIR}" "${SDK_PATH}/${FW_BASE_DIR}-${ARCH_NAME}" )
		then
			echo "error finalizing arch build"
			exit 1
		fi
	fi
}

create_directory_for()
{
	local dst_dir=$(dirname "$1")
	if [ ! -e "${dst_dir}" ]
	then
		echo "MKDIR ${dst_dir}"
		if ! ( mkdir -m 775 -p "${dst_dir}" )
		then
			echo "failed to create directory: ${dst_dir}"
			exit 1
		fi
		# fix dir permissions
		if ! ( chmod 0775 `find ${FINAL_FW_BASE_DIR} -type d` )
		then
			echo "error setting permissions"
			exit 1
		fi
	fi
}

install_executable_to_framework()
{
	local src_name="$1"
	local src_files=""
	local dst_file="${FINAL_FW_DIR}/${src_name}"
	for arch in $BUILD_ARCHS_LIST 
	do
		local tmpfile="${SDK_PATH}/${FW_BASE_DIR}-${arch}/${FW_VERSION_DIR}/${src_name}"
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
		local extralibs=$(otool -L ${src_files} | grep "compatibility version" |cut -d' ' -f 1 | sort | uniq -u)
		if [ "x$extralibs" != "x" ]
		then
			echo "WARNING: linking difference"
			echo "$extralibs"
		fi
		if [ ! -e "${dst_file}" ] && [ ! -h "${dst_file}" ]
		then
			echo "LIPO ${dst_file}"
			if ! ( lipo -create -o "${dst_file}" ${src_files} )
			then
				echo "error creating fat binary"
				exit 1
			fi
			if ! ( chmod 0775 "${dst_file}" )
			then
				echo "error settings permissions"
				exit 1
			fi
		fi
	fi
}

install_file_to_framework()
{
	local src_name="$1"
	for arch in $BUILD_ARCHS_LIST 
	do
		local src_file="${SDK_PATH}/${FW_BASE_DIR}-${arch}/${FW_VERSION_DIR}/${src_name}"
		local dst_file="${FINAL_FW_DIR}/${src_name}"
		create_directory_for "${dst_file}"
		if [ ! -e "${dst_file}" ] && [ ! -h "${dst_file}" ]
		then
			if [ -h "${src_file}" ]
			then
				echo "CP ${dst_file}"
				if ! ( cp -PpR "${src_file}" "${dst_file}" )
				then
					echo "error copying file"
					exit 1
				fi
			elif [ -f "${src_file}" ]
			then
				echo "INSTALL ${dst_file}"
				if ! ( install -m 0664 "${src_file}" "${dst_file}" )
				then
					echo "error installing file"
					exit 1
				fi
			else
				echo "no such file: ${src_file}"
				exit 1
			fi
		else
			if [ -f "${src_file}" ] && [ ! -h "${src_file}" ] && [ -f "${dst_file}" ] && [ ! -h "${dst_file}" ]
			then
				diff -q "${src_file}" "${dst_file}"
			fi
		fi
	done
}

install_message_catalog_to_framework()
{
	local src_file="$1"
	local lang_name=$( basename -s .po $src_file )
	local dst_file="${FINAL_FW_DIR}/Resources/${lang_name}.lproj/Localizable.strings"
	if [ ! -e "$dst_file" ]
	then
		echo "MSGCAT $src_file $dst_file"
		create_directory_for "$dst_file"
		if ! ( msgcat -t UTF-8 --stringtable-output -o "$dst_file" "$src_file" )
		then
			echo "error creating message catalog: $lang"
			exit 1
		fi
		if ! ( chmod 0664 "${dst_file}" )
		then
			echo "error setting permissions"
			exit 1
		fi
		plutil -lint "$dst_file"
	fi
}

install_en_message_catalog_to_framework()
{
	local src_file="$1"
	local lang_name="en"
	local dst_file="${FINAL_FW_DIR}/Resources/${lang_name}.lproj/Localizable.strings"
	if [ ! -e "$dst_file" ]
	then
		echo "MSGCAT $src_file $dst_file"
		create_directory_for "$dst_file"
		if ! ( msgcat -t UTF-8 "$src_file" | msgen --stringtable-output -o "$dst_file" - )
		then
			echo "error creating English message catalog"
			exit 1
		fi
		if ! ( chmod 0664 "${dst_file}" )
		then
			echo "error setting permissions"
			exit 1
		fi
		plutil -lint "$dst_file"
	fi
}

copy_file_to_framework()
{
	local src_file="$1"
	local dst_file="${FINAL_FW_DIR}/$2"
	if [ ! -e "$dst_file" ]
	then
		create_directory_for "$dst_file"
		if ! ( install -m 0664 "$src_file" "$dst_file" )
		then
			echo "error installing file"
			exit 1
		fi
	fi
}

fill_framework_revision()
{
	local dst_file="${FINAL_FW_DIR}/$1"
	if [ -e "$dst_file" ]
	then
		if ! ( sed -e "s/@FRAMEWORK_REVISION@/${FW_VERSION_REV}/g" -i "" "$dst_file" )
		then
			echo "sed error"
			exit 1
		fi
	fi
}

make_framework_link()
{
	local link_target="$1"
	local link_name="$2"
	echo "LN $link_name"
	if ! ( cd "${FINAL_FW_DIR}" && ln -sf "$link_target" "$link_name" )
	then
		echo "error creating link"
		exit 1
	fi
}

make_framework_version_links()
{
	if ! ( cd "${FINAL_FW_BASE_DIR}/Versions" && \
		ln -sf "${FW_VERSION}" "Current" && \
		cd "${FINAL_FW_BASE_DIR}" && \
		ln -sf "Versions/Current/Headers" "Headers" && \
		ln -sf "Versions/Current/Extractor" "Extractor" && \
		ln -sf "Versions/Current/PlugIns" "PlugIns" && \
		ln -sf "Versions/Current/Resources" "Resources" )
	then
		echo "error creating standard framework links"
		exit 1
	fi
}

FW_VERSION=`grep "LIB_VERSION_CURRENT=[0123456789]*" ./configure | cut -d= -f2`
FW_VERSION_DIR="Versions/${FW_VERSION}"
FW_DIR="${FW_BASE_DIR}/${FW_VERSION_DIR}"
FINAL_FW_DIR="${FINAL_FW_BASE_DIR}/${FW_VERSION_DIR}"
ORIG_DIR=$(pwd)
old_umask=$(umask)

if [ ! -n "$1" ]
then
  FW_VERSION_REV="1"
else
  FW_VERSION_REV="$1"
fi  

# prepare build env
fetch_all_packages
umask 002
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
	ARCH_CC="gcc -arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
	ARCH_CXX="g++ -arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
	ARCH_CPPFLAGS="-I${SDK_PATH}/${FW_DIR}/include -isysroot ${SDK_PATH}"
	ARCH_CFLAGS="-arch ${ARCH_NAME} -isysroot ${SDK_PATH}"
	ARCH_LDFLAGS="-L${SDK_PATH}/${FW_DIR}/lib -arch ${ARCH_NAME} -isysroot ${SDK_PATH} -Wl,-syslibroot,${SDK_PATH}"

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
install_executable_to_framework "Extractor"
for tfn in lib/libextractor/libextractor*so
do
	install_executable_to_framework "$tfn"
done
install_file_to_framework 'include/extractor.h'
install_file_to_framework 'share/info/dir'
install_file_to_framework 'share/info/extractor.info'
install_file_to_framework 'share/man/man1/extract.1'
install_file_to_framework 'share/man/man3/libextractor.3'
cd "${ORIG_DIR}"
copy_file_to_framework "./contrib/macosx/Info.plist" "Resources/Info.plist"
fill_framework_revision "Resources/Info.plist"
for tfn in ./po/*.po
do
	install_message_catalog_to_framework "$tfn"
done
install_en_message_catalog_to_framework "./po/libextractor.pot"
make_framework_link "lib" "Libraries"
make_framework_link "lib/libextractor" "PlugIns"
make_framework_link "include" "Headers"
make_framework_version_links

umask ${old_umask}
echo "done."
