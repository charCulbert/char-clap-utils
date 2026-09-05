#pragma once

#include "char_clap_utils/Events.h"

#include <clap/clap.h>

#include <cstdint>
#include <type_traits>

namespace char_clap
{

template <typename Sample>
class AudioInputBusView
{
    static_assert(std::is_same_v<Sample, float> || std::is_same_v<Sample, double>);

public:
    constexpr AudioInputBusView(const clap_audio_buffer_t& buffer, uint32_t frames) noexcept
        : buffer(&buffer), frames(frames)
    {
    }

    [[nodiscard]] constexpr uint32_t channelCount() const noexcept
    {
        return buffer->channel_count;
    }

    [[nodiscard]] constexpr uint32_t frameCount() const noexcept { return frames; }

    [[nodiscard]] const Sample* channel(uint32_t index) const noexcept
    {
        if (index >= channelCount())
            return nullptr;

        if constexpr (std::is_same_v<Sample, float>)
            return buffer->data32 != nullptr ? buffer->data32[index] : nullptr;
        else
            return buffer->data64 != nullptr ? buffer->data64[index] : nullptr;
    }

private:
    const clap_audio_buffer_t* buffer;
    uint32_t frames;
};

template <typename Sample>
class AudioOutputBusView
{
    static_assert(std::is_same_v<Sample, float> || std::is_same_v<Sample, double>);

public:
    constexpr AudioOutputBusView(clap_audio_buffer_t& buffer, uint32_t frames) noexcept
        : buffer(&buffer), frames(frames)
    {
    }

    [[nodiscard]] constexpr uint32_t channelCount() const noexcept
    {
        return buffer->channel_count;
    }

    [[nodiscard]] constexpr uint32_t frameCount() const noexcept { return frames; }

    [[nodiscard]] Sample* channel(uint32_t index) const noexcept
    {
        if (index >= channelCount())
            return nullptr;

        if constexpr (std::is_same_v<Sample, float>)
            return buffer->data32 != nullptr ? buffer->data32[index] : nullptr;
        else
            return buffer->data64 != nullptr ? buffer->data64[index] : nullptr;
    }

    void setConstantMask(uint64_t mask) const noexcept { buffer->constant_mask = mask; }

private:
    clap_audio_buffer_t* buffer;
    uint32_t frames;
};

class ProcessView
{
public:
    explicit constexpr ProcessView(const clap_process_t& process) noexcept
        : process(&process)
    {
    }

    [[nodiscard]] constexpr uint32_t frameCount() const noexcept
    {
        return process->frames_count;
    }

    [[nodiscard]] constexpr int64_t steadyTime() const noexcept
    {
        return process->steady_time;
    }

    [[nodiscard]] constexpr const clap_event_transport_t* transport() const noexcept
    {
        return process->transport;
    }

    template <typename Sample>
    [[nodiscard]] AudioInputBusView<Sample> audioInput(uint32_t index) const noexcept
    {
        return { process->audio_inputs[index], frameCount() };
    }

    template <typename Sample>
    [[nodiscard]] AudioOutputBusView<Sample> audioOutput(uint32_t index) const noexcept
    {
        return { process->audio_outputs[index], frameCount() };
    }

    [[nodiscard]] constexpr InputEventsView inputEvents() const noexcept
    {
        return InputEventsView { process->in_events };
    }

    [[nodiscard]] constexpr OutputEventsView outputEvents() const noexcept
    {
        return OutputEventsView { process->out_events };
    }

private:
    const clap_process_t* process;
};

} // namespace char_clap
