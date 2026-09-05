#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace char_clap
{

class NativeWebView
{
public:
    struct Resource
    {
        std::vector<uint8_t> data;
        std::string mimeType;
    };

    using FetchResource = std::function<std::optional<Resource>(const std::string&)>;
    using Receive = std::function<bool(const void*, uint32_t)>;

    NativeWebView(std::string uri, FetchResource fetchResource, Receive receive);
    ~NativeWebView();

    NativeWebView(const NativeWebView&) = delete;
    NativeWebView& operator=(const NativeWebView&) = delete;

    [[nodiscard]] bool isOpen() const noexcept;
    bool attach(void* parent);
    bool send(const void* data, uint32_t size);
    void setSize(uint32_t width, uint32_t height);
    void setVisible(bool visible);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace char_clap
