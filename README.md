# char-clap-utils

Small C++17 helpers for plug-ins that implement CLAP directly.
Your plug-in owns its processor, parameters, callbacks, state, and UI.

Headers live in `include/char_clap_utils/`, in the `char_clap` namespace:

- `Process.h`: non-owning views of CLAP audio buffers and process blocks.
- `Events.h`: access to CLAP input and output events.
- `EventChunks.h`: split rendering at event timestamps without copying events.
- `ParameterState.h`: parameter publication between threads, modulation, and timed ramps.
- `Streams.h`: complete CLAP stream reads and writes.
- `WebUI.h`: WebView resources, messages, and CLAP GUI lifecycle support.
- `NativeWebView.h`: optional macOS WKWebView adapter; requires compiled code and CHOC.
- `AUv3Ramp.h`: AUv3 ramp events; requires the `charCulbert/clap-wrapper` extension headers.

## Usage

Add this repository as a submodule:

```sh
git submodule add https://github.com/charCulbert/char-clap-utils.git external/char-clap-utils
```

Add `external/char-clap-utils/include` and the CLAP SDK's `include` directory
to your compiler's include paths, then include the headers you need:

```cpp
#include <char_clap_utils/ParameterState.h>
#include <char_clap_utils/EventChunks.h>
```

The core helpers are header-only and require no CMake integration.

## Optional web UI build helpers

For native macOS WebViews, enable `OBJCXX` in your project and use:

```cmake
include(external/char-clap-utils/cmake/CharClapWebUI.cmake)
char_clap_add_native_webview(my_plugin "${CHOC_ROOT}")
```

This compiles the adapter and links AppKit and WebKit. The same file provides
`char_clap_stage_web_ui` to copy your UI and selected Compost components into
plug-in resources. Neither helper is needed to use the headers.

To run the tests, set `CLAP_SDK_ROOT` to a CLAP SDK checkout with the WebView
extension headers, then:

```sh
c++ -std=c++17 -pthread -Iinclude -I"$CLAP_SDK_ROOT/include" \
  tests/char_clap_utils_tests.cpp -o /tmp/char-clap-utils-tests
/tmp/char-clap-utils-tests
```
