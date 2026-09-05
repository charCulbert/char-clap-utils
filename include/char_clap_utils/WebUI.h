#pragma once

#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
#include "char_clap_utils/NativeWebView.h"
#endif

#include <clap/clap.h>
#include <clap/ext/draft/webview.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#if defined(__APPLE__)
#include <filesystem>
#endif
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <limits.h>
#include <mach-o/dyld.h>
#endif

namespace char_clap
{

inline std::string resourceRoot;

inline bool setResourceRoot(const char* pluginPath)
{
    if (pluginPath == nullptr)
        return false;

    resourceRoot = pluginPath;
#if defined(__APPLE__)
    constexpr std::string_view executableMarker = "/Contents/MacOS/";
    constexpr std::string_view extensionMarker = ".appex/";
    if (resourceRoot.find('/') == std::string::npos)
    {
        std::vector<char> executablePath(PATH_MAX);
        uint32_t size = static_cast<uint32_t>(executablePath.size());
        if (_NSGetExecutablePath(executablePath.data(), &size) == 0)
            resourceRoot = executablePath.data();
    }

#if TARGET_OS_IPHONE
    if (const auto separator = resourceRoot.find_last_of('/'); separator != std::string::npos)
        resourceRoot.resize(separator);
#else
    if (const auto marker = resourceRoot.find(executableMarker); marker != std::string::npos)
        resourceRoot = resourceRoot.substr(0, marker) + "/Contents/Resources";
    else if (const auto marker = resourceRoot.find(extensionMarker); marker != std::string::npos)
    {
        resourceRoot.resize(marker + extensionMarker.size() - 1);
        resourceRoot += "/Contents/Resources";
    }
    else
        resourceRoot += "/Contents/Resources";
#endif
#endif
    return true;
}

class WebUI
{
public:
    using Receive = std::function<bool(std::string_view)>;

    WebUI(const clap_host_t* host, Receive receive)
        : host(host), receive(std::move(receive))
    {
        if (host != nullptr)
            hostWebview = static_cast<const clap_host_webview_t*>(
                host->get_extension(host, CLAP_EXT_WEBVIEW));
    }

    int32_t getUri(char* destination, uint32_t capacity) const noexcept
    {
        constexpr std::string_view uri = "/ui/index.html";
        if (destination != nullptr && capacity != 0)
            std::snprintf(destination, capacity, "%s", uri.data());
        return static_cast<int32_t>(uri.size() + 1);
    }

    bool getResource(const char* requestedPath,
                     char* mime,
                     uint32_t mimeCapacity,
                     const clap_ostream_t* stream) const
    {
        if (requestedPath == nullptr || stream == nullptr)
            return false;

        std::string path = requestedPath;
        while (!path.empty() && path.front() == '/')
            path.erase(path.begin());
        if (path.empty() || path.find("..") != std::string::npos)
            return false;

        // A directory opens successfully as an ifstream on macOS and then reads as
        // zero bytes, which would be served as an empty octet-stream response.
        const auto fullPath = resourceRoot + "/" + path;
#if defined(__APPLE__)
        if (!isRegularFile(fullPath))
            return false;
#endif

        auto filePath = fullPath;
#if defined(__wasi__)
        if (!filePath.empty() && filePath.front() == '/')
            filePath.erase(filePath.begin());
#endif
        std::ifstream input(filePath, std::ios::binary);
        if (!input)
            return false;

        std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
        auto mediaType = "application/octet-stream";
        if (endsWith(path, ".html")) mediaType = "text/html";
        else if (endsWith(path, ".js")) mediaType = "text/javascript";
        else if (endsWith(path, ".css")) mediaType = "text/css";
        else if (endsWith(path, ".wasm")) mediaType = "application/wasm";

        if (mime != nullptr && mimeCapacity != 0)
            std::snprintf(mime, mimeCapacity, "%s", mediaType);
        return bytes.empty()
               || stream->write(stream, bytes.data(), bytes.size())
                      == static_cast<int64_t>(bytes.size());
    }

    bool receiveBytes(const void* data, uint32_t size) const
    {
        return data != nullptr && receive
               && receive(std::string_view(static_cast<const char*>(data), size));
    }

    bool send(std::string_view message) const
    {
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        if (nativeWebview)
            return nativeWebview->send(message.data(), static_cast<uint32_t>(message.size()));
#endif
        return hostWebview != nullptr
               && hostWebview->send(host, message.data(), static_cast<uint32_t>(message.size()));
    }

