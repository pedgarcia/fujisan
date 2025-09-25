/*
 * Fujisan - Modern Atari Emulator
 * Unified Audio Backend Implementation
 */

#include "../include/unifiedaudiobackend.h"
#include <QDebug>
#include <QDateTime>
#include <cstring>
#include <algorithm>
#include <cmath>

UnifiedAudioBackend::UnifiedAudioBackend(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_audioDevice(0)
    , m_targetSampleRate(44100)
    , m_actualSampleRate(44100)
    , m_channels(2)
    , m_sampleSize(2)
    , m_frameSize(4)
    , m_volume(1.0f)
    , m_resampleRatio(1.0f)
    , m_resamplePhase(0.0f)
    , m_tempResampleBufferSize(0)
    , m_underrunCount(0)
    , m_overrunCount(0)
    , m_totalFramesProcessed(0)
    , m_targetLatencyMs(25)  // 25ms default latency
    , m_minLatencyMs(10)
    , m_maxLatencyMs(100)
    , m_currentLatencyMs(25.0f)
    , m_lastPerformanceCheck(0)
    , m_consecutiveUnderruns(0)
    , m_consecutiveOverruns(0)
    , m_needsLatencyAdjustment(false)
{
    // Initialize SDL audio subsystem if not already done
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            qWarning() << "Failed to initialize SDL audio:" << SDL_GetError();
        }
    }
}

UnifiedAudioBackend::~UnifiedAudioBackend()
{
    shutdown();
}

bool UnifiedAudioBackend::initialize(int targetSampleRate, int channels, int sampleSize)
{
    if (m_initialized) {
        shutdown();
    }

    m_targetSampleRate = targetSampleRate;
    m_channels = channels;
    m_sampleSize = sampleSize;
    m_frameSize = channels * sampleSize;

    // Set platform-specific optimizations
    setPlatformOptimizations();

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    SDL_zero(obtained);

    desired.freq = m_targetSampleRate;
    desired.channels = m_channels;

    // Set format based on sample size
    if (sampleSize == 1) {
        desired.format = AUDIO_U8;
    } else if (sampleSize == 2) {
        desired.format = AUDIO_S16SYS;
    } else {
        qWarning() << "Unsupported sample size:" << sampleSize;
        return false;
    }

    // Calculate optimal buffer size for target latency
    int optimalBufferSize = getOptimalBufferSize();
    desired.samples = optimalBufferSize;

    // Set callback
    desired.callback = sdlAudioCallback;
    desired.userdata = this;

    // Open audio device
    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained,
                                       SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

    if (m_audioDevice == 0) {
        qWarning() << "Failed to open SDL audio device:" << SDL_GetError();
        return false;
    }

    // Store actual parameters
    m_audioSpec = obtained;
    m_actualSampleRate = obtained.freq;
    m_channels = obtained.channels;

    // Calculate resampling ratio
    m_resampleRatio = static_cast<float>(m_actualSampleRate) / static_cast<float>(m_targetSampleRate);

    // Allocate resampling buffer
    m_tempResampleBufferSize = (obtained.samples * m_channels * 2);  // Extra space for resampling
    m_tempResampleBuffer = std::make_unique<float[]>(m_tempResampleBufferSize);

    // Calculate actual latency
    float actualLatencyMs = (static_cast<float>(obtained.samples) * 1000.0f) / static_cast<float>(m_actualSampleRate);
    m_currentLatencyMs.store(actualLatencyMs);

    qDebug() << "Unified Audio Backend initialized:";
    qDebug() << "  Target sample rate:" << m_targetSampleRate << "Hz";
    qDebug() << "  Actual sample rate:" << m_actualSampleRate << "Hz";
    qDebug() << "  Channels:" << m_channels;
    qDebug() << "  Sample size:" << m_sampleSize << "bytes";
    qDebug() << "  Buffer samples:" << obtained.samples;
    qDebug() << "  Actual latency:" << actualLatencyMs << "ms";
    qDebug() << "  Resample ratio:" << m_resampleRatio;

    // Reset statistics
    resetStats();

    m_initialized = true;

    // Start audio playback
    SDL_PauseAudioDevice(m_audioDevice, 0);

    return true;
}

void UnifiedAudioBackend::shutdown()
{
    if (m_initialized) {
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
        m_initialized = false;
    }
}

