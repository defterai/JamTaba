#include "MainControllerStandalone.h"

#include "midi/RtMidiDriver.h"
#include "midi/MidiMessage.h"
#include "audio/PortAudioDriver.h"
#include "audio/core/LocalInputNode.h"
#include "vst/VstPlugin.h"
#include "vst/VstHost.h"
#include "vst/VstPluginFinder.h"
#include "audio/core/PluginDescriptor.h"
#include "NinjamController.h"
#include "vst/VstPluginChecker.h"
#include "gui/MainWindowStandalone.h"
#include "ninjam/client/Service.h"

#include <QDialog>
#include <QHostAddress>

#ifdef Q_OS_WIN
    #include <windows.h>
#endif

#ifdef Q_OS_MAC
    #include <AudioUnit/AudioUnit.h>
    #include "AU/AudioUnitPlugin.h"
#endif

#include <QDataStream>
#include <QFile>
#include <QDirIterator>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include "log/Logging.h"
#include "Configurator.h"
#include "Helpers.h"

#include "controller/AudioController.h"

using ninjam::client::ServerInfo;

QString MainControllerStandalone::getJamtabaFlavor() const
{
    return "Standalone";
}

void MainControllerStandalone::setInputTrackToMono(const QSharedPointer<audio::LocalInputNode>& inputTrack,
                                                   int inputIndexInAudioDevice)
{
    if (!inputIndexIsValid(inputIndexInAudioDevice)) // use the first available channel?
        inputIndexInAudioDevice = 0;

    int availableInputs = audioDriver->getInputsCount();
    if (availableInputs > 0) {
        emit inputTrack->postSetAudioInputProps(audio::LocalAudioInputProps(inputIndexInAudioDevice, 1), this); // mono
        emit inputTrack->postSetInputMode(audio::LocalInputMode::AUDIO, this);
    } else {
        emit inputTrack->postSetInputMode(audio::LocalInputMode::DISABLED, this);
    }
    if (window) {
        window->refreshTrackInputSelection(inputTrack->getID());
    }
}

bool MainControllerStandalone::pluginDescriptorLessThan(const audio::PluginDescriptor &d1,
                                                        const audio::PluginDescriptor &d2)
{
    return d1.getName().localeAwareCompare(d2.getName()) < 0;
}

/**
 * @param category: VST, AU or NATIVE
 * @return A map indexed by plugin Manufacturer name.
 */

QMap<QString, QList<audio::PluginDescriptor> > MainControllerStandalone::getPluginsDescriptors(
    audio::PluginDescriptor::Category category)
{
    QMap<QString, QList<audio::PluginDescriptor> > descriptors;

    for (const auto &descriptor : pluginsDescriptors)
    {
        if (descriptor.getCategory() == category)
        {
            QString manufacturer = descriptor.getManufacturer();
            if (!descriptors.contains(manufacturer))
                descriptors.insert(manufacturer, QList<audio::PluginDescriptor>());

            descriptors[manufacturer].append(descriptor);
        }
    }

    for (const QString &manufacturer : descriptors.keys())
        qSort(descriptors[manufacturer].begin(),
              descriptors[manufacturer].end(), pluginDescriptorLessThan);

    return descriptors;
}

void MainControllerStandalone::addPluginsScanPath(const QString &path)
{
    settings.vstSettings.addPluginScanPath(path);
}

void MainControllerStandalone::removePluginsScanPath(const QString &path)
{
    settings.vstSettings.removePluginScanPath(path);
}

void MainControllerStandalone::clearPluginsList()
{
    pluginsDescriptors.clear();
}

void MainControllerStandalone::clearPluginsCache()
{
    settings.vstSettings.clearPluginsCache();

#ifdef Q_OS_MAC
    settings.audioUnitSettings.clearPluginsCache();
#endif
}

// VST BlackList ...
void MainControllerStandalone::addBlackVstToSettings(const QString &path)
{
    settings.vstSettings.addIgnoredPlugin(path);
}

void MainControllerStandalone::removeBlackVstFromSettings(const QString &pluginPath)
{
    settings.vstSettings.removeIgnoredPlugin(pluginPath);
}

bool MainControllerStandalone::inputIndexIsValid(int inputIndex)
{
    return inputIndex >= 0 && inputIndex <= audioDriver->getInputsCount();
}

