#ifndef LOCALINPUTTRACKSETTINGS_H
#define LOCALINPUTTRACKSETTINGS_H

#include "SettingsObject.h"
#include "audio/core/PluginDescriptor.h"
#include <QList>

namespace persistence {

static const qint8 MIN_MIDI_TRANSPOSE = -24;
static const qint8 MAX_MIDI_TRANSPOSE = 24;

class Plugin final
{
public:
    static const quint8 MAX_PROCESSORS_PER_TRACK = 4;

    class Builder final {
    public:
        explicit Builder(const audio::PluginDescriptor &descriptor);
        explicit Builder(const QJsonObject &in);
        inline Builder& setBypassed(bool value)
        {
            bypassed = value;
            return *this;
        }
        inline Builder& setData(const QByteArray& data)
        {
            this->data = data;
            return *this;
        }
        inline Plugin build() {
            return Plugin(*this);
        }
        friend class Plugin;
    private:
        QString path;
        QString name;
        QString manufacturer;
        bool bypassed = false;
        QByteArray data;
        audio::PluginDescriptor::Category category = audio::PluginDescriptor::Invalid_Plugin;
    };
    explicit Plugin(const Builder& builder);
    void write(QJsonObject &out) const;
    inline const QString& getPath() const
    {
        return path;
    }
    inline const QString& getName() const
    {
        return name;
    }
    inline const QString& getManufacturer() const
    {
        return manufacturer;
    }
    inline bool isBypassed() const
    {
        return bypassed;
    }
    inline const QByteArray& getData() const
    {
        return data;
    }
    inline const audio::PluginDescriptor::Category getCategory() const
    {
        return category;
    }
private:
    QString path;
    QString name;
    QString manufacturer;
    bool bypassed;
    QByteArray data; // saved data to restore in next jam session
    audio::PluginDescriptor::Category category; // VST, AU, NATIVE plugin
};

class SubChannel final
{
public:
    class Builder final {
    public:
        Builder();
        Builder(const QJsonObject &in, bool skipMidiRouting);
        inline Builder& setFirstInput(int firstInput)
        {
            this->firstInput = firstInput;
            return *this;
        }
        inline Builder& setChannelsCount(int channelsCount)
        {
            this->channelsCount = channelsCount;
            return *this;
        }
        inline Builder& setMidiDevice(int midiDevice)
        {
            this->midiDevice = midiDevice;
            return *this;
        }
        inline Builder& setMidiChannel(int midiChannel)
        {
            this->midiChannel = midiChannel;
            return *this;
        }
        Builder& setBoost(int boost);
        Builder& setGain(float gain);
        Builder& setPan(float pan);
        inline Builder& setMuted(bool muted)
        {
            this->muted = muted;
            return *this;
        }
        inline Builder& setStereoInverted(bool stereoInverted)
        {
            this->stereoInverted = stereoInverted;
            return *this;
        }
        Builder& setTranspose(qint8 transpose);
        Builder& setLowerMidiNote(quint8 lowerMidiNote);
        Builder& setHigherMidiNote(quint8 higherMidiNote);
        inline Builder& setRoutingMidiToFirstSubchannel(bool enabled)
        {
            this->routingMidiToFirstSubchannel = enabled;
            return *this;
        }
        inline SubChannel build()
        {
            return SubChannel(*this);
        }
        friend class SubChannel;
    private:
        QList<Plugin> plugins;
        int firstInput;
        int channelsCount;
        int midiDevice;
        int midiChannel;
        float gain;
        int boost; // [-1, 0, +1]
        float pan;
        bool muted;
        bool stereoInverted;
        qint8 transpose; // midi transpose
        quint8 lowerMidiNote; // midi rey range
        quint8 higherMidiNote;
        bool routingMidiToFirstSubchannel;
    };
    SubChannel(const Builder& builder);
    void write(QJsonObject &out, bool skipMidiRouting) const;

    inline const QList<persistence::Plugin>& getPlugins() const
    {
        return plugins;
    }
    void setPlugins(const QList<Plugin> &newPlugins);
    inline int getFirstInput() const
    {
        return firstInput;
    }
    inline int getChannelsCount() const
    {
        return channelsCount;
    }
    inline bool isNoInput() const
    {
        return channelsCount <= 0;
    }
    inline bool isMono() const
    {
        return channelsCount == 1;
    }
    inline bool isStereo() const
    {
        return channelsCount == 2;
    }
    inline int getMidiDevice() const
    {
        return midiDevice;
    }
    inline bool isMidi() const
    {
        return midiDevice >= 0;
    }
    inline int getMidiChannel() const
    {
        return midiChannel;
    }
    inline float getGain() const
    {
        return gain;
    }
    inline int getBoost() const
    {
        return boost;
    }
    inline float getPan() const
    {
        return pan;
    }
    inline bool isMuted() const
    {
        return muted;
    }
    inline bool isStereoInverted() const
    {
        return stereoInverted;
    }
    inline qint8 getTranspose() const
    {
        return transpose;
    }
    inline quint8 getLowerMidiNote() const
    {
        return lowerMidiNote;
    }
    inline quint8 getHigherMidiNote() const
    {
        return higherMidiNote;
    }
    inline bool isRoutingMidiToFirstSubchannel() const
    {
        return routingMidiToFirstSubchannel;
    }
private:
    QList<persistence::Plugin> plugins;
    int firstInput;
    int channelsCount;
    int midiDevice;
    int midiChannel;
    float gain;
    int boost; // [-1, 0, +1]
    float pan;
    bool muted;
    bool stereoInverted;
    qint8 transpose; // midi transpose
    quint8 lowerMidiNote; // midi rey range
    quint8 higherMidiNote;
    bool routingMidiToFirstSubchannel;
};

class Channel final
{
public:
    class Builder final {
    public:
        Builder();
        Builder(const QJsonObject &in, bool allowSubchannels);
        inline Builder& setInstrumentIndex(int instrumentIndex)
        {
            this->instrumentIndex = instrumentIndex;
            return *this;
        }
        inline Builder& addSubChannel(const SubChannel& subChannel)
        {
            subChannels.append(subChannel);
            return *this;
        }
        inline Channel build()
        {
            return Channel(*this);
        }
        friend class Channel;
    private:
        int instrumentIndex;
        QList<SubChannel> subChannels;
    };
    explicit Channel(const Builder& builder);
    void write(QJsonObject &out) const;
    inline int getInstrumentIndex() const
    {
        return instrumentIndex;
    }
    inline const QList<SubChannel>& getSubChannels() const
    {
        return subChannels;
    }
    inline bool hasSubChannels() const
    {
        return !subChannels.isEmpty();
    }
private:
    int instrumentIndex;
    QList<SubChannel> subChannels;
};

class LocalInputTrackSettings final : public SettingsObject
{
public:
    class Builder final {
    public:
        Builder() = default;
        inline Builder& addChannel(const Channel& channel)
        {
            channels.append(channel);
            return *this;
        }
        inline LocalInputTrackSettings build()
        {
            return LocalInputTrackSettings(*this);
        }
        friend class LocalInputTrackSettings;
    private:
        QList<Channel> channels;
    };

    explicit LocalInputTrackSettings(const Builder& builder);
    explicit LocalInputTrackSettings(bool createOneTrack = false);
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    void read(const QJsonObject &in, bool allowSubchannels);
    inline const QList<Channel>& getChannels() const
    {
        return channels;
    }
    inline bool isValid() const
    {
        return !channels.isEmpty();
    }
private:
    QList<Channel> channels;
};

}

#endif // LOCALINPUTTRACKSETTINGS_H
