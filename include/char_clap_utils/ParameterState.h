#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace char_clap
{

namespace detail
{

class PublishedDouble
{
    static_assert(std::atomic<uint32_t>::is_always_lock_free);

public:
    explicit PublishedDouble(double initialValue = 0.0) noexcept
    {
        uint64_t bits = 0;
        std::memcpy(&bits, &initialValue, sizeof(bits));
        low.store(static_cast<uint32_t>(bits), std::memory_order_relaxed);
        high.store(static_cast<uint32_t>(bits >> 32), std::memory_order_relaxed);
    }

    PublishedDouble(const PublishedDouble&) = delete;
    PublishedDouble& operator=(const PublishedDouble&) = delete;

    // One writer, any number of readers.
    void store(double value) noexcept
    {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));

        const auto odd = sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        // Acquiring either payload word must also expose the in-progress sequence.
        low.store(static_cast<uint32_t>(bits), std::memory_order_release);
        high.store(static_cast<uint32_t>(bits >> 32), std::memory_order_release);
        sequence.store(odd + 1, std::memory_order_release);
    }

    [[nodiscard]] bool tryLoad(double& value) const noexcept
    {
        uint32_t revision = 0;
        return tryLoad(value, revision);
    }

    // One attempt; a reader must not span 2^31 stores (revision wraparound).
    [[nodiscard]] bool tryLoad(double& value, uint32_t& revision) const noexcept
    {
        const auto before = sequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0)
            return false;

        const auto lowBits = low.load(std::memory_order_acquire);
        const auto highBits = high.load(std::memory_order_acquire);
        if (before != sequence.load(std::memory_order_acquire))
            return false;

        const uint64_t bits = (static_cast<uint64_t>(highBits) << 32) | lowBits;
        std::memcpy(&value, &bits, sizeof(value));
        revision = before;
        return true;
    }

    // Non-real-time convenience; audio callers must use tryLoad().
    [[nodiscard]] double load() const noexcept
    {
        double value = 0.0;
        while (!tryLoad(value)) {}
        return value;
    }

private:
    std::atomic<uint32_t> sequence { 0 };
    std::atomic<uint32_t> low { 0 };
    std::atomic<uint32_t> high { 0 };
};

} // namespace detail

// publishBase() and baseValueForMainThread() belong to the main thread.
// Other operations belong to the audio thread, or the main thread while stopped.
class ParameterState
{
public:
    ParameterState(double minimum, double maximum, double defaultValue) noexcept
        : minimum(minimum),
          maximum(maximum),
          publishedBase(clamp(defaultValue)),
          audioBase(clamp(defaultValue)),
          mainBase(clamp(defaultValue)),
          lastReportedBase(clamp(defaultValue)),
          automated(clamp(defaultValue)),
          rendered(clamp(defaultValue)),
          rampTarget(clamp(defaultValue))
    {
    }

    ParameterState(const ParameterState&) = delete;
    ParameterState& operator=(const ParameterState&) = delete;

    [[nodiscard]] double clamp(double value) const noexcept
    {
        if (!std::isfinite(value))
            value = minimum;
        return std::clamp(value, minimum, maximum);
    }

    void publishBase(double value) noexcept
    {
        const auto clamped = clamp(value);
        publishedBase.store(clamped);
        mainBase = lastReportedBase = clamped;
        latestIsMain.store(1, std::memory_order_release);
    }

    [[nodiscard]] bool consumePublishedBase() noexcept
    {
        double value = 0.0;
        uint32_t generation = 0;
        if (!publishedBase.tryLoad(value, generation) || generation == consumedGeneration)
            return false;

        consumedGeneration = generation;
        setAutomatedBase(value);
        return true;
    }

    void setAutomatedBase(double value) noexcept
    {
        automated = rendered = clamp(value);
        rampFrames = 0;
        finishRampOnNextSample = false;
        audioBase.store(automated);
        latestIsMain.store(0, std::memory_order_release);
    }

    void setGlobalModulation(double amount) noexcept { modulation = amount; }

    void beginTimedRamp(double target, uint32_t durationFrames) noexcept
    {
        automated = rendered;
        rampTarget = clamp(target);
        rampFrames = durationFrames;
        finishRampOnNextSample = false;
        rampIncrement = durationFrames == 0
                            ? 0.0
                            : (rampTarget - automated) / static_cast<double>(durationFrames);

        if (durationFrames == 0)
            automated = rendered = rampTarget;

        audioBase.store(rampTarget);
        latestIsMain.store(0, std::memory_order_release);
    }

    [[nodiscard]] double nextValue() noexcept
    {
        advanceRamp();
        return clamp(rendered + modulation);
    }

    [[nodiscard]] double baseValueForMainThread() const noexcept
    {
        if (latestIsMain.load(std::memory_order_acquire) != 0)
            return mainBase;

        // Keep the last stable value if an audio-thread write is in progress.
        (void) audioBase.tryLoad(lastReportedBase);
        return lastReportedBase;
    }

private:
    void advanceRamp() noexcept
    {
        if (finishRampOnNextSample)
        {
            rendered = rampTarget;
            finishRampOnNextSample = false;
        }

        if (rampFrames == 0)
            return;

        rendered = automated;
        automated += rampIncrement;
        --rampFrames;

        if (rampFrames == 0)
        {
            automated = rampTarget;
            finishRampOnNextSample = true;
        }
    }

    const double minimum;
    const double maximum;
    detail::PublishedDouble publishedBase;
    detail::PublishedDouble audioBase;
    double mainBase;
    mutable double lastReportedBase;
    std::atomic<uint32_t> latestIsMain { 1 };
    uint32_t consumedGeneration = 0;
    double automated;
    double rendered;
    double modulation = 0.0;
    double rampTarget;
    double rampIncrement = 0.0;
    uint32_t rampFrames = 0;
    bool finishRampOnNextSample = false;
};

} // namespace char_clap