void MainControllerStandalone::setInputTrackToMIDI(const QSharedPointer<audio::LocalInputNode>& inputTrack,
                                                   const audio::MidiInputProps& midiInpuProps)
{
    emit inputTrack->postSetMidiInputProps(midiInpuProps, this);
    emit inputTrack->postSetInputMode(audio::LocalInputMode::MIDI, this);

    if (window) {
        window->refreshTrackInputSelection(inputTrack->getID());
    }
}

void MainControllerStandalone::setInputTrackToNoInput(const QSharedPointer<audio::LocalInputNode>& inputTrack)
{
    emit inputTrack->postSetInputMode(audio::LocalInputMode::DISABLED, this);
    if (window) {
        window->refreshTrackInputSelection(inputTrack->getID());
    }
    if (isPlayingInNinjamRoom()) {     // send the finish interval message
        if (audioIntervalsToUpload.contains(inputTrack->getID())) {
            ninjamService->sendIntervalPart(audioIntervalsToUpload[inputTrack->getID()].getGUID(), QByteArray(), true);
        }
    }
}

void MainControllerStandalone::setInputTrackToStereo(const QSharedPointer<audio::LocalInputNode>& inputTrack,
                                                     int firstInputIndex)
{
    if (!inputIndexIsValid(firstInputIndex))
        firstInputIndex = 0;    // use the first channel
    int availableInputChannels = audioDriver->getInputsCount();
    if (availableInputChannels > 0) {     // we have input channels?
        if (availableInputChannels >= 2) {    // can really use stereo?
            emit inputTrack->postSetAudioInputProps(audio::LocalAudioInputProps(firstInputIndex, 2), this);    // stereo
        } else {
            emit inputTrack->postSetAudioInputProps(audio::LocalAudioInputProps(firstInputIndex, 1), this);    // mono
        }
        emit inputTrack->postSetInputMode(audio::LocalInputMode::AUDIO, this);
    } else {
        emit inputTrack->postSetInputMode(audio::LocalInputMode::DISABLED, this);
    }

    if (window) {
        window->refreshTrackInputSelection(inputTrack->getID());
    }
}

void MainControllerStandalone::updateBpm(int newBpm)
{
    MainController::updateBpm(newBpm);

    for (Host *host : hosts)
        host->setTempo(newBpm);
}

void MainControllerStandalone::connectInNinjamServer(const ServerInfo &server)
{
    MainController::connectInNinjamServer(server);

    for (auto host : hosts)
        host->setTempo(server.getBpm());
}

void MainControllerStandalone::setSampleRate(int newSampleRate)
{
    MainController::setSampleRate(newSampleRate);

    for (auto host : hosts)
        host->setSampleRate(newSampleRate);

    audioDriver->setSampleRate(newSampleRate);
}

void MainControllerStandalone::setBufferSize(int newBufferSize)
{
    for (auto host : hosts)
        host->setBlockSize(newBufferSize);

    audioDriver->setBufferSize(newBufferSize);
    settings.audioSettings.setBufferSize(newBufferSize);
}

void MainControllerStandalone::on_audioDriverStarted()
{
    emit audioController->postEnumInputsOnPool([](QSharedPointer<audio::LocalInputNode> inputTrack) {
        inputTrack->resumeProcessors();
        return true; // continue next input
    }, audioController->getPluginsThreadPool());
}

void MainControllerStandalone::on_audioDriverStopped()
{
    emit audioController->postEnumInputsOnPool([](QSharedPointer<audio::LocalInputNode> inputTrack) {
        inputTrack->suspendProcessors();    // suspend plugins
        return true; // continue next input
    }, audioController->getPluginsThreadPool());
}

void MainControllerStandalone::handleNewNinjamInterval()
{
    MainController::handleNewNinjamInterval();

    for (auto host : hosts)
        host->setPlayingFlag(true);
}

void MainControllerStandalone::setupNinjamControllerSignals()
{
    MainController::setupNinjamControllerSignals();

    connect(ninjamController.data(), &NinjamController::startProcessing, this, &MainControllerStandalone::on_ninjamStartProcessing);
}

void MainControllerStandalone::clearNinjamControllerSignals()
{
    MainController::clearNinjamControllerSignals();

    disconnect(ninjamController.data(), &NinjamController::startProcessing, this, &MainControllerStandalone::on_ninjamStartProcessing);
}

