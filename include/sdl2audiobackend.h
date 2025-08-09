/*
 * Fujisan - Modern Atari Emulator
 * SDL2 Audio Backend for low-latency audio
 */

#ifndef SDL2AUDIOBACKEND_H
#define SDL2AUDIOBACKEND_H

#ifdef HAVE_SDL2_AUDIO

#include <QObject>
#include <SDL.h>
#include <functional>

class SDL2AudioBackend : public QObject
{
    Q_OBJECT

public:
    explicit SDL2AudioBackend(QObject *parent = nullptr);
    ~SDL2AudioBackend();

    // Initialize SDL2 audio with specified parameters
    bool initialize(int sampleRate, int channels, int sampleSize);
    
    // Shutdown audio
    void shutdown();
    
    // Control playback
    void pause();
    void resume();
    
    // Set the callback that will provide audio data
    // The callback should fill the buffer with the requested number of bytes
    void setAudioCallback(std::function<void(unsigned char*, int)> callback);
    
    // Check if audio is initialized
    bool isInitialized() const { return m_initialized; }
    
    // Get/set volume (0.0 to 1.0)
    float getVolume() const { return m_volume; }
    void setVolume(float volume);
    
    // Get actual buffer size being used
    int getBufferSize() const { return m_bufferSize; }
    
    // Get latency in milliseconds
    float getLatencyMs() const;

private:
    // SDL audio callback (static, calls instance method)
    static void sdlAudioCallback(void* userdata, Uint8* stream, int len);
    
    // Instance audio callback
    void audioCallback(Uint8* stream, int len);
    
    bool m_initialized;
    SDL_AudioDeviceID m_audioDevice;
    SDL_AudioSpec m_audioSpec;
    std::function<void(unsigned char*, int)> m_userCallback;
    float m_volume;
    int m_bufferSize;
    int m_sampleRate;
    int m_channels;
    int m_sampleSize;
    
    // Buffer for volume adjustment
    unsigned char* m_tempBuffer;
};

#endif // HAVE_SDL2_AUDIO

#endif // SDL2AUDIOBACKEND_H