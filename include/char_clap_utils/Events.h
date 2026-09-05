#pragma once

#include <clap/clap.h>

#include <cstdint>

namespace char_clap
{

class InputEventsView
{
public:
    constexpr InputEventsView() noexcept = default;
    explicit constexpr InputEventsView(const clap_input_events_t* events) noexcept
        : events(events)
    {
    }

    [[nodiscard]] uint32_t size() const noexcept
    {
        return events != nullptr && events->size != nullptr ? events->size(events) : 0;
    }

    [[nodiscard]] const clap_event_header_t* operator[](uint32_t index) const noexcept
    {
        return events != nullptr && events->get != nullptr && index < size()
                   ? events->get(events, index)
                   : nullptr;
    }

private:
    const clap_input_events_t* events = nullptr;
};

class OutputEventsView
{
public:
    constexpr OutputEventsView() noexcept = default;
    explicit constexpr OutputEventsView(const clap_output_events_t* events) noexcept
        : events(events)
    {
    }

    [[nodiscard]] bool tryPush(const clap_event_header_t& event) const noexcept
    {
        return events != nullptr && events->try_push != nullptr
               && events->try_push(events, &event);
    }

    template <typename Event>
    [[nodiscard]] bool tryPush(const Event& event) const noexcept
    {
        return tryPush(event.header);
    }

private:
    const clap_output_events_t* events = nullptr;
};

} // namespace char_clap
