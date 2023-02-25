#include "AudioEncoderController.h"
#include "audio/vorbis/Vorbis.h"
#include "audio/vorbis/VorbisEncoder.h"
#include "log/Logging.h"
#include <functional>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

namespace controller
{

class EncodingThread final : public QThread
{
public:
    using EncodeFuncImpl = std::function<void(const AudioChannelData& channelData)>;

    explicit EncodingThread(EncodeFuncImpl&& encodeFuncImpl) :
        encodeFuncImpl(encodeFuncImpl),
        mutex(QMutex::NonRecursive),
        stopRequested(false)
    {
        qCDebug(jtNinjamCore) << "Starting Encoding Thread";
        start();
    }

    ~EncodingThread()
    {
        stop();
    }

    void addChannelDataToEncode(const AudioChannelData& channelData)
    {
        if (channelData.samples.isNull() || channelData.samples->isEmpty())
            return;
        // qCDebug(jtNinjamCore) << "Adding samples to encode";
        QMutexLocker locker(&mutex);
        chunksToEncode.push_back(channelData);
        // this method is called by Qt main thread (the producer thread).
        hasAvailableChunksToEncode.wakeAll();// wakeup the encoding thread (consumer thread)
    }

    void addChannelDataToEncode(const QList<AudioChannelData>& channelData)
    {
        if (channelData.isEmpty())
            return;
        // qCDebug(jtNinjamCore) << "Adding samples to encode";
        QMutexLocker locker(&mutex);
        chunksToEncode.insert(chunksToEncode.end(), channelData.begin(), channelData.end());
        // this method is called by Qt main thread (the producer thread).
        hasAvailableChunksToEncode.wakeAll();// wakeup the encoding thread (consumer thread)
    }

    void stop()
    {
        if (!stopRequested) {
            QMutexLocker locker(&mutex);
            if (!stopRequested) {
                stopRequested = true;
                hasAvailableChunksToEncode.wakeAll();
                qCDebug(jtNinjamCore) << "Stopping Encoding Thread";
            }
        }
    }

protected:

    void run() override
    {
        while (!stopRequested) {
            {
                QMutexLocker locker(&mutex);
                while (!stopRequested && chunksToEncode.empty()) {
                    hasAvailableChunksToEncode.wait(&mutex);
                }
                if (stopRequested){
                    break;
                }
                std::swap(chunksProcessing, chunksToEncode);
            }
            for (const auto& chunk : qAsConst(chunksProcessing)) {
                encodeFuncImpl(chunk);
                if (stopRequested) {
                    break;
                }
            }
            chunksProcessing.clear();
        }
        qCDebug(jtNinjamCore) << "Encoding thread stopped!";
    }

private:
    EncodeFuncImpl encodeFuncImpl;
    QMutex mutex;
    QWaitCondition hasAvailableChunksToEncode;
    std::vector<AudioChannelData> chunksToEncode;
    std::vector<AudioChannelData> chunksProcessing;
    volatile bool stopRequested;
};


AudioEncoderController::AudioEncoderController() :
    encodingThread(nullptr),
    encodersMutex(QMutex::NonRecursive),
    audioEncodingQuality(vorbis::EncoderQualityNormal),
    voiceEncodingQuality(vorbis::EncoderQualityLow)
{

}

AudioEncoderController::~AudioEncoderController()
{
    stop();
}

void AudioEncoderController::start()
{
    if (encodingThread) return;
    QMutexLocker locker(&mutex);
    if (!encodingThread) {
        encodingThread.reset(new EncodingThread(std::bind(&AudioEncoderController::encode, this, std::placeholders::_1)));
        encodingThread->start();
    }
}

void AudioEncoderController::stop()
{
    {
        QMutexLocker locker(&mutex);
        if (encodingThread) {
            encodingThread->stop();
            encodingThread->wait(); // wait the encoding thread to finish
            encodingThread.reset();
        }
    }

    {
        QMutexLocker locker(&encodersMutex);
        encoders.clear();
    }
}

void AudioEncoderController::setAudioEncodeQuality(float quality)
{
    audioEncodingQuality = quality;
}

void AudioEncoderController::setVoiceEncodeQuality(float quality)
{
    voiceEncodingQuality = quality;
}

void AudioEncoderController::scheduleEncode(const AudioChannelData& channelData)
{
    if (!encodingThread) return;
    QMutexLocker locker(&mutex);
    if (encodingThread) {
        encodingThread->addChannelDataToEncode(channelData);
    }
}

void AudioEncoderController::scheduleEncode(const QList<AudioChannelData>& channelData)
{
    if (!encodingThread) return;
    QMutexLocker locker(&mutex);
    if (encodingThread) {
        encodingThread->addChannelDataToEncode(channelData);
    }
}

QSharedPointer<AudioEncoder> AudioEncoderController::getEncoder(const AudioChannelData& channelData)
{
    int channels = channelData.samples->getChannels();
    if (channels <= 0) {
        return 0;
    }
    int channelIndex = channelData.channelID;
    int sampleRate = channelData.sampleRate;
    float quality = channelData.isVoiceChat ? voiceEncodingQuality : audioEncodingQuality;
    QMutexLocker locker(&encodersMutex);
    auto iterator = encoders.find(channelIndex);
    if (iterator == encoders.end() || (iterator.value()->getChannels() != channels ||
                                       iterator.value()->getSampleRate() != sampleRate ||
                                       !qFuzzyCompare(iterator.value()->getQuality(), quality))) {
        return encoders[channelIndex] = createQSharedPointer<vorbis::Encoder>(channels, sampleRate, quality);
    }
    return iterator.value();
}

void AudioEncoderController::encode(const AudioChannelData& channelData)
{
    auto encoder = getEncoder(channelData);
    if (encoder) {
        QByteArray encodedBytes(encoder->encode(*channelData.samples));
        if (channelData.isLastPart) {
            encodedBytes.append(encoder->finishIntervalEncoding());
        }
        if (!encodedBytes.isEmpty()) {
            emit encodeCompleted(channelData, encodedBytes);
        }
    }
}

}
