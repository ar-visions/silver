type: shared
modules: Au vec opencv
link: -ldeflate -lImath -lOpenEXR -lOpenEXRCore -lOpenEXRUtil -lz -lm -lpng
cflags: -DIMATH_DLL -O2 -g0 -mavx2 -mfma -I$IMPORT/include/opencv4 -I$IMPORT/include/opencv4/opencv2 -I$IMPORT/include/OpenEXR -I$IMPORT/include/Imath
