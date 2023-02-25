#ifndef AUDIOENCODERCONTROLLER_H
#define AUDIOENCODERCONTROLLER_H

#include "audio/Encoder.h"
#include "AudioController.h"
#include <QByteArray>
#include <QSharedPointer>
#include <QScopedPointer>
#include <QMap>

namespace controller
{

class EncodingThread;

class AudioEncoderController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AudioEncoderController)

public:
    AudioEncoderController();
    virtual ~AudioEncoderController();
    void start();
    void stop();
    void setAudioEncodeQuality(float quality);
    void setVoiceEncodeQuality(float quality);
    void scheduleEncode(const AudioChannelData& channelData);
    void scheduleEncode(const QList<AudioChannelData>& channelData);

signals:
    void encodeCompleted(const controller::AudioChannelData& channelData, const QByteArray& encodedData);

private:
    QMutex mutex;
    QScopedPointer<EncodingThread, QScopedPointerDeleteLater> encodingThread;
    QMutex encodersMutex;
    QMap<int, QSharedPointer<AudioEncoder>> encoders;
    float audioEncodingQuality;
    float voiceEncodingQuality;

    QSharedPointer<AudioEncoder> getEncoder(const AudioChannelData& channelData);
    void encode(const AudioChannelData& channelData);

    friend class encodingThread;
};

}

#endif // AUDIOENCODERCONTROLLER_H