void MainControllerStandalone::on_ninjamStartProcessing(int intervalPosition)
{
    for (auto host : hosts)
        host->setPositionInSamples(intervalPosition); // update the vst host time line in every audio callback.
}

void MainControllerStandalone::addFoundedVstPlugin(const QString &name, const QString &path)
{
    bool containThePlugin = false;
    for (const auto& descriptor : qAsConst(pluginsDescriptors))
    {
        if (descriptor.isVST() && descriptor.getPath() == path)
        {
            containThePlugin = true;
            break;
        }
    }

    if (!containThePlugin)
    {
        settings.vstSettings.addPlugin(path);
        auto category = audio::PluginDescriptor::VST_Plugin;
        QString manufacturer = "";
        pluginsDescriptors.append(audio::PluginDescriptor(name, category, manufacturer, path));
    }
}

#ifdef Q_OS_MAC
void MainControllerStandalone::addFoundedAudioUnitPlugin(const QString &name, const QString &path)
{
    bool containThePlugin = false;
    for (const auto descriptor : pluginsDescriptors)
    {
        if (descriptor.isAU() && descriptor.getPath() == path)
        {
            containThePlugin = true;
            break;
        }
    }
    if (!containThePlugin)
    {
        settings.audioUnitSettings.addPlugin(path);
        pluginsDescriptors.append(au::createPluginDescriptor(name, path));
    }
}

#endif

void MainControllerStandalone::setMainWindow(MainWindow *mainWindow)
{
    MainController::setMainWindow(mainWindow);

    // store a casted pointer to convenience when calling MainWindowStandalone specific functions
    window = dynamic_cast<MainWindowStandalone *>(mainWindow);
}

midi::MidiDriver *MainControllerStandalone::createMidiDriver()
{
    // return new Midi::PortMidiDriver(settings.getMidiInputDevicesStatus());
    return new midi::RtMidiDriver(settings.midiSettings.getInputDevicesStatus(),
                                  settings.syncSettings.getOutputDevicesStatus());
    // return new Midi::NullMidiDriver();
}

controller::NinjamController *MainControllerStandalone::createNinjamController()
{
    return new NinjamController(this);
}

QSharedPointer<audio::AudioDriver> MainControllerStandalone::createAudioDriver()
{
    auto driver = audio::PortAudioDriver::CreateInstance();
    if (driver) {
        driver->configure(this->settings.audioSettings);
        return driver.staticCast<audio::AudioDriver>();
    }
    return nullptr;
}

MainControllerStandalone::MainControllerStandalone(persistence::Settings settings,
                                                   QApplication *application) :
    MainController(settings),
    application(application),
    audioDriver(nullptr)
{
    application->setQuitOnLastWindowClosed(true);

    hosts.append(vst::VstHost::getInstance());
#ifdef Q_OS_MAC
    hosts.append(au::AudioUnitHost::getInstance());
#endif

    connect(vst::VstHost::getInstance(), &vst::VstHost::pluginRequestingWindowResize,
            this, &MainControllerStandalone::setVstPluginWindowSize);
}

void MainControllerStandalone::setVstPluginWindowSize(QString pluginName, int newWidht,
                                                      int newHeight)
{
    auto pluginEditorWindow = vst::VstPlugin::getPluginEditorWindow(pluginName);
    if (pluginEditorWindow)
    {
        pluginEditorWindow->setFixedSize(newWidht, newHeight);
        // pluginEditorWindow->updateGeometry();
    }
}

