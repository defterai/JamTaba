#include "MainControllerPlugin.h"
#include "midi/MidiDriver.h"
#include "audio/core/LocalInputNode.h"
#include "controller/AudioController.h"
#include "gui/MainWindow.h"
#include "JamTabaPlugin.h"
#include "log/Logging.h"

MainControllerPlugin::MainControllerPlugin(const Settings &settings, JamTabaPlugin *plugin) :
    MainController(settings),
    plugin(plugin)
{
    qCDebug(jtCore) << "Creating MainControllerVST instance!";
}

MainControllerPlugin::~MainControllerPlugin()
{
    saveLastUserSettings();
}

persistence::Preset MainControllerPlugin::loadPreset(const QString &name)
{
    return settings.readPresetFromFile(name, false); // don't allow multi subchannels in vst plugin and avoid hacking in json file to create subchannels in VSt plugin.
}

QSharedPointer<audio::LocalInputNode> MainControllerPlugin::createInputNode(int groupIndex)
{
    auto inputTrackNode = MainController::createInputNode(groupIndex);

    // VST plugins always use audio as input

    int inputTracksCount;
    audioController->postTask([&]() {
        inputTracksCount = audioController->getInputTracksCount();
    }).waitForFinished();
    int firstChannelIndex = (inputTracksCount - 1) * 2;
    emit inputTrackNode->postSetAudioInputProps(audio::LocalAudioInputProps(firstChannelIndex, 2), this); //stereo
    emit inputTrackNode->postSetInputMode(audio::LocalInputMode::AUDIO, this);
    return inputTrackNode;
}

QString MainControllerPlugin::getUserEnvironmentString() const
{
    return MainController::getUserEnvironmentString() + " running in " + getHostName();
}

QString MainControllerPlugin::getHostName() const
{
    if (plugin)
        return plugin->getHostName();
    return "(Error getting host name)";
}

int MainControllerPlugin::getHostBpm() const
{
    if (plugin)
        return plugin->getHostBpm();
    return -1;
}

void MainControllerPlugin::setSampleRate(int newSampleRate)
{
    //this->sampleRate = newSampleRate;
    MainController::setSampleRate(newSampleRate);
}

float MainControllerPlugin::getSampleRate() const
{
    if (plugin)
        return plugin->getSampleRate();

    return 0;
}

NinjamControllerPlugin *MainControllerPlugin::createNinjamController()
{
    return new NinjamControllerPlugin(const_cast<MainControllerPlugin*>(this));
}

midi::MidiDriver *MainControllerPlugin::createMidiDriver()
{
    return new midi::NullMidiDriver();
}

void MainControllerPlugin::setCSS(const QString &css)
{
    
    if (qApp) {
        qCDebug(jtCore) << "setting CSS";
        qApp->setStyleSheet(css); // qApp is a global variable created in dll main.
    }
    else
    {
        qWarning() << "Can´t set CSS, qApp is null!";
    }
}
