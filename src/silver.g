type:       app
modules:    A net aether aclang allvm img
link:       -lLLVM -llldb
import:     mbedtls https://github.com/Mbed-TLS/mbedtls ec40440
    -DPython3_EXECUTABLE=$IMPORT/bin/python3
    -DCMAKE_C_COMPILER="gcc"
    -DCMAKE_CXX_COMPILER="g++"
    -DENABLE_TESTING=0
    -DPSA_CRYPTO_DRIVERS=0
    -DCMAKE_POSITION_INDEPENDENT_CODE=1
    -DLINK_WITH_PTHREAD=1
    >> git submodule update --init --recursive
