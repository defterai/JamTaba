#ifndef AUDIOSETTINGS_H
#define AUDIOSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class AudioSettings final : public SettingsObject
{
public:
    static const int DEFAULT_SAMPLE_RATE = 44100;
    static const int MIN_SAMPLE_RATE = 44100;
    static const int MAX_SAMPLE_RATE = 192000;
    static const int DEFAULT_BUFFER_SIZE = 128;
    static const int MIN_BUFFER_SIZE = 16;
    static const int MAX_BUFFER_SIZE = 4096;

    AudioSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline int getSampleRate() const
    {
        return sampleRate;
    }
    void setSampleRate(int value);
    inline int getBufferSize() const
    {
        return bufferSize;
    }
    void setBufferSize(int value);
    inline int getFirstInputIndex() const
    {
        return firstIn;
    }
    inline void setFirstInputIndex(int value)
    {
        if (value >= -1) firstIn = value;
    }
    inline int getFirstOutputIndex() const
    {
        return firstOut;
    }
    inline void setFirstOutputIndex(int value)
    {
        if (value >= -1) firstOut = value;
    }
    inline int getLastInputIndex() const
    {
        return lastIn;
    }
    inline void setLastInputIndex(int value)
    {
        if (value >= -1) lastIn = value;
    }
    inline int getLastOutputIndex() const
    {
        return lastOut;
    }
    inline void setLastOutputIndex(int value)
    {
        if (value >= -1) lastOut = value;
    }
    inline const QString& getInputDevice() const
    {
        return audioInputDevice;
    }
    inline void setInputDevice(const QString& value)
    {
        audioInputDevice = value;
    }
    inline const QString& getOutputDevice() const
    {
        return audioOutputDevice;
    }
    inline void setOutputDevice(const QString& value)
    {
        audioOutputDevice = value;
    }
    inline float getEncodingQuality() const
    {
        return encodingQuality;
    }
    void setEncodingQuality(float value);
private:
    int sampleRate;
    int bufferSize;
    int firstIn;
    int firstOut;
    int lastIn;
    int lastOut;
    QString audioInputDevice;
    QString audioOutputDevice;
    float encodingQuality;
};

}

#endif // AUDIOSETTINGS_H