void MainControllerStandalone::start()
{
    // creating audio and midi driver before call start() in base class (MainController::start())

    if (!midiDriver)
    {
        qCInfo(jtCore) << "Creating midi driver...";
        midiDriver.reset(createMidiDriver());
    }

    if (!audioDriver)
    {
        qCInfo(jtCore) << "Creating audio driver...";
        QSharedPointer<audio::AudioDriver> driver;
        try
        {
            driver = createAudioDriver();
        }
        catch (const std::runtime_error &error)
        {
            qCritical() << "Audio initialization fail: " << QString::fromUtf8(
                error.what());
            QMessageBox::warning(window, "Audio Initialization Problem!", error.what());
        }
        if (!driver)
            driver = createQSharedPointer<audio::NullAudioDriver>();

        audioDriver = driver;

        QObject::connect(audioDriver.data(), &audio::AudioDriver::sampleRateChanged, this,
                         &MainControllerStandalone::setSampleRate);
        QObject::connect(audioDriver.data(), &audio::AudioDriver::stopped, this,
                         &MainControllerStandalone::on_audioDriverStopped);
        QObject::connect(audioDriver.data(), &audio::AudioDriver::started, this,
                         &MainControllerStandalone::on_audioDriverStarted);

        QObject::connect(audioDriver.data(), &audio::AudioDriver::processDataAvailable,
                         [&](QFutureInterface<void> futureInterface,
                         QSharedPointer<audio::SamplesBuffer> in,
                         QSharedPointer<audio::SamplesBuffer> out) {
            process(in, out);
            futureInterface.reportFinished();
        });
    }

    // calling the base class
    MainController::start();

    if (audioDriver)
    {
        if (!audioDriver->canBeStarted())
            useNullAudioDriver();
        audioDriver->start();
    }

    if (midiDriver)
        midiDriver->start(settings.midiSettings.getInputDevicesStatus(),
                          settings.syncSettings.getOutputDevicesStatus());

    qCInfo(jtCore) << "Creating plugin finder...";
    vstPluginFinder.reset(new audio::VSTPluginFinder());

#ifdef Q_OS_MAC

    auPluginFinder.reset(new audio::AudioUnitPluginFinder());

    connect(auPluginFinder.data(), &audio::AudioUnitPluginFinder::pluginScanFinished, this,
            &MainControllerStandalone::addFoundedAudioUnitPlugin);

#endif

    connect(vstPluginFinder.data(), &audio::VSTPluginFinder::pluginScanFinished, this,
            &MainControllerStandalone::addFoundedVstPlugin);

    if (audioDriver)
    {
        for (auto host : hosts)
        {
            host->setSampleRate(audioDriver->getSampleRate());
            host->setBlockSize(audioDriver->getBufferSize());
        }
    }
}

void MainControllerStandalone::cancelPluginFinders()
{
    if (vstPluginFinder)
        vstPluginFinder->cancel();

#ifdef Q_OS_MAC

    if (auPluginFinder)
        auPluginFinder->cancel();

#endif
}

void MainControllerStandalone::setCSS(const QString &css)
{
    application->setStyleSheet(css);
}

MainControllerStandalone::~MainControllerStandalone()
{
    qCDebug(jtCore) << "StandaloneMainController destructor!";
    // pluginsDescriptors.clear();
}

QSharedPointer<audio::Plugin> MainControllerStandalone::createPluginInstance(
    const audio::PluginDescriptor &descriptor)
{
    if (descriptor.isNative())
    {
        if (descriptor.getName() == "Delay")
            return createQSharedPointer<audio::JamtabaDelay>(audioDriver->getSampleRate());
    }
    else if (descriptor.isVST())
    {
        auto host = vst::VstHost::getInstance();
        return vst::VstPlugin::load(host, descriptor);
    }

#ifdef Q_OS_MAC

    else if (descriptor.isAU())
    {
        return au::audioUnitPluginfromPath(descriptor.getPath());
    }

#endif

    return nullptr;
}

QStringList MainControllerStandalone::getSteinbergRecommendedPaths()
{
    /*
    On a 64-bit OS

    64-bit plugins path = HKEY_LOCAL_MACHINE\SOFTWARE\VST
    32-bit plugins path = HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\VST
    */

    QStringList vstPaths;

#ifdef Q_OS_WIN

    #ifdef _WIN64 // 64 bits
    QSettings wowSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\VST\\",
                          QSettings::NativeFormat);
    vstPaths.append(wowSettings.value("VSTPluginsPath").toString());
    #else // 32 bits
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\VST\\", QSettings::NativeFormat);
    vstPaths.append(settings.value("VSTPluginsPath").toString());
    #endif
#endif

#ifdef Q_OS_MACX
    vstPaths.append("/Library/Audio/Plug-Ins/VST");
    vstPaths.append("~/Library/Audio/Plug-Ins/VST");
#endif

#ifdef Q_OS_LINUX
    // Steinberg VST 2.4 docs are saying nothing about default paths for VST plugins in Linux. But I see these paths
    // in a Linux plugin documentation...

    vstPaths.append("~/.vst/");
    vstPaths.append("/user/lib/vst");
#endif

    return vstPaths;
}

void MainControllerStandalone::addDefaultPluginsScanPath()
{
    QStringList vstPaths;

    // first try read the path store in registry by Jamtaba installer.
    // If this value is not founded use the Steinberg recommended paths.
    QSettings jamtabaRegistryEntry("HKEY_CURRENT_USER\\SOFTWARE\\Jamtaba", QSettings::NativeFormat);
    QString vst2InstallDir = jamtabaRegistryEntry.value("VST2InstallDir").toString();

    if (!vst2InstallDir.isEmpty())
        vstPaths.append(vst2InstallDir);
    else
        vstPaths.append(getSteinbergRecommendedPaths());

    for (const QString &vstPath : vstPaths)
    {
        if (!vstPath.isEmpty() && QDir(vstPath).exists())
            addPluginsScanPath(vstPath);
    }
}

/**
 * We need scan plugins when plugins cache is empty OR we have new plugins in scan folders
 * This code is executed whem Jamtaba is started.
*/

bool MainControllerStandalone::vstScanIsNeeded() const
{
    bool vstCacheIsEmpty = settings.vstSettings.getPluginPaths().isEmpty();
    if (vstCacheIsEmpty)
        return true;

    // checking for new vst plugins in scan folders
    const QStringList& foldersToScan = settings.vstSettings.getPluginScanPaths();

    QStringList skipList(settings.vstSettings.getIgnoredPlugins());
    skipList.append(settings.vstSettings.getPluginScanPaths());

    bool newVstFounded = false;
    for (const QString &scanFolder : foldersToScan)
    {
        QDirIterator folderIterator(scanFolder, QDir::AllDirs | QDir::NoDotAndDotDot,
                                    QDirIterator::Subdirectories);
        while (folderIterator.hasNext())
        {
            folderIterator.next(); // point to next file inside current folder
            QString filePath = folderIterator.filePath();
            if (!skipList.contains(filePath) && Vst::PluginChecker::isValidPluginFile(filePath))
            {
                newVstFounded = true;
                break;
            }
        }
        if (newVstFounded)
            break;
    }

    return newVstFounded;
}

#ifdef Q_OS_MAC

void MainControllerStandalone::initializeAudioUnitPluginsList(const QStringList &paths)
{
    for (const QString &path : paths)
        qDebug() << "Inicializando " << path;
}

#endif

void MainControllerStandalone::initializeVstPluginsList(const QStringList &paths)
{
    auto category = audio::PluginDescriptor::VST_Plugin;
    for (const QString &path : paths)
    {
        QFile file(path);
        if (file.exists())
        {
            QString pluginName = audio::PluginDescriptor::getVstPluginNameFromPath(path);
            QString manufacturer = "";
            pluginsDescriptors.append(audio::PluginDescriptor(pluginName, category, manufacturer,
                                                              path));
        }
    }
}

void MainControllerStandalone::scanAllVstPlugins()
{
    saveLastUserSettings(); // save the config file before start scanning
    clearPluginsCache();
    scanVstPlugins(false);
}

void MainControllerStandalone::scanOnlyNewVstPlugins()
{
    saveLastUserSettings(); // save the config file before start scanning
    scanVstPlugins(true);
}

void MainControllerStandalone::scanVstPlugins(bool scanOnlyNewPlugins)
{
    if (vstPluginFinder)
    {
        if (!scanOnlyNewPlugins)
            pluginsDescriptors.clear();

        // The skipList contains the paths for black listed plugins by default.
        // If the parameter 'scanOnlyNewPlugins' is 'true' the cached plugins are added in the skipList too.
        QStringList skipList(settings.vstSettings.getIgnoredPlugins());
        if (scanOnlyNewPlugins)
            skipList.append(settings.vstSettings.getPluginPaths());

        const QStringList& foldersToScan = settings.vstSettings.getPluginScanPaths();
        vstPluginFinder->scan(foldersToScan, skipList);
    }
}

#ifdef Q_OS_MAC
void MainControllerStandalone::scanAudioUnitPlugins()
{
    if (auPluginFinder)
        auPluginFinder->scan();
}

#endif

void MainControllerStandalone::openExternalAudioControlPanel()
{
    if (audioDriver->hasControlPanel()) // just in case
        audioDriver->openControlPanel((void *)mainWindow->winId());
}

void MainControllerStandalone::stopNinjamController()
{
    MainController::stopNinjamController();

    for (Host *host : hosts)
        host->setPlayingFlag(false);
}

