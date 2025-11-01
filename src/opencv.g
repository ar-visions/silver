import: NVIDIA:libdeflate/682a668 as deflate
    -DBUILD_SHARED_LIBS=ON

import: AcademySoftwareFoundation:Imath/c0396a0

import: AcademySoftwareFoundation:openexr/0b83825
    -DBUILD_SHARED_LIBS=ON

import: madler:zlib/04f42ce
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

import: glennrp:libpng/07b8803
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

import: opencv:opencv/49486f6
    -DBUILD_LIST=core,imgproc
    -DCMAKE_C_COMPILER="{'clang-cl' if win else 'gcc'}"
    -DCMAKE_CXX_COMPILER="{'clang-cl' if win else 'g++'}"
    -G "{default_generator}"

type: static
modules: A
link:   -lopencv_core -lopencv_imgproc
cflags: -O2 -g0 -mavx2 -mfma -I$IMPORT/include/opencv4 -I$IMPORT/include/opencv4/opencv2
