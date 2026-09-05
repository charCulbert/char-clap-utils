#pragma once

#include <clap/clap.h>
#include <clapwrapper/auv3-param-ramp.h>

#include <cstdint>
#include <cstring>

namespace char_clap
{

constexpr const char rampEventSpaceName[] = "com.charlieculbert.char-clap-utils.ramp";
constexpr uint16_t rampEventType = 0;

struct RampEvent
{
    clap_event_header_t header;
    clap_id parameterId;
    double target;
    uint32_t durationFrames;
};

static_assert(sizeof(RampEvent) <= 64);

inline bool writeRampEvent(uint16_t eventSpace,
                           const clap_wrapper_auv3_param_ramp_info_t& ramp,
                           void* storage,
                           uint32_t capacity,
                           uint32_t& eventSize) noexcept
{
    if (eventSpace == 0 || eventSpace == UINT16_MAX || storage == nullptr
        || capacity < sizeof(RampEvent))
        return false;

    const RampEvent event {
        { sizeof(RampEvent), ramp.sample_offset, eventSpace, rampEventType, 0 },
        ramp.parameter_id,
        ramp.target_value,
        ramp.duration_sample_frames
    };
    std::memcpy(storage, &event, sizeof(event));
    eventSize = sizeof(event);
    return true;
}

} // namespace char_clap
