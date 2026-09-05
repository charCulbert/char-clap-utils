# char-clap-utils

Small C++17 helpers for plug-ins that implement CLAP directly.

This is not a plug-in framework. Products own their `clap_plugin_t`, extension
callbacks, processor, parameters, state, entry point, UI, and deployment.

Headers (`include/char_clap_utils/`, namespace `char_clap`):

- `Process.h`, `Events.h`, `EventChunks.h`: non-owning CLAP process and event
  views, event-boundary rendering without copying host events;
- `ParameterState.h`: lock-free parameter publication, global modulation, and
  exact timed ramps;
- `Streams.h`: complete CLAP stream reads and writes;
- `WebUI.h`: `clap.webview/3` resource serving from the bundle's `Resources`
  directory plus a `clap.gui` implementation; with `char-clap-utils::webview`
  it also opens a WKWebView for hosts that only offer a Cocoa window;
- `AUv3Ramp.h`: the ramp event written for the clap-wrapper
  `auv3-param-ramp` extension (needs the `charCulbert/clap-wrapper` headers).

CMake (`cmake/CharClapWebUI.cmake`):

- `char_clap_stage_web_ui(<target> SOURCE <ui dir> COMPOST_ROOT <compost>
  COMPONENTS knob scope ...)` stages the UI and the named
  [compost](https://github.com/charCulbert/compost) components into a
  resource directory for `make_clapfirst_plugins`.

```cmake
add_subdirectory(path/to/char-clap-utils)
target_link_libraries(my_plugin PRIVATE char-clap-utils::char-clap-utils)
# macOS only: WKWebView adapter, needs CHAR_CLAP_UTILS_CHOC_ROOT
target_link_libraries(my_plugin PRIVATE char-clap-utils::webview)
```

The consuming target must also make the CLAP SDK headers available.
`char-clap-utils::webview` is built by default on macOS and needs
`CHAR_CLAP_UTILS_CHOC_ROOT`; set `CHAR_CLAP_UTILS_NATIVE_WEBVIEW=OFF` to
skip it.
