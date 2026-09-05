#include "char_clap_utils/EventChunks.h"
#include "char_clap_utils/ParameterState.h"
#include "char_clap_utils/Process.h"
#include "char_clap_utils/WebUI.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>
#include <vector>

namespace
{

struct InputEvents
{
    std::vector<const clap_event_header_t*> events;
    clap_input_events_t interface { this, size, get };

    static uint32_t CLAP_ABI size(const clap_input_events_t* list)
    {
        return static_cast<uint32_t>(static_cast<const InputEvents*>(list->ctx)->events.size());
    }

    static const clap_event_header_t* CLAP_ABI get(const clap_input_events_t* list,
                                                    uint32_t index)
    {
        return static_cast<const InputEvents*>(list->ctx)->events[index];
    }
};

void testEventChunks()
{
    clap_event_header_t first { sizeof(first), 2, CLAP_CORE_EVENT_SPACE_ID, 1, 0 };
    clap_event_header_t second { sizeof(second), 5, CLAP_CORE_EVENT_SPACE_ID, 2, 0 };
    InputEvents input { { &first, &second } };
    std::vector<uint16_t> order;

    char_clap::processEventChunks(
        char_clap::InputEventsView { &input.interface },
        8,
        [&](const clap_event_header_t& event) noexcept
        {
            order.push_back(static_cast<uint16_t>(100 + event.type));
        },
        [&](uint32_t begin, uint32_t end) noexcept
        {
            order.push_back(static_cast<uint16_t>(begin * 10 + end));
        });

    assert((order == std::vector<uint16_t> { 2, 101, 25, 102, 58 }));
}

void testParameterState()
{
    char_clap::ParameterState parameter { 0.0, 1.0, 0.25 };
    assert(!parameter.consumePublishedBase());
    assert(parameter.baseValueForMainThread() == 0.25);
    parameter.publishBase(0.5);
    assert(parameter.baseValueForMainThread() == 0.5);
    assert(parameter.consumePublishedBase());
    assert(!parameter.consumePublishedBase());
    parameter.setGlobalModulation(0.1);
    assert(std::abs(parameter.nextValue() - 0.6) < 1.0e-12);

    parameter.setGlobalModulation(0.0);
    parameter.beginTimedRamp(1.0, 2);
    assert(parameter.baseValueForMainThread() == 1.0);
    assert(std::abs(parameter.nextValue() - 0.5) < 1.0e-12);
    assert(std::abs(parameter.nextValue() - 0.75) < 1.0e-12);
    assert(std::abs(parameter.nextValue() - 1.0) < 1.0e-12);

    parameter.publishBase(0.2);
    parameter.publishBase(0.3);
    assert(parameter.consumePublishedBase());
    assert(parameter.nextValue() == 0.3);
    parameter.setAutomatedBase(0.7);
    assert(!parameter.consumePublishedBase());
    assert(parameter.baseValueForMainThread() == 0.7);
    parameter.publishBase(0.4);
    assert(parameter.baseValueForMainThread() == 0.4);
}

// Both halves change, so a torn read cannot accidentally equal a published value.
double snapshotValue(uint32_t index)
{
    const uint64_t bits = 0x3ff0000000000000ULL | (static_cast<uint64_t>(index) << 32)
                          | (index ^ 0xa5a5a5a5u);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void testConcurrentSnapshots()
{
    constexpr uint32_t iterations = 200000;
    char_clap::detail::PublishedDouble published(snapshotValue(0));
    std::atomic<uint32_t> ready { 0 };
    std::atomic<bool> start { false }, done { false };

    auto read = [&]
    {
        ready.fetch_add(1);
        while (!start.load()) std::this_thread::yield();
        do
        {
            double value = 0.0;
            uint32_t revision = 0;
            if (published.tryLoad(value, revision))
            {
                assert((revision & 1u) == 0);
                assert(value == snapshotValue(revision / 2));
            }
        } while (!done.load());
    };

    std::thread first(read), second(read);
    while (ready.load() != 2) std::this_thread::yield();
    start.store(true);
    for (uint32_t i = 1; i <= iterations; ++i)
        published.store(snapshotValue(i));
    done.store(true);
    first.join();
    second.join();

    double value = 0.0;
    uint32_t revision = 0;
    assert(published.tryLoad(value, revision));
    assert(value == snapshotValue(iterations) && revision == iterations * 2);
}

void testConcurrentParameters()
{
    constexpr uint32_t iterations = 100000;
    char_clap::ParameterState parameter { -double(iterations), double(iterations), 0.0 };
    std::atomic<bool> started { false }, mainDone { false };

    std::thread audio([&]
    {
        double previous = 0.0;
        started.store(true);
        while (previous < iterations)
        {
            if (parameter.consumePublishedBase())
            {
                const auto value = parameter.nextValue();
                // Publications may coalesce, but must never be replayed.
                assert(value > previous && value <= iterations && std::floor(value) == value);
                previous = value;
                parameter.setAutomatedBase(-value);
            }
            assert(parameter.nextValue() == -previous);
        }
        while (!mainDone.load()) std::this_thread::yield();
        parameter.setAutomatedBase(-previous);
    });

    while (!started.load()) std::this_thread::yield();
    for (uint32_t i = 1; i <= iterations; ++i)
    {
        parameter.publishBase(i);
        const auto displayed = parameter.baseValueForMainThread();
        assert(std::isfinite(displayed) && std::abs(displayed) <= iterations);
    }
    mainDone.store(true);
    audio.join();
    assert(parameter.baseValueForMainThread() == -double(iterations));
}

void testWasmResource()
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("char-clap-utils-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    const std::string bytes("\0asm\1\0\0\0", 8);
    {
        std::ofstream file(directory / "editor.wasm", std::ios::binary);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    std::string received;
    clap_ostream_t stream {
        &received,
        [](const clap_ostream_t* output, const void* data, uint64_t size) -> int64_t
        {
            static_cast<std::string*>(output->ctx)->append(static_cast<const char*>(data), size);
            return static_cast<int64_t>(size);
        }
    };
    const auto previousRoot = char_clap::resourceRoot;
    char_clap::resourceRoot = directory.string();
    char_clap::WebUI ui(nullptr, {});
    char mime[64] {};
    const auto served = ui.getResource("/editor.wasm", mime, sizeof(mime), &stream);
    char_clap::resourceRoot = previousRoot;
    std::filesystem::remove_all(directory);

    assert(served);
    assert(std::string(mime) == "application/wasm");
    assert(received == bytes);
}

} // namespace

int main()
{
    testEventChunks();
    testParameterState();
    testConcurrentSnapshots();
    testConcurrentParameters();
    testWasmResource();
}
