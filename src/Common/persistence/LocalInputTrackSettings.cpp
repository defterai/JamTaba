#include "LocalInputTrackSettings.h"
#include "log/Logging.h"
#include "Utils.h"
#include <QJsonArray>
#include <QFile>

using namespace persistence;

Plugin::Plugin(const Builder& builder) :
    path(builder.path),
    name(builder.name),
    manufacturer(builder.manufacturer),
    bypassed(builder.bypassed),
    data(builder.data),
    category(builder.category)
{
    qCDebug(jtSettings) << "Plugin ctor";
}

void Plugin::write(QJsonObject &out) const
{
    out["name"] = name;
    if (!path.isEmpty()) {
        out["path"] = path;
    }
    out["bypassed"] = bypassed;
    if (!data.isEmpty()) {
        out["data"] = QString(data.toBase64());
    }
    out["category"] = static_cast<quint8>(category);
    if (!manufacturer.isEmpty()) {
        out["manufacturer"] = manufacturer;
    }
}

Plugin::Builder::Builder(const audio::PluginDescriptor &descriptor):
    path(descriptor.getPath()),
    name(descriptor.getName()),
    manufacturer(descriptor.getManufacturer()),
    category(descriptor.getCategory()) {

}

Plugin::Builder::Builder(const QJsonObject &in):
    path(in["path"].toString("")),
    name(in["name"].toString("")),
    manufacturer(in["manufacturer"].toString("")),
    bypassed(in["bypassed"].toBool(false)),
    category(static_cast<audio::PluginDescriptor::Category>(in["category"].toInt(int(audio::PluginDescriptor::Category::VST_Plugin))))
{
    QString dataString = in["data"].toString("");
    if (!dataString.isEmpty()) {
        QByteArray rawByteArray(dataString.toStdString().c_str());
        data = QByteArray::fromBase64(rawByteArray);
    }
}

SubChannel::SubChannel(const Builder& builder):
    plugins(builder.plugins),
    firstInput(builder.firstInput),
    channelsCount(builder.channelsCount),
    midiDevice(builder.midiDevice),
    midiChannel(builder.midiChannel),
    gain(builder.gain),
    boost(builder.boost),
    pan(builder.pan),
    muted(builder.muted),
    stereoInverted(builder.stereoInverted),
    transpose(builder.transpose),
    lowerMidiNote(builder.lowerMidiNote),
    higherMidiNote(builder.higherMidiNote),
    routingMidiToFirstSubchannel(builder.routingMidiToFirstSubchannel)
{
    qCDebug(jtSettings) << "SubChannel ctor";
}

void SubChannel::write(QJsonObject &out, bool skipMidiRouting) const
{
    out["firstInput"] = firstInput;
    out["channelsCount"] = channelsCount;
    out["midiDevice"] = midiDevice;
    out["midiChannel"] = midiChannel;
    out["gain"] = gain;
    out["boost"] = boost;
    out["pan"] = pan;
    out["muted"] = muted;
    out["stereoInverted"] = stereoInverted;
    out["transpose"] = transpose;
    out["lowerNote"] = lowerMidiNote;
    out["higherNote"] = higherMidiNote;
    if (!skipMidiRouting) {
        out["routingMidiInput"] = routingMidiToFirstSubchannel;
    }
    QJsonArray pluginsArray;
    for (const auto &plugin : plugins) {
        QJsonObject pluginObject;
        plugin.write(pluginObject);
        pluginsArray.append(pluginObject);
    }
    out["plugins"] = pluginsArray;
}

void SubChannel::setPlugins(const QList<Plugin> &newPlugins)
{
    if (newPlugins.length() >= Plugin::MAX_PROCESSORS_PER_TRACK) {
        plugins.clear();
        for (int i = 0; i < Plugin::MAX_PROCESSORS_PER_TRACK; ++i) {
            plugins.append(newPlugins.at(i));
        }
    } else {
        plugins = newPlugins;
    }
}

SubChannel::Builder::Builder():
    firstInput(0),
    channelsCount(2),
    midiDevice(-1),
    midiChannel(-1),
    gain(1.0f),
    boost(0.0f),
    pan(0.0f),
    muted(false),
    stereoInverted(false),
    transpose(0),
    lowerMidiNote(0),
    higherMidiNote(127),
    routingMidiToFirstSubchannel(false)
{

}

SubChannel::Builder::Builder(const QJsonObject &in, bool skipMidiRouting):
    firstInput(in["firstInput"].toInt(0)),
    channelsCount(in["channelsCount"].toInt(0)),
    midiDevice(in["midiDevice"].toInt(-1)),
    midiChannel(in["midiChannel"].toInt(-1)),
    gain(Utils::clampGain(in["gain"].toDouble(1))),
    boost(Utils::clampBoost(in["boost"].toInt(0))),
    pan(Utils::clampPan(in["pan"].toDouble(0))),
    muted(in["muted"].toBool(false)),
    stereoInverted(in["stereoInverted"].toBool(false)),
    transpose(in["transpose"].toInt(0)),
    lowerMidiNote(in["lowerNote"].toInt(0)),
    higherMidiNote(in["higherNote"].toInt(127)),
    routingMidiToFirstSubchannel(!skipMidiRouting && in["routingMidiInput"].toBool(false))
{
    // adjust transpose, low, high values
    setTranspose(transpose);
    setLowerMidiNote(lowerMidiNote);
    setHigherMidiNote(higherMidiNote);
    if (in.contains("plugins")) {
        QJsonArray pluginsArray = in["plugins"].toArray();
        for (int p = 0; p < pluginsArray.size(); ++p) {
            QJsonObject pluginObject = pluginsArray.at(p).toObject();
            Plugin::Builder pluginBuilder(pluginObject);
            Plugin plugin = pluginBuilder.build();
            bool pathIsValid = !plugin.getPath().isEmpty();
            if (plugin.getCategory() == audio::PluginDescriptor::VST_Plugin) {
                pathIsValid = QFile::exists(plugin.getPath());
            }
            if (pathIsValid) {
                plugins.append(plugin);
                if (plugins.length() >= Plugin::MAX_PROCESSORS_PER_TRACK) {
                    break; // ignore all plugins above allowed count
                }
            }
        }
    }
}

