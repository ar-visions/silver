type: shared
modules: Au
link: -lmbedtls -lmbedx509 -lmbedcrypto -lpthread
import:     Mbed-TLS:mbedtls/ec40440 as mbedtls
    -DPython3_EXECUTABLE={'$IMPORT/bin/python3' if not win else 'python'}
    -DCMAKE_C_COMPILER="{'clang-cl' if win else 'gcc'}"
    -DCMAKE_CXX_COMPILER="{'clang-cl' if win else 'g++'}"
    -DENABLE_TESTING=0
    -DPSA_CRYPTO_DRIVERS=0
    -DCMAKE_POSITION_INDEPENDENT_CODE=1
    -DLINK_WITH_PTHREAD=1
    CC={'clang-cl' if win else 'gcc'}
    >> git submodule update --init --recursive
    >> python3 -m pip install jsonschema jinja2
    >> { 'scripts\make_generated_files.bat' if win else '' }