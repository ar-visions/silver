import: deflate https://github.com/ebiggers/libdeflate                  682a668
    -DBUILD_SHARED_LIBS=ON

import: Imath   https://github.com/AcademySoftwareFoundation/Imath      c0396a0

import: openexr https://github.com/AcademySoftwareFoundation/openexr    0b83825
    -DBUILD_SHARED_LIBS=ON

import: zlib    https://github.com/madler/zlib                          04f42ce
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

import: libpng  https://github.com/glennrp/libpng                       07b8803
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

import: opencv  https://github.com/opencv/opencv                        master
    -DBUILD_LIST=core,imgproc

type: static
modules: A
link:   -lopencv_core -lopencv_imgproc
cflags: -O2 -g0 -mavx2 -mfma -I$IMPORT/include/opencv4 -I$IMPORT/include/opencv4/opencv2