SubChannel::Builder& SubChannel::Builder::setBoost(int boost)
{
    this->boost = Utils::clampBoost(boost);
    return *this;
}

SubChannel::Builder& SubChannel::Builder::setGain(float gain)
{
    this->gain = Utils::clampGain(gain);
    return *this;
}

SubChannel::Builder& SubChannel::Builder::setPan(float pan)
{
    this->pan = Utils::clampPan(pan);
    return *this;
}

SubChannel::Builder& SubChannel::Builder::setTranspose(qint8 transpose)
{
    if (transpose > MAX_MIDI_TRANSPOSE)
        this->transpose = MAX_MIDI_TRANSPOSE;
    else if (transpose < MIN_MIDI_TRANSPOSE)
        this->transpose = MIN_MIDI_TRANSPOSE;
    else
        this->transpose = transpose;
    return *this;
}

SubChannel::Builder& SubChannel::Builder::setLowerMidiNote(quint8 lowerMidiNote)
{
    if (lowerMidiNote > 127)
        this->lowerMidiNote = 127;
    else
        this->lowerMidiNote = lowerMidiNote;
    return *this;
}

SubChannel::Builder& SubChannel::Builder::setHigherMidiNote(quint8 higherMidiNote)
{
    if (higherMidiNote > 127)
        this->higherMidiNote = 127;
    else
        this->higherMidiNote = higherMidiNote;
    return *this;
}

Channel::Channel(const Builder& builder):
    instrumentIndex(builder.instrumentIndex),
    subChannels(builder.subChannels)
{
    qCDebug(jtSettings) << "Channel ctor";
}

void Channel::write(QJsonObject &out) const
{
    out["instrument"] = instrumentIndex;
    QJsonArray subchannelsArrays;
    int subchannelsCount = 0;
    for (const SubChannel &sub : subChannels) {
        QJsonObject subChannelObject;
        sub.write(subChannelObject, subchannelsCount == 0);
        subchannelsArrays.append(subChannelObject);
        subchannelsCount++;
    }
    out["subchannels"] = subchannelsArrays;
}

Channel::Builder::Builder():
    instrumentIndex(-1)
{

}

Channel::Builder::Builder(const QJsonObject &in, bool allowSubchannels):
    instrumentIndex(in["instrument"].toInt(-1))
{
    if (in.contains("subchannels")) {
        QJsonArray subChannelsArray = in["subchannels"].toArray();
        int subChannelsLimit = allowSubchannels ? subChannelsArray.size() : 1;
        for (int k = 0; k < subChannelsLimit; ++k) {
            QJsonObject subChannelObject = subChannelsArray.at(k).toObject();
            SubChannel::Builder subChannelBuilder(subChannelObject, k == 0);
            persistence::SubChannel subChannel = subChannelBuilder.build();
            subChannels.append(subChannel);
        }
    }
}

LocalInputTrackSettings::LocalInputTrackSettings(const Builder& builder) :
    SettingsObject("inputs"),
    channels(builder.channels)
{
    qCDebug(jtSettings) << "LocalInputTrackSettings ctor";
}

LocalInputTrackSettings::LocalInputTrackSettings(bool createOneTrack) :
    SettingsObject("inputs")
{
    qCDebug(jtSettings) << "LocalInputTrackSettings ctor";
    if (createOneTrack) {
        Channel::Builder channelBuilder;
        channelBuilder.addSubChannel(SubChannel::Builder().build());
        channels.append(channelBuilder.build());
    }
}

void LocalInputTrackSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "LocalInputTrackSettings write";
    QJsonArray channelsArray;
    for (const Channel &channel : channels) {
        QJsonObject channelObject;
        channel.write(channelObject);
        channelsArray.append(channelObject);
    }
    out["channels"] = channelsArray;
}

void LocalInputTrackSettings::read(const QJsonObject &in, bool allowSubchannels)
{
    qCDebug(jtSettings) << "LocalInputTrackSettings read (too complex to dump...)";

    if (in.contains("channels")) {
        QJsonArray channelsArray = in["channels"].toArray();
        for (int i = 0; i < channelsArray.size(); ++i) {
            QJsonObject channelObject = channelsArray.at(i).toObject();
            Channel::Builder channelBuilder(channelObject, allowSubchannels);
            persistence::Channel channel = channelBuilder.build();
            if (channel.hasSubChannels()) {
                channels.append(channel);
            }
        }
    }
}

void LocalInputTrackSettings::read(const QJsonObject &in)
{
    read(in, true); // allowing multi subchannel by default
}