    bool guiIsApiSupported(const char* api, bool floating) const noexcept
    {
        if (api == nullptr || floating)
            return false;
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        if (std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0)
            return true;
#endif
        // Only offer the webview window api to a host that hosts the webview
        // itself. A host without CLAP_EXT_WEBVIEW must be given a native view,
        // otherwise guiCreate succeeds without producing one and nothing shows.
        return hostWebview != nullptr && std::strcmp(api, CLAP_WINDOW_API_WEBVIEW) == 0;
    }

    bool guiGetPreferredApi(const char** api, bool* floating) const noexcept
    {
        if (api == nullptr || floating == nullptr)
            return false;
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        *api = CLAP_WINDOW_API_COCOA;
#else
        *api = CLAP_WINDOW_API_WEBVIEW;
#endif
        *floating = false;
        return true;
    }

    bool guiCreate(const char* api, bool floating, uint32_t width, uint32_t height)
    {
        if (!guiIsApiSupported(api, floating))
            return false;

        viewWidth = width;
        viewHeight = height;
        if (std::strcmp(api, CLAP_WINDOW_API_WEBVIEW) == 0)
            return true;

#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        nativeWebview = std::make_unique<NativeWebView>(
            "/ui/index.html",
            [this](const std::string& path) { return fetchNativeResource(path); },
            [this](const void* data, uint32_t size) { return receiveBytes(data, size); });
        if (!nativeWebview->isOpen())
        {
            nativeWebview.reset();
            return false;
        }
        nativeWebview->setSize(viewWidth, viewHeight);
        return true;
#else
        return false;
#endif
    }

    void guiDestroy() noexcept
    {
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        nativeWebview.reset();
#endif
    }

    bool guiShow() noexcept
    {
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        if (nativeWebview)
            nativeWebview->setVisible(true);
#endif
        return true;
    }

    bool guiHide() noexcept
    {
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        if (nativeWebview)
            nativeWebview->setVisible(false);
#endif
        return true;
    }

    bool guiGetSize(uint32_t* width, uint32_t* height) const noexcept
    {
        if (width == nullptr || height == nullptr)
            return false;
        *width = viewWidth;
        *height = viewHeight;
        return true;
    }

    bool guiSetSize(uint32_t width, uint32_t height) noexcept
    {
        viewWidth = width;
        viewHeight = height;
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        if (nativeWebview)
            nativeWebview->setSize(width, height);
#endif
        return true;
    }

    bool guiSetParent(const clap_window_t* window)
    {
        if (window == nullptr || window->api == nullptr)
            return false;
        if (std::strcmp(window->api, CLAP_WINDOW_API_WEBVIEW) == 0)
            return window->ptr == nullptr;
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
        return std::strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0
               && nativeWebview && nativeWebview->attach(window->ptr);
#else
        return false;
#endif
    }

private:
    static bool endsWith(const std::string& text, std::string_view suffix)
    {
        return text.size() >= suffix.size()
               && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

#if defined(__APPLE__)
    static bool isRegularFile(const std::string& path)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error);
    }
#endif

    const clap_host_t* host = nullptr;
    const clap_host_webview_t* hostWebview = nullptr;
    Receive receive;
    uint32_t viewWidth = 800;
    uint32_t viewHeight = 500;
#if defined(CHAR_CLAP_HAS_NATIVE_WEBVIEW)
    std::unique_ptr<NativeWebView> nativeWebview;

    std::optional<NativeWebView::Resource> fetchNativeResource(const std::string& path) const
    {
        struct Stream
        {
            clap_ostream_t interface { this, write };
            std::vector<uint8_t> bytes;

            static int64_t CLAP_ABI write(const clap_ostream_t* stream,
                                          const void* data,
                                          uint64_t size)
            {
                auto& self = *static_cast<Stream*>(stream->ctx);
                const auto* first = static_cast<const uint8_t*>(data);
                self.bytes.insert(self.bytes.end(), first, first + size);
                return static_cast<int64_t>(size);
            }
        } stream;

        char mime[256] {};
        if (!getResource(path.c_str(), mime, sizeof(mime), &stream.interface))
            return std::nullopt;
        return NativeWebView::Resource { std::move(stream.bytes), mime };
    }
#endif
};

} // namespace char_clap
