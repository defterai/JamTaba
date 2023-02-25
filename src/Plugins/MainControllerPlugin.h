#ifndef MAINCONTROLLERPLUGIN_H
#define MAINCONTROLLERPLUGIN_H

#include "MainController.h"
#include "NinjamController.h"
#include "audio/core/PluginDescriptor.h"
#include "NinjamControllerPlugin.h"

class JamTabaPlugin;

namespace persistence {
class Settings;
class Preset;
}

namespace audio {
class AudioDriver;
class LocalInputNode;
}

using persistence::Settings;
using persistence::Preset;
using audio::AudioDriver;
using audio::LocalInputNode;

class MainControllerPlugin : public controller::MainController
{

    friend class NinjamControllerPlugin;

public:
    MainControllerPlugin(const Settings &settings, JamTabaPlugin *plugin);
    virtual ~MainControllerPlugin();

    QString getUserEnvironmentString() const override;

    AudioDriver *createAudioDriver(const Settings &settings);

    NinjamControllerPlugin *createNinjamController() override;

    inline NinjamControllerPlugin *getNinjamController() const override
    {
        return dynamic_cast<NinjamControllerPlugin *>(ninjamController.data());
    }

    void setCSS(const QString &css) override;

    QSharedPointer<audio::LocalInputNode> createInputNode(int groupIndex) override;

    void setSampleRate(int newSampleRate) override;

    float getSampleRate() const override;

    midi::MidiDriver *createMidiDriver();

    inline void exit() // TODO remove this ??
    {
    }

    int getHostBpm() const;

    QString getHostName() const;

    virtual void resizePluginEditor(int newWidth, int newHeight) = 0;

    Preset loadPreset(const QString &name) override;

    inline QVector<midi::MidiMessage> pullMidiMessagesFromPlugins() override
    {
        return QVector<midi::MidiMessage>(); // empty buffer
    }

    void startMidiClock() const override {};
    void stopMidiClock() const override {};
    void continueMidiClock() const override {};
    void sendMidiClockPulse() const override {};

protected:
    inline QVector<midi::MidiMessage> pullMidiMessagesFromDevices() override
    {
        return QVector<midi::MidiMessage>(); // empty buffer
    }

    JamTabaPlugin *plugin;

};

#endif // MAINCONTROLLERPLUGIN_H