void MainControllerStandalone::quit()
{
    // destroy the extern !
    // if(JTBConfig)delete JTBConfig;
    qDebug() << "Thank you for Jamming with Jamtaba !";
    application->quit();
}

QVector<midi::MidiMessage> MainControllerStandalone::pullMidiMessagesFromPlugins()
{
    // return midi messages created by vst and AU plugins, not by midi controllers.
    QVector<midi::MidiMessage> receivedMidiMessages;
    for (auto host : hosts)
    {
        std::vector<midi::MidiMessage> pulledMessages = host->pullReceivedMidiMessages();
        receivedMidiMessages.reserve(pulledMessages.size());
        for (const auto& message : pulledMessages) {
            receivedMidiMessages.append(message);
        }
    }

    return receivedMidiMessages;
}

void MainControllerStandalone::startMidiClock() const
{
    midiDriver->sendClockStart();
}

void MainControllerStandalone::stopMidiClock() const
{
    midiDriver->sendClockStop();
}

void MainControllerStandalone::continueMidiClock() const
{
    midiDriver->sendClockContinue();
}

void MainControllerStandalone::sendMidiClockPulse() const
{
    midiDriver->sendClockPulse();
}

QVector<midi::MidiMessage> MainControllerStandalone::pullMidiMessagesFromDevices()
{
    if (!midiDriver) {
        return QVector<midi::MidiMessage>();
    }
    auto result = midiDriver->getBuffer();
    return QVector<midi::MidiMessage>(result.begin(), result.end());
}

bool MainControllerStandalone::isUsingNullAudioDriver() const
{
    return qobject_cast<audio::NullAudioDriver *>(audioDriver.data()) != nullptr;
}

void MainControllerStandalone::stop()
{
    MainController::stop();

    if (audioDriver) {

        //disconnect(this->audioDriver.data(), );

        this->audioDriver->release();
    }

    if (midiDriver)
        this->midiDriver->release();

    qCDebug(jtCore) << "audio and midi drivers released";
}

void MainControllerStandalone::useNullAudioDriver()
{
    qCWarning(jtCore) << "Audio driver can't be used, using NullAudioDriver!";
    audioDriver.reset(new audio::NullAudioDriver());
}

void MainControllerStandalone::updateInputTracksRange()
{
    emit audioController->postEnumInputs([&](const QSharedPointer<audio::LocalInputNode>& inputTrack) {
        switch (inputTrack->getInputMode()) {
        case audio::LocalInputMode::AUDIO:   // audio track
        {
            auto inputTrackRange = inputTrack->getAudioInputProps().getChannelRange();
            // If global input range is reduced to 2 channels and user previous selected inputs 3+4 the input range need be corrected to avoid a beautiful crash :)
            int globalInputs = audioDriver->getInputsCount();
            if (inputTrackRange.getFirstChannel() >= globalInputs) {
                if (globalInputs >= inputTrackRange.getChannels()) {   // we have enough channels?
                    if (inputTrackRange.isMono())
                        setInputTrackToMono(inputTrack, 0);
                    else
                        setInputTrackToStereo(inputTrack, 0);
                }
                else {
                    setInputTrackToNoInput(inputTrack);
                }
            }
            break;
        }
        case audio::LocalInputMode::MIDI:   // midi track
        {
            int selectedDevice = inputTrack->getMidiInputProps().getDevice();
            bool deviceIsValid = selectedDevice >= 0 &&
                    selectedDevice < midiDriver->getMaxInputDevices() &&
                    midiDriver->inputDeviceIsGloballyEnabled(selectedDevice);
            if (!deviceIsValid) {
                // try another available midi input device
                int firstAvailableDevice = midiDriver->getFirstGloballyEnableInputDevice();
                if (firstAvailableDevice >= 0) {
                    audio::MidiInputProps midiInputProps;
                    midiInputProps.setDevice(firstAvailableDevice);
                    midiInputProps.setChannel(-1);  // select all channels
                    setInputTrackToMIDI(inputTrack, midiInputProps);
                } else {
                    setInputTrackToNoInput(inputTrack);
                }
            }
            break;
        }
        default:
            // nothing
            break;
        }
        return true; // continue next input
    });
}

float MainControllerStandalone::getSampleRate() const
{
    if (audioDriver)
        return audioDriver->getSampleRate();

    return 44100;
}
