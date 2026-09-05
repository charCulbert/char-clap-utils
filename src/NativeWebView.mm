#include "char_clap_utils/NativeWebView.h"

#include <choc/gui/choc_WebView.h>

#import <AppKit/AppKit.h>

#include <sstream>

namespace char_clap
{

namespace
{
// WebKit hands the scheme handler the URL's normalised path, which drops the
// trailing slash: navigating to "charclap://ui/ui/" arrives here as "/ui".
// Treat both spellings as a request for the directory's index document.
bool isDirectoryRequest(const std::string& path, const std::string& directory)
{
    if (path == directory)
        return true;
    return path.size() + 1 == directory.size() && directory.compare(0, path.size(), path) == 0;
}
} // namespace

struct NativeWebView::Impl
{
    Impl(std::string uri, FetchResource fetchResource, Receive receive)
    {
        choc::ui::WebView::Options options;
        const auto slash = uri.find_last_of('/');
        const auto directory = slash == std::string::npos ? std::string("/") : uri.substr(0, slash + 1);
        options.customSchemeURI = "charclap://ui" + directory;
        options.fetchResource = [fetch = std::move(fetchResource), directory, uri]
            (const std::string& path)
            -> std::optional<choc::ui::WebView::Options::Resource>
        {
            const auto resource = fetch(isDirectoryRequest(path, directory) ? uri : path);
            if (!resource)
                return std::nullopt;

            choc::ui::WebView::Options::Resource result;
            result.data = resource->data;
            result.mimeType = resource->mimeType;
            return result;
        };
        options.webviewIsReady = [this, receive = std::move(receive)]
            (choc::ui::WebView& view)
        {
            view.bind("clapPluginPostMessage",
                [receive](const choc::value::ValueView& arguments)
                {
                    if (!arguments.isArray() || arguments.size() != 1 || !arguments[0].isArray())
                        return choc::value::Value(false);

                    const auto source = arguments[0];
                    std::vector<uint8_t> bytes(source.size());
                    for (uint32_t i = 0; i < source.size(); ++i)
                        bytes[i] = static_cast<uint8_t>(source[i].getWithDefault<int32_t>(0));

                    return choc::value::Value(receive(bytes.data(), static_cast<uint32_t>(bytes.size())));
            });
            view.addInitScript(messageBridgeScript);
        };

        webview = std::make_unique<choc::ui::WebView>(options);
        if (!webview->loadedOK())
            webview.reset();
    }

    ~Impl()
    {
        if (auto* view = nativeView())
            [view removeFromSuperview];
    }

    [[nodiscard]] NSView* nativeView() const noexcept
    {
        return webview ? (__bridge NSView*) webview->getViewHandle() : nil;
    }

    static constexpr const char* messageBridgeScript = R"JS(
(() => {
  window.addEventListener('message', event => {
    if (event.source !== window)
      return;
    let bytes;
    if (event.data instanceof ArrayBuffer)
      bytes = new Uint8Array(event.data);
    else if (ArrayBuffer.isView(event.data))
      bytes = new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength);
    else
      return;
    event.stopImmediatePropagation();
    clapPluginPostMessage(Array.from(bytes));
  }, { capture: true });
})();
)JS";

    std::unique_ptr<choc::ui::WebView> webview;
};

NativeWebView::NativeWebView(std::string uri, FetchResource fetchResource, Receive receive)
    : impl(std::make_unique<Impl>(std::move(uri), std::move(fetchResource), std::move(receive)))
{
}

NativeWebView::~NativeWebView() = default;

bool NativeWebView::isOpen() const noexcept
{
    return impl && impl->webview;
}

bool NativeWebView::attach(void* parent)
{
    if (!isOpen() || parent == nullptr)
        return false;

    auto* parentView = (__bridge NSView*) parent;
    auto* childView = impl->nativeView();
    [childView setFrame:[parentView bounds]];
    [childView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [parentView addSubview:childView];
    return true;
}

bool NativeWebView::send(const void* data, uint32_t size)
{
    if (!isOpen() || (data == nullptr && size != 0))
        return false;

    const auto* bytes = static_cast<const uint8_t*>(data);
    std::ostringstream script;
    script << "window.dispatchEvent(new MessageEvent('message',{data:new Uint8Array([";
    for (uint32_t i = 0; i < size; ++i)
    {
        if (i != 0)
            script << ',';
        script << static_cast<uint32_t>(bytes[i]);
    }
    script << "]).buffer}));";
    return impl->webview->evaluateJavascript(script.str());
}

void NativeWebView::setSize(uint32_t width, uint32_t height)
{
    if (auto* view = impl ? impl->nativeView() : nil)
        [view setFrameSize:NSMakeSize(width, height)];
}

void NativeWebView::setVisible(bool visible)
{
    if (auto* view = impl ? impl->nativeView() : nil)
        [view setHidden:!visible];
}

} // namespace char_clap
