set(SOURCES
    BackingStoreManager.cpp
    CanvasHost.cpp
    CompositorState.cpp
    ContextState.cpp
    ConnectionFromClient.cpp
    ConnectionFromWebContent.cpp
    FontClient.cpp
    PausedDebuggerOverlay.cpp
    ScrollSnapController.cpp
    ScrollbarController.cpp
    VSyncScheduler.cpp
)

set(GENERATED_SOURCES
    CompositorControlClientEndpoint.h
    CompositorControlServerEndpoint.h
    CompositorFontClientEndpoint.h
    CompositorFontServerEndpoint.h
    CompositorWebContentClientEndpoint.h
    CompositorWebContentServerEndpoint.h
)

add_library(compositorservice STATIC ${SOURCES} ${GENERATED_SOURCES})
ladybird_generated_sources(compositorservice)

add_executable(Compositor main.cpp)

if (LINUX)
    target_sources(Compositor PRIVATE SandboxLinux.cpp)
elseif (APPLE)
    target_sources(Compositor PRIVATE SandboxMacOS.cpp)
else()
    target_sources(Compositor PRIVATE SandboxUnimplemented.cpp)
endif()

target_include_directories(compositorservice PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../..)
target_include_directories(compositorservice PRIVATE ${LADYBIRD_SOURCE_DIR}/Services/)

target_link_libraries(Compositor PRIVATE compositorservice LibCore LibMain LibSandbox)
# trinity draws (webgfx); WebGL is not ported, so no GL library
target_link_libraries(compositorservice PRIVATE LibCore LibGfx LibIPC LibMedia LibCompositing webgfx)

if (APPLE)
    target_link_libraries(compositorservice PUBLIC "-framework CoreGraphics" "-framework CoreVideo")
endif()

if (WIN32)
    target_include_directories(Compositor PRIVATE $<BUILD_INTERFACE:${PTHREAD_INCLUDE_DIR}>)
    target_include_directories(compositorservice PRIVATE $<BUILD_INTERFACE:${PTHREAD_INCLUDE_DIR}>)
    ladybird_windows_bin(Compositor CONSOLE)
endif()

ladybird_sign_helper_process(Compositor)