void UnifiedAudioBackend::pause()
{
    if (m_initialized) {
        SDL_PauseAudioDevice(m_audioDevice, 1);
    }
}

void UnifiedAudioBackend::resume()
{
    if (m_initialized) {
        SDL_PauseAudioDevice(m_audioDevice, 0);
    }
}

bool UnifiedAudioBackend::submitAudio(const unsigned char* data, int length, AudioPriority priority)
{
    if (!m_initialized || !data || length <= 0) {
        return false;
    }

    return writeToRingBuffer(data, length, priority);
}

void UnifiedAudioBackend::setVolume(float volume)
{
    m_volume.store(std::max(0.0f, std::min(1.0f, volume)));
}

float UnifiedAudioBackend::getLatencyMs() const
{
    return m_currentLatencyMs.load();
}

float UnifiedAudioBackend::getBufferFillPercent() const
{
    if (!m_initialized) return 0.0f;

    // Calculate combined buffer fill level
    int lowPriorityFill = m_lowPriorityBuffer.size.load();
    int highPriorityFill = m_highPriorityBuffer.size.load();
    int totalFill = lowPriorityFill + highPriorityFill;
    int totalCapacity = RING_BUFFER_SIZE * 2;  // Both buffers

    return (static_cast<float>(totalFill) / static_cast<float>(totalCapacity)) * 100.0f;
}

void UnifiedAudioBackend::resetStats()
{
    m_underrunCount.store(0);
    m_overrunCount.store(0);
    m_totalFramesProcessed.store(0);
}

void UnifiedAudioBackend::sdlAudioCallback(void* userdata, Uint8* stream, int len)
{
    UnifiedAudioBackend* backend = static_cast<UnifiedAudioBackend*>(userdata);
    if (backend) {
        backend->audioCallback(stream, len);
    }
}

void UnifiedAudioBackend::audioCallback(Uint8* stream, int len)
{
    // Clear output buffer
    memset(stream, 0, len);

    if (!m_initialized) return;

    // Calculate destination samples (what SDL wants)
    int destinationSamples = len / (m_sampleSize * m_channels);
    m_totalFramesProcessed.fetch_add(destinationSamples);

    // Calculate source samples (what we need from emulator at target rate)
    int sourceSamples = static_cast<int>(destinationSamples / m_resampleRatio);
    int sourceFloatSamples = sourceSamples * m_channels;

    if (sourceFloatSamples > m_tempResampleBufferSize) {
        // This shouldn't happen with proper buffer sizing, but be safe
        sourceFloatSamples = m_tempResampleBufferSize;
        sourceSamples = sourceFloatSamples / m_channels;
    }

    // Clear temp buffer
    memset(m_tempResampleBuffer.get(), 0, sourceFloatSamples * sizeof(float));

    // Mix audio channels at emulator's sample rate (source rate)
    mixChannels(m_tempResampleBuffer.get(), sourceSamples);

    // Apply resampling if needed
    std::unique_ptr<float[]> resampledBuffer;
    float* finalBuffer = m_tempResampleBuffer.get();
    int finalSamples = sourceFloatSamples;

    if (std::abs(m_resampleRatio - 1.0f) > 0.001f) {
        // Need resampling - allocate output buffer
        int destinationFloatSamples = destinationSamples * m_channels;
        resampledBuffer = std::make_unique<float[]>(destinationFloatSamples);

        // Simple linear interpolation resampling
        for (int i = 0; i < destinationFloatSamples; i += m_channels) {
            float sourcePos = i * m_resampleRatio / m_channels;
            int sourceIndex = static_cast<int>(sourcePos) * m_channels;
            float fraction = sourcePos - static_cast<int>(sourcePos);

            for (int ch = 0; ch < m_channels; ch++) {
                if (sourceIndex + m_channels < sourceFloatSamples) {
                    // Linear interpolation
                    float sample1 = m_tempResampleBuffer[sourceIndex + ch];
                    float sample2 = m_tempResampleBuffer[sourceIndex + m_channels + ch];
                    resampledBuffer[i + ch] = sample1 + fraction * (sample2 - sample1);
                } else if (sourceIndex < sourceFloatSamples) {
                    // Use last available sample
                    resampledBuffer[i + ch] = m_tempResampleBuffer[sourceIndex + ch];
                } else {
                    // Silence for out-of-bounds
                    resampledBuffer[i + ch] = 0.0f;
                }
            }
        }

        finalBuffer = resampledBuffer.get();
        finalSamples = destinationFloatSamples;
    }

    // Apply volume
    float volume = m_volume.load();
    if (volume < 0.99f && volume > 0.01f) {
        for (int i = 0; i < finalSamples; i++) {
            finalBuffer[i] *= volume;
        }
    } else if (volume <= 0.01f) {
        // Muted - output is already silence
        return;
    }

    // Convert back to output format
    if (m_sampleSize == 1) {
        // 8-bit unsigned
        Uint8* output = stream;
        for (int i = 0; i < finalSamples; i++) {
            float sample = finalBuffer[i];
            sample = std::max(-1.0f, std::min(1.0f, sample));  // Clamp
            output[i] = static_cast<Uint8>((sample + 1.0f) * 127.5f);
        }
    } else if (m_sampleSize == 2) {
        // 16-bit signed
        Sint16* output = reinterpret_cast<Sint16*>(stream);
        for (int i = 0; i < finalSamples; i++) {
            float sample = finalBuffer[i];
            sample = std::max(-1.0f, std::min(1.0f, sample));  // Clamp
            output[i] = static_cast<Sint16>(sample * 32767.0f);
        }
    }
}

