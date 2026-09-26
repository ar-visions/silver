// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "RemoteMediaPlayerProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO) && !USE(GSTREAMER)

#include <WebCore/NotImplemented.h>

namespace WebKit {

void RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable()
{
    notImplemented();
}

} // namespace WebKit

#endif
