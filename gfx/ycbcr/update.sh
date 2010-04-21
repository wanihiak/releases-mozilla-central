# update.sh <chromium-src-directory>
cp $1/media/base/yuv_convert.h .
cp $1/media/base/yuv_convert.cc yuv_convert.cpp
cp $1/media/base/yuv_row.h .
cp $1/media/base/yuv_row_linux.cc yuv_row_linux.cpp
cp $1/media/base/yuv_row_mac.cc yuv_row_mac.cpp
cp $1/media/base/yuv_row_win.cc yuv_row_win.cpp
cp $1/media/base/yuv_row_linux.cc yuv_row_c.cpp
patch -p3 <convert.patch
patch -p3 <picture_region.patch
patch -p3 <remove_scale.patch
patch -p3 <export.patch
patch -p3 <win64_mac64.patch
patch -p3 <yv24.patch