void UnifiedAudioBackend::mixChannels(float* output, int frames)
{
    // Read from high priority buffer first (beeps, UI sounds)
    unsigned char highPriorityData[8192];  // Stack buffer for real-time safety
    int highPriorityBytes = readFromRingBuffer(highPriorityData,
                                               std::min(static_cast<int>(sizeof(highPriorityData)),
                                                       frames * m_frameSize),
                                               HIGH_PRIORITY);

    // Read from low priority buffer (background audio)
    unsigned char lowPriorityData[8192];
    int lowPriorityBytes = readFromRingBuffer(lowPriorityData,
                                              std::min(static_cast<int>(sizeof(lowPriorityData)),
                                                      frames * m_frameSize),
                                              LOW_PRIORITY);

    // Convert and mix high priority audio
    if (highPriorityBytes > 0) {
        int highPriorityFrames = highPriorityBytes / m_frameSize;
        for (int frame = 0; frame < std::min(highPriorityFrames, frames); frame++) {
            for (int ch = 0; ch < m_channels; ch++) {
                int sampleIndex = frame * m_channels + ch;
                int byteIndex = frame * m_frameSize + ch * m_sampleSize;

                float sample = 0.0f;
                if (m_sampleSize == 1) {
                    sample = (static_cast<float>(highPriorityData[byteIndex]) - 128.0f) / 128.0f;
                } else if (m_sampleSize == 2) {
                    Sint16 sample16 = *reinterpret_cast<Sint16*>(&highPriorityData[byteIndex]);
                    sample = static_cast<float>(sample16) / 32768.0f;
                }

                output[sampleIndex] += sample;
            }
        }
    }

    // Convert and mix low priority audio (attenuated if high priority is present)
    if (lowPriorityBytes > 0) {
        int lowPriorityFrames = lowPriorityBytes / m_frameSize;
        float attenuation = (highPriorityBytes > 0) ? 0.3f : 1.0f;  // Duck background audio

        for (int frame = 0; frame < std::min(lowPriorityFrames, frames); frame++) {
            for (int ch = 0; ch < m_channels; ch++) {
                int sampleIndex = frame * m_channels + ch;
                int byteIndex = frame * m_frameSize + ch * m_sampleSize;

                float sample = 0.0f;
                if (m_sampleSize == 1) {
                    sample = (static_cast<float>(lowPriorityData[byteIndex]) - 128.0f) / 128.0f;
                } else if (m_sampleSize == 2) {
                    Sint16 sample16 = *reinterpret_cast<Sint16*>(&lowPriorityData[byteIndex]);
                    sample = static_cast<float>(sample16) / 32768.0f;
                }

                output[sampleIndex] += sample * attenuation;
            }
        }
    }

    // Check for underrun and update consecutive counter
    if (highPriorityBytes == 0 && lowPriorityBytes == 0) {
        m_underrunCount.fetch_add(1);
        m_consecutiveUnderruns.fetch_add(1);
    } else {
        // Reset consecutive underruns if we successfully provided data
        m_consecutiveUnderruns.store(0);
    }
}

