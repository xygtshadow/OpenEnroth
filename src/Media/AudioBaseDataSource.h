#pragma once

#include <queue>
#include <deque>
#include <memory>

#include "AudioDataSource.h"

struct AVFormatContext;
struct AVCodecContext;
struct SwrContext;

class AudioBaseDataSource : public IAudioDataSource {
 public:
    AudioBaseDataSource();
    virtual ~AudioBaseDataSource() { Close(); }

    virtual bool Open() override;
    virtual void Close() override;

    virtual size_t GetSampleRate() override;
    virtual size_t GetChannelCount() override;
    virtual Blob GetNextBuffer() override;

    virtual float GetDuration() override;

    // Playback starts (and, once the track loops, restarts) this many seconds into the stream.
    // Must be set before Open().
    void SetStartSeconds(float startSeconds) { _startSeconds = startSeconds; }

 protected:
    AVFormatContext *pFormatContext;
    int iStreamIndex;
    AVCodecContext *pCodecContext;
    SwrContext *pConverter;
    bool bOpened;
    std::queue<Blob> queue;

    float _savedDuration;
    float _startSeconds = 0.0f;
};
