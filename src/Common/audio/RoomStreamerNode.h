#ifndef ROOM_STREAMER_NODE_H
#define ROOM_STREAMER_NODE_H

#include "core/AudioNode.h"
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include "SamplesBufferResampler.h"

class QIODevice;

namespace audio {

class Mp3Decoder;

class AbstractMp3Streamer : public AudioNode
{
    Q_OBJECT

public:
    AbstractMp3Streamer(audio::Mp3Decoder *decoder, int maxBufferSize);
    virtual ~AbstractMp3Streamer();
    void processReplacing(const audio::SamplesBuffer &in, audio::SamplesBuffer &out,
                          std::vector<midi::MidiMessage> &midiBuffer) override;
    bool isStreaming() const;
    bool isBuffering() const;
    int getDecoderSampleRate() const;
signals:
    void error(const QString &errorMsg);
    void stateChanged(bool streaming);
    void bufferingChanged(bool buffering, int percent);

    void postStartStream(const QString& streamPath);
    void postStopStream();

private slots:
    void startStream(const QString& streamPath);
    void stopStream();

protected:
    virtual void free();
    virtual void initialize(const QString &streamPath);
    bool hasBufferedSamples() const;
    int getBufferedSamples() const;
    void addDecodeData(const QByteArray& data);
    bool decode(const unsigned int maxBytesToDecode);
    int getSamplesToRender(int outLenght);

private:
    static const int MAX_BYTES_PER_DECODING;

    audio::Mp3Decoder *decoder;
    QByteArray bytesToDecode;
    SamplesBuffer bufferedSamples;
    SamplesBufferResampler resampler;
    int maxBufferSize;
    bool streaming;
    bool buffering;

    bool needResampling() const;
    int getBufferingPercentage() const;
};

inline bool AbstractMp3Streamer::isStreaming() const
{
    return streaming;
}

inline bool AbstractMp3Streamer::isBuffering() const
{
    return buffering;
}

// +++++++++++++++++++++++++++++++++++++++++++++

class NinjamRoomStreamerNode : public AbstractMp3Streamer
{
    Q_OBJECT

public:
    NinjamRoomStreamerNode();
    ~NinjamRoomStreamerNode();
    void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) override;

protected:
    void free() override;
    void initialize(const QString &streamPath) override;

private:
    QIODevice *device;
    QNetworkAccessManager httpClient;

    static const int BUFFER_SIZE;

private slots:
    void on_reply_error(QNetworkReply::NetworkError);
    void on_reply_read();
};

} // namespace end

#endif