bool UnifiedAudioBackend::writeToRingBuffer(const unsigned char* data, int length, AudioPriority priority)
{
    RingBuffer& buffer = (priority == HIGH_PRIORITY) ? m_highPriorityBuffer : m_lowPriorityBuffer;

    int writePos = buffer.writePos.load();
    int readPos = buffer.readPos.load();
    int currentSize = buffer.size.load();

    // Check if we have enough space
    if (currentSize + length > RING_BUFFER_SIZE) {
        m_overrunCount.fetch_add(1);
        m_consecutiveOverruns.fetch_add(1);
        return false;  // Buffer full
    } else {
        // Reset consecutive overruns if we successfully wrote data
        m_consecutiveOverruns.store(0);
    }

    // Write data (handle wrap-around)
    int remaining = length;
    int offset = 0;

    while (remaining > 0) {
        int chunkSize = std::min(remaining, RING_BUFFER_SIZE - writePos);
        memcpy(buffer.data.get() + writePos, data + offset, chunkSize);

        writePos = (writePos + chunkSize) % RING_BUFFER_SIZE;
        offset += chunkSize;
        remaining -= chunkSize;
    }

    // Update positions atomically
    buffer.writePos.store(writePos);
    buffer.size.fetch_add(length);

    return true;
}

int UnifiedAudioBackend::readFromRingBuffer(unsigned char* data, int maxLength, AudioPriority priority)
{
    RingBuffer& buffer = (priority == HIGH_PRIORITY) ? m_highPriorityBuffer : m_lowPriorityBuffer;

    int currentSize = buffer.size.load();
    int readPos = buffer.readPos.load();

    int toRead = std::min(maxLength, currentSize);
    if (toRead <= 0) {
        return 0;
    }

    // Read data (handle wrap-around)
    int remaining = toRead;
    int offset = 0;

    while (remaining > 0) {
        int chunkSize = std::min(remaining, RING_BUFFER_SIZE - readPos);
        memcpy(data + offset, buffer.data.get() + readPos, chunkSize);

        readPos = (readPos + chunkSize) % RING_BUFFER_SIZE;
        offset += chunkSize;
        remaining -= chunkSize;
    }

    // Update positions atomically
    buffer.readPos.store(readPos);
    buffer.size.fetch_sub(toRead);

    return toRead;
}

void UnifiedAudioBackend::resample(const float* input, int inputFrames, float* output, int outputFrames, int channels)
{
    // Use linear interpolation for high-quality resampling
    resampleLinear(input, inputFrames, output, outputFrames, channels);
}

void UnifiedAudioBackend::resampleLinear(const float* input, int inputFrames, float* output, int outputFrames, int channels)
{
    if (!input || !output || inputFrames <= 0 || outputFrames <= 0 || channels <= 0) {
        return;
    }

    float ratio = static_cast<float>(inputFrames) / static_cast<float>(outputFrames);

    for (int outFrame = 0; outFrame < outputFrames; outFrame++) {
        float srcPos = static_cast<float>(outFrame) * ratio;
        int srcFrame = static_cast<int>(srcPos);
        float frac = srcPos - static_cast<float>(srcFrame);

        // Ensure we don't read past the end
        if (srcFrame >= inputFrames - 1) {
            srcFrame = inputFrames - 1;
            frac = 0.0f;
        }

        for (int ch = 0; ch < channels; ch++) {
            int inIdx1 = srcFrame * channels + ch;
            int inIdx2 = (srcFrame + 1) * channels + ch;
            int outIdx = outFrame * channels + ch;

            if (srcFrame < inputFrames - 1) {
                // Linear interpolation between two samples
                float sample1 = input[inIdx1];
                float sample2 = input[inIdx2];
                output[outIdx] = sample1 + frac * (sample2 - sample1);
            } else {
                // Just use the last sample
                output[outIdx] = input[inIdx1];
            }
        }
    }
}

void UnifiedAudioBackend::setPlatformOptimizations()
{
#ifdef _WIN32
    // Windows: Use larger buffers for stability
    m_targetLatencyMs = 40;
    m_minLatencyMs = 20;
    m_maxLatencyMs = 100;
#elif defined(__APPLE__)
    // macOS: Can use smaller buffers for lower latency
    m_targetLatencyMs = 20;
    m_minLatencyMs = 10;
    m_maxLatencyMs = 80;
#else
    // Linux: Medium latency for compatibility
    m_targetLatencyMs = 25;
    m_minLatencyMs = 15;
    m_maxLatencyMs = 90;
#endif

    qDebug() << "Platform optimizations set - target latency:" << m_targetLatencyMs << "ms";
}

