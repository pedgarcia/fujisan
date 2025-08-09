/*
 * Fujisan - Modern Atari Emulator
 * SDL2 Audio Backend Implementation
 */

#include "../include/sdl2audiobackend.h"

#ifdef HAVE_SDL2_AUDIO

#include <QDebug>
#include <cstring>
#include <algorithm>

SDL2AudioBackend::SDL2AudioBackend(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_audioDevice(0)
    , m_userCallback(nullptr)
    , m_volume(1.0f)
    , m_bufferSize(0)
    , m_sampleRate(0)
    , m_channels(0)
    , m_sampleSize(0)
    , m_tempBuffer(nullptr)
{
    // Initialize SDL audio subsystem if not already done
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            qWarning() << "Failed to initialize SDL audio:" << SDL_GetError();
        }
    }
}

SDL2AudioBackend::~SDL2AudioBackend()
{
    shutdown();
    
    if (m_tempBuffer) {
        delete[] m_tempBuffer;
        m_tempBuffer = nullptr;
    }
    
    // Don't quit SDL audio here as other parts might be using it
}

bool SDL2AudioBackend::initialize(int sampleRate, int channels, int sampleSize)
{
    if (m_initialized) {
        shutdown();
    }
    
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    SDL_zero(obtained);
    
    desired.freq = sampleRate;
    desired.channels = channels;
    
    // Set format based on sample size
    if (sampleSize == 1) {
        desired.format = AUDIO_U8;
    } else if (sampleSize == 2) {
        desired.format = AUDIO_S16SYS;  // System byte order
    } else {
        qWarning() << "Unsupported sample size:" << sampleSize;
        return false;
    }
    
    // Use power-of-2 buffer sizes for better SDL2 performance
    // Smaller buffers = lower latency but more CPU usage
    if (sampleRate <= 22050) {
        desired.samples = 256;  // At 22050Hz: ~11.6ms
    } else if (sampleRate <= 44100) {
        desired.samples = 512;  // At 44100Hz: ~11.6ms
    } else {
        desired.samples = 512;  // At 48000Hz: ~10.7ms
    }
    qDebug() << "SDL2 using" << desired.samples << "samples for" << sampleRate << "Hz";
    
    // Set callback
    desired.callback = sdlAudioCallback;
    desired.userdata = this;
    
    // Open audio device (0 means use default device)
    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 
                                       SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    
    if (m_audioDevice == 0) {
        qWarning() << "Failed to open SDL audio device:" << SDL_GetError();
        return false;
    }
    
    // Store actual parameters
    m_audioSpec = obtained;
    m_sampleRate = obtained.freq;
    m_channels = obtained.channels;
    m_sampleSize = sampleSize;
    m_bufferSize = obtained.samples * channels * sampleSize;
    
    // Allocate temp buffer for volume adjustment
    if (m_tempBuffer) {
        delete[] m_tempBuffer;
    }
    m_tempBuffer = new unsigned char[m_bufferSize];
    
    qDebug() << "SDL2 Audio initialized:";
    qDebug() << "  Sample rate:" << m_sampleRate << "Hz";
    qDebug() << "  Channels:" << m_channels;
    qDebug() << "  Sample size:" << m_sampleSize << "bytes";
    qDebug() << "  Buffer samples:" << obtained.samples;
    qDebug() << "  Buffer size:" << m_bufferSize << "bytes";
    float latency = (float)obtained.samples * 1000.0f / (float)m_sampleRate;
    qDebug() << "  Latency:" << latency << "ms";
    
    m_initialized = true;
    
    // Start audio playback
    SDL_PauseAudioDevice(m_audioDevice, 0);
    
    return true;
}

void SDL2AudioBackend::shutdown()
{
    if (m_initialized) {
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
        m_initialized = false;
    }
}

void SDL2AudioBackend::pause()
{
    if (m_initialized) {
        SDL_PauseAudioDevice(m_audioDevice, 1);
    }
}

void SDL2AudioBackend::resume()
{
    if (m_initialized) {
        SDL_PauseAudioDevice(m_audioDevice, 0);
    }
}

void SDL2AudioBackend::setAudioCallback(std::function<void(unsigned char*, int)> callback)
{
    SDL_LockAudioDevice(m_audioDevice);
    m_userCallback = callback;
    SDL_UnlockAudioDevice(m_audioDevice);
}

void SDL2AudioBackend::setVolume(float volume)
{
    m_volume = std::max(0.0f, std::min(1.0f, volume));
}

float SDL2AudioBackend::getLatencyMs() const
{
    if (!m_initialized || m_sampleRate == 0) {
        return 0.0f;
    }
    
    // Calculate latency based on actual buffer size
    float samplesPerChannel = m_audioSpec.samples;
    float latencySeconds = samplesPerChannel / static_cast<float>(m_sampleRate);
    return latencySeconds * 1000.0f;
}

void SDL2AudioBackend::sdlAudioCallback(void* userdata, Uint8* stream, int len)
{
    SDL2AudioBackend* backend = static_cast<SDL2AudioBackend*>(userdata);
    if (backend) {
        backend->audioCallback(stream, len);
    }
}

void SDL2AudioBackend::audioCallback(Uint8* stream, int len)
{
    // Fill with silence by default
    memset(stream, 0, len);
    
    // Call user callback if set
    if (m_userCallback) {
        // Get audio data into temp buffer
        m_userCallback(m_tempBuffer, len);
        
        // Apply volume and copy to output
        if (m_volume >= 0.99f) {
            // No volume adjustment needed
            memcpy(stream, m_tempBuffer, len);
        } else if (m_volume > 0.01f) {
            // Apply volume
            if (m_sampleSize == 1) {
                // 8-bit unsigned samples
                for (int i = 0; i < len; i++) {
                    int sample = m_tempBuffer[i] - 128;  // Convert to signed
                    sample = static_cast<int>(sample * m_volume);
                    stream[i] = static_cast<Uint8>(sample + 128);  // Convert back to unsigned
                }
            } else if (m_sampleSize == 2) {
                // 16-bit signed samples
                Sint16* src = reinterpret_cast<Sint16*>(m_tempBuffer);
                Sint16* dst = reinterpret_cast<Sint16*>(stream);
                int samples = len / 2;
                for (int i = 0; i < samples; i++) {
                    dst[i] = static_cast<Sint16>(src[i] * m_volume);
                }
            }
        }
        // else volume is 0, leave silence
    }
}

#endif // HAVE_SDL2_AUDIO