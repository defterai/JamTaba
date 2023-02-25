#include "RoomStreamerNode.h"
#include "Mp3Decoder.h"
#include "core/AudioDriver.h"
#include "core/SamplesBuffer.h"
#include "log/Logging.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <QNetworkReply>
#include <QDebug>
#include <QBuffer>
#include <QObject>
#include <QDateTime>
#include <QWaitCondition>
#include <cmath>
#include <QMutexLocker>
#include <QFile>

namespace audio {
class Mp3Decoder;
}

using audio::AbstractMp3Streamer;
using audio::NinjamRoomStreamerNode;
using audio::Mp3Decoder;
using audio::Mp3DecoderMiniMp3;
using audio::SamplesBuffer;

const int AbstractMp3Streamer::MAX_BYTES_PER_DECODING = 2048;


AbstractMp3Streamer::AbstractMp3Streamer(Mp3Decoder *decoder, int maxBufferSize) :
    AudioNode(44100),
    decoder(decoder),
    bufferedSamples(2, 4096),
    maxBufferSize(maxBufferSize),
    streaming(false),
    buffering(false)
{
    bufferedSamples.setFrameLenght(0); // reset internal offset

    connect(this, &AbstractMp3Streamer::postStartStream, this, &AbstractMp3Streamer::startStream);
    connect(this, &AbstractMp3Streamer::postStopStream, this, &AbstractMp3Streamer::stopStream);
}

AbstractMp3Streamer::~AbstractMp3Streamer()
{
    delete decoder;
}

void AbstractMp3Streamer::startStream(const QString& streamPath)
{
    bool wasStreaming = isStreaming();
    free();
    initialize(streamPath);
    bool nowStreaming = isStreaming();
    if (nowStreaming != wasStreaming) {
        emit stateChanged(nowStreaming);
    }
    emit bufferingChanged(true, 0);
}

void AbstractMp3Streamer::stopStream()
{
    bool wasStreaming = isStreaming();
    free();
    bool nowStreaming = isStreaming();
    if (nowStreaming != wasStreaming) {
        emit stateChanged(nowStreaming);
    }
    emit bufferingChanged(false, 0);
}

int AbstractMp3Streamer::getSamplesToRender(int outLenght)
{
    if (needResampling()) {
        return getInputResamplingLength(getDecoderSampleRate(), getSampleRate(), outLenght);
    }
    return outLenght;
}

void AbstractMp3Streamer::processReplacing(const SamplesBuffer &in, SamplesBuffer &out, std::vector<midi::MidiMessage> &)
{
    Q_UNUSED(in);

    if (bufferedSamples.isEmpty() || !streaming)
        return;

    int samplesToRender = getSamplesToRender(out.getFrameLenght());
    if (samplesToRender <= 0)
        return;

    internalInputBuffer.setFrameLenght(samplesToRender);
    internalInputBuffer.set(bufferedSamples);

    if (needResampling()) {
        const auto &resampledBuffer = resampler.resample(internalInputBuffer, out.getFrameLenght());
        internalOutputBuffer.setFrameLenght(resampledBuffer.getFrameLenght());
        internalOutputBuffer.set(resampledBuffer);
    } else {
        internalOutputBuffer.setFrameLenght(out.getFrameLenght());
        internalOutputBuffer.set(internalInputBuffer);
    }

    bufferedSamples.discardFirstSamples(samplesToRender);// keep non rendered samples for next audio callback

    if (internalOutputBuffer.getFrameLenght() < out.getFrameLenght())
        qCDebug(jtNinjamRoomStreamer) << out.getFrameLenght()
            - internalOutputBuffer.getFrameLenght() << " samples missing";

    this->lastPeak.update(internalOutputBuffer.computePeak());

    out.add(internalOutputBuffer);

    emit audioPeakChanged(lastPeak);
}

void AbstractMp3Streamer::free()
{
    qCDebug(jtNinjamRoomStreamer) << "stopping room stream";

    decoder->reset(); // discard unprocessed bytes
    bufferedSamples.zero();// discard samples
    bytesToDecode.clear();
    streaming = false;
    buffering = false;
    resetLastPeak();
}

void AbstractMp3Streamer::initialize(const QString &streamPath)
{
    streaming = !streamPath.isNull() && !streamPath.isEmpty();
    bufferedSamples.zero();
    bytesToDecode.clear();
    buffering = true;
}

int AbstractMp3Streamer::getDecoderSampleRate() const
{
    return decoder->getSampleRate();
}

bool AbstractMp3Streamer::needResampling() const
{
    return getDecoderSampleRate() != getSampleRate();
}

int AbstractMp3Streamer::getBufferingPercentage() const
{
    return bytesToDecode.size() / (float)maxBufferSize * 100;
}

bool AbstractMp3Streamer::hasBufferedSamples() const
{
    return !bufferedSamples.isEmpty();
}

int AbstractMp3Streamer::getBufferedSamples() const
{
    return bufferedSamples.getFrameLenght();
}

void AbstractMp3Streamer::addDecodeData(const QByteArray& data)
{
    bytesToDecode.append(data);
    if (bytesToDecode.size() >= maxBufferSize) {
        buffering = false;
        emit bufferingChanged(false, 100);
    } else {
        emit bufferingChanged(buffering, getBufferingPercentage());
    }
    if (buffering) {
        qCDebug(jtNinjamRoomStreamer) << "bytes downloaded  bytesToDecode:" << bytesToDecode.size()
                                      << " bufferedSamples: " << bufferedSamples.getFrameLenght();
    }
}

bool AbstractMp3Streamer::decode(const unsigned int maxBytesToDecode)
{
    int totalBytesToProcess = std::min((int)maxBytesToDecode, bytesToDecode.size());
    char *in = bytesToDecode.data();

    if (totalBytesToProcess > 0) {
        int bytesProcessed = 0;
        while (bytesProcessed < totalBytesToProcess) {// split bytesReaded in chunks to avoid a very large decoded buffer
            // chunks maxsize is 2048 bytes
            int bytesToProcess = std::min(totalBytesToProcess - bytesProcessed, MAX_BYTES_PER_DECODING);
            const auto &decodedBuffer = decoder->decode(in, bytesToProcess);

            // prepare in for the next decoding
            in += bytesToProcess;
            bytesProcessed += bytesToProcess;
            // +++++++++++++++++  PROCESS DECODED SAMPLES ++++++++++++++++
            bufferedSamples.append(decodedBuffer);
        }

        bytesToDecode = bytesToDecode.right(bytesToDecode.size() - bytesProcessed);
    }

    if (bytesToDecode.isEmpty()) { // no more bytes to decode
        qCritical() << "no more bytes to decode and not enough buffered samples. Buffering ...";
        buffering = true;
        emit bufferingChanged(true, 0);
        return false;
    }
    return true;
}

// +++++++++++++++++++++++++++++++++++++++

const int NinjamRoomStreamerNode::BUFFER_SIZE = 128000;

NinjamRoomStreamerNode::NinjamRoomStreamerNode() :
    AbstractMp3Streamer(new Mp3DecoderMiniMp3(), BUFFER_SIZE),
    device(nullptr),
    httpClient(nullptr)
{

}

void NinjamRoomStreamerNode::free()
{
    if (device) {
        device->deleteLater();
        device = nullptr;
    }
    AbstractMp3Streamer::free();
}

void NinjamRoomStreamerNode::initialize(const QString &streamPath)
{
    AbstractMp3Streamer::initialize(streamPath);
    if (!streamPath.isEmpty()) {
        qCDebug(jtNinjamRoomStreamer) << "connecting in " << streamPath;

        QNetworkReply *reply = httpClient.get(QNetworkRequest(QUrl(streamPath)));
        QObject::connect(reply, &QNetworkReply::readyRead, this, &NinjamRoomStreamerNode::on_reply_read);
        QObject::connect(reply, &QNetworkReply::errorOccurred, this, &NinjamRoomStreamerNode::on_reply_error);
        this->device = reply;
    }
}

void NinjamRoomStreamerNode::on_reply_error(QNetworkReply::NetworkError /*error*/)
{
    QString msg = "ERROR playing room stream";
    qCritical() << msg;
    emit error(msg);
}

void NinjamRoomStreamerNode::on_reply_read()
{
    if (!device) {
        qCDebug(jtNinjamRoomStreamer) << "device is null!";
        return;
    }
    if (device->isOpen() && device->isReadable()) {
        addDecodeData(device->readAll());
    } else {
        qCritical() << "problem in device!";
    }
}

NinjamRoomStreamerNode::~NinjamRoomStreamerNode()
{
    free();
}

void NinjamRoomStreamerNode::processReplacing(const SamplesBuffer &in, SamplesBuffer &out,
                                              std::vector<midi::MidiMessage> &midiBuffer)
{
    Q_UNUSED(in)
    if (isBuffering()) {
        return;
    }
    int samplesToRender = getSamplesToRender(out.getFrameLenght());
    while (getBufferedSamples() < samplesToRender) {
        if (!decode(256)) {
            break;
        }
    }
    if (hasBufferedSamples()) {
        AbstractMp3Streamer::processReplacing(in, out, midiBuffer);
    }
}