int UnifiedAudioBackend::getOptimalBufferSize() const
{
    // Calculate buffer size for target latency
    int samplesForLatency = (m_targetSampleRate * m_targetLatencyMs) / 1000;

    // Round to nearest power of 2 for SDL2 efficiency
    int bufferSize = 64;  // Minimum
    while (bufferSize < samplesForLatency && bufferSize < 2048) {
        bufferSize *= 2;
    }

    qDebug() << "Calculated optimal buffer size:" << bufferSize << "samples for" << m_targetLatencyMs << "ms latency";
    return bufferSize;
}

void UnifiedAudioBackend::checkPerformanceAndAdjust()
{
    if (!m_initialized) return;

    long long currentTime = QDateTime::currentMSecsSinceEpoch();
    long long lastCheck = m_lastPerformanceCheck.load();

    // Check performance every 5 seconds
    if (currentTime - lastCheck < 5000) {
        return;
    }

    // Update last check time
    m_lastPerformanceCheck.store(currentTime);

    // Check if we need to adjust latency based on performance
    if (shouldIncreaseLatency()) {
        m_needsLatencyAdjustment.store(true);
        qDebug() << "Performance check: recommending latency increase due to underruns";
    } else if (shouldDecreaseLatency()) {
        m_needsLatencyAdjustment.store(true);
        qDebug() << "Performance check: recommending latency decrease - system performing well";
    }

    // Reset consecutive counters after check
    m_consecutiveUnderruns.store(0);
    m_consecutiveOverruns.store(0);
}

void UnifiedAudioBackend::adjustLatencyForPerformance()
{
    if (!m_needsLatencyAdjustment.load()) {
        return;
    }

    int currentLatency = m_targetLatencyMs;
    int newLatency = currentLatency;

    if (shouldIncreaseLatency()) {
        // Increase latency to prevent underruns
        newLatency = std::min(currentLatency + 10, m_maxLatencyMs);
        qDebug() << "Increasing audio latency from" << currentLatency << "ms to" << newLatency << "ms";
    } else if (shouldDecreaseLatency()) {
        // Decrease latency for better responsiveness
        newLatency = std::max(currentLatency - 5, m_minLatencyMs);
        qDebug() << "Decreasing audio latency from" << currentLatency << "ms to" << newLatency << "ms";
    }

    if (newLatency != currentLatency) {
        m_targetLatencyMs = newLatency;

        // Reinitialize audio with new latency
        int sampleRate = m_targetSampleRate;
        int channels = m_channels;
        int sampleSize = m_sampleSize;

        shutdown();
        if (initialize(sampleRate, channels, sampleSize)) {
            qDebug() << "Successfully adjusted audio latency to" << newLatency << "ms";
        } else {
            qWarning() << "Failed to reinitialize audio with new latency, reverting";
            m_targetLatencyMs = currentLatency;
            initialize(sampleRate, channels, sampleSize);
        }
    }

    m_needsLatencyAdjustment.store(false);
}

bool UnifiedAudioBackend::shouldIncreaseLatency() const
{
    // Increase latency if we've had multiple underruns recently
    int underruns = m_consecutiveUnderruns.load();
    int totalUnderruns = m_underrunCount.load();

    // If we've had 3+ consecutive underruns, or total underruns > 50, increase latency
    return (underruns >= 3) || (totalUnderruns > 50);
}

bool UnifiedAudioBackend::shouldDecreaseLatency() const
{
    // Only decrease latency if system has been stable
    int underruns = m_underrunCount.load();
    int overruns = m_overrunCount.load();
    long long totalFrames = m_totalFramesProcessed.load();

    // Don't decrease if we're already at minimum
    if (m_targetLatencyMs <= m_minLatencyMs) {
        return false;
    }

    // Only decrease if we've processed a lot of frames with very few problems
    if (totalFrames > 10000) {  // Processed at least ~3 minutes of audio
        double underrunRate = static_cast<double>(underruns) / static_cast<double>(totalFrames) * 1000.0;
        double overrunRate = static_cast<double>(overruns) / static_cast<double>(totalFrames) * 1000.0;

        // If underrun/overrun rate is very low (< 0.1%), we can try lower latency
        return (underrunRate < 0.1) && (overrunRate < 0.1);
    }

    return false;
}