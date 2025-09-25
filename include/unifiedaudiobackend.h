/*
 * Fujisan - Modern Atari Emulator
 * Unified Audio Backend with Lockless Ring Buffer
 */

#ifndef UNIFIEDAUDIOBACKEND_H
#define UNIFIEDAUDIOBACKEND_H

#include <QObject>
#include <SDL.h>
#include <atomic>
#include <memory>

class UnifiedAudioBackend : public QObject
{
    Q_OBJECT

public:
    enum AudioPriority {
        LOW_PRIORITY = 0,   // Background music, game audio
        HIGH_PRIORITY = 1   // UI sounds, beeps, key clicks
    };

    explicit UnifiedAudioBackend(QObject *parent = nullptr);
    ~UnifiedAudioBackend();

    // Initialize audio with specified parameters
    bool initialize(int targetSampleRate = 44100, int channels = 2, int sampleSize = 2);

    // Shutdown audio
    void shutdown();

    // Control playback
    void pause();
    void resume();

    // Audio data submission (thread-safe, lockless)
    bool submitAudio(const unsigned char* data, int length, AudioPriority priority = LOW_PRIORITY);

    // Volume control (0.0 to 1.0)
    void setVolume(float volume);
    float getVolume() const { return m_volume.load(); }

    // Get current latency in milliseconds
    float getLatencyMs() const;

    // Get buffer status for monitoring
    float getBufferFillPercent() const;
    int getUnderrunCount() const { return m_underrunCount.load(); }
    int getOverrunCount() const { return m_overrunCount.load(); }

    // Reset statistics
    void resetStats();

    // Dynamic latency management (public for external monitoring)
    void checkPerformanceAndAdjust();
    void adjustLatencyForPerformance();

private:
    // SDL audio callback (static, calls instance method)
    static void sdlAudioCallback(void* userdata, Uint8* stream, int len);

    // Instance audio callback (real-time safe)
    void audioCallback(Uint8* stream, int len);

    // Sample rate conversion (real-time safe)
    void resample(const float* input, int inputFrames, float* output, int outputFrames, int channels);

    // High-quality resampling with linear interpolation
    void resampleLinear(const float* input, int inputFrames, float* output, int outputFrames, int channels);

    // Audio mixing (real-time safe)
    void mixChannels(float* output, int frames);

    // Ring buffer operations (lockless)
    bool writeToRingBuffer(const unsigned char* data, int length, AudioPriority priority);
    int readFromRingBuffer(unsigned char* data, int maxLength, AudioPriority priority);

    // Configuration
    bool m_initialized;
    SDL_AudioDeviceID m_audioDevice;
    SDL_AudioSpec m_audioSpec;

    // Audio parameters
    int m_targetSampleRate;
    int m_actualSampleRate;
    int m_channels;
    int m_sampleSize;
    int m_frameSize;  // bytes per frame (channels * sampleSize)

    // Volume control (atomic for thread safety)
    std::atomic<float> m_volume;

    // Ring buffers (separate for high/low priority)
    static const int RING_BUFFER_SIZE = 65536;  // 64KB per priority level

    struct RingBuffer {
        std::unique_ptr<unsigned char[]> data;
        std::atomic<int> writePos;
        std::atomic<int> readPos;
        std::atomic<int> size;

        RingBuffer() : writePos(0), readPos(0), size(0) {
            data = std::make_unique<unsigned char[]>(RING_BUFFER_SIZE);
        }
    };

    RingBuffer m_lowPriorityBuffer;
    RingBuffer m_highPriorityBuffer;

    // Resampling state
    float m_resampleRatio;
    float m_resamplePhase;
    std::unique_ptr<float[]> m_tempResampleBuffer;
    int m_tempResampleBufferSize;

    // Performance monitoring (atomic)
    std::atomic<int> m_underrunCount;
    std::atomic<int> m_overrunCount;
    std::atomic<long long> m_totalFramesProcessed;

    // Dynamic latency management
    int m_targetLatencyMs;
    int m_minLatencyMs;
    int m_maxLatencyMs;
    std::atomic<float> m_currentLatencyMs;

    // Performance monitoring for dynamic adjustment
    mutable std::atomic<long long> m_lastPerformanceCheck;
    std::atomic<int> m_consecutiveUnderruns;
    std::atomic<int> m_consecutiveOverruns;
    std::atomic<bool> m_needsLatencyAdjustment;

    // Platform-specific optimizations
    void setPlatformOptimizations();
    int getOptimalBufferSize() const;

    // Dynamic latency management methods (private implementation)
    bool shouldIncreaseLatency() const;
    bool shouldDecreaseLatency() const;
};

#endif // UNIFIEDAUDIOBACKEND_H