#ifndef VORBIS_DECODER_H
#define VORBIS_DECODER_H

#include <vorbis/vorbisfile.h>
#include "audio/core/SamplesBuffer.h"
#include <QMutex>
#include <QList>
#include <QByteArray>
#include <QSharedPointer>

namespace vorbis {

class Decoder
{

public:

    Decoder();
    ~Decoder();
    const audio::SamplesBuffer &decode(int maxSamplesToDecode);

    bool isStereo() const;

    bool isMono() const;

    int getChannels() const;

    int getSampleRate() const;

    bool isInitialized() const;

    void setInputData(const QSharedPointer<QByteArray>& vorbisData);

    void addInputData(const QSharedPointer<QByteArray>& vorbisData);

    bool initialize();

    bool isFinished() const { return finished; }

    bool isValid() const { return valid; }

private:

    audio::SamplesBuffer internalBuffer;
    OggVorbis_File vorbisFile;
    bool initialized;

    mutable QMutex mutex;
    QList<QSharedPointer<QByteArray>> vorbisInputs;
    int vorbisInputOffset;

    static size_t readOgg(void *oggOutBuffer, size_t size, size_t nmemb, void *decoderInstance);

    size_t consumeTo(void *oggOutBuffer, size_t bytesToConsume);
    size_t getInputSize() const;

    bool finished = false; // all input was decoded
    bool valid = true;// will be flagged as invalid when an error is detected
};

inline bool Decoder::isStereo() const
{
    return getChannels() == 2;
}

inline bool Decoder::isInitialized() const
{
    return initialized;
}

} // namespace

#endif
