#pragma once

#include "char_clap_utils/Events.h"

#include <cstdint>
#include <utility>

namespace char_clap
{

// Applies events before rendering the sample at their timestamp.
template <typename HandleEvent, typename RenderRange>
inline void processEventChunks(const InputEventsView& events,
                               uint32_t frameCount,
                               HandleEvent&& handleEvent,
                               RenderRange&& renderRange)
    noexcept(noexcept(handleEvent(std::declval<const clap_event_header_t&>()))
             && noexcept(renderRange(uint32_t {}, uint32_t {})))
{
    if (frameCount == 0)
        return;

    const auto eventCount = events.size();
    if (eventCount == 0)
    {
        renderRange(0, frameCount);
        return;
    }

    uint32_t frame = 0;
    uint32_t eventIndex = 0;

    while (eventIndex < eventCount)
    {
        auto* event = events[eventIndex];
        if (event == nullptr)
        {
            ++eventIndex;
            continue;
        }

        if (event->time >= frameCount)
            break;

        if (event->time > frame)
        {
            renderRange(frame, event->time);
            frame = event->time;
        }

        while (eventIndex < eventCount)
        {
            event = events[eventIndex];
            if (event == nullptr)
            {
                ++eventIndex;
                continue;
            }

            if (event->time > frame)
                break;

            handleEvent(*event);
            ++eventIndex;
        }
    }

    if (frame < frameCount)
        renderRange(frame, frameCount);
}

} // namespace char_clap
