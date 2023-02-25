#ifndef STANDALONEMAINCONTROLLER_H
#define STANDALONEMAINCONTROLLER_H

#include "MainController.h"
#include "audio/core/SamplesBuffer.h"
#include <QApplication>
#include <QSharedPointer>
#include <QFutureInterface>

#ifdef Q_OS_MAC
    #include "AU/AudioUnitPluginFinder.h"
#endif

class QCoreApplication;

class MainWindowStandalone;
class Host;

namespace midi
{
    class MidiDriver;
}

namespace ninjam
{
    namespace client
    {
        class Server;
    }
}

namespace audio
{
    class VSTPluginFinder;
    class Plugin;
    class AudioDriver;
    class PluginDescriptor;
    class MidiInputProps;
}

using audio::VSTPluginFinder;
using audio::Plugin;
using audio::AudioDriver;
using audio::PluginDescriptor;

namespace controller
{
    class MainControllerStandalone : public MainController
    {
        Q_OBJECT

    public:
        MainControllerStandalone(persistence::Settings settings, QApplication *application);
        ~MainControllerStandalone() override;

        void initializeVstPluginsList(const QStringList &paths);

#ifdef Q_OS_MAC
        void initializeAudioUnitPluginsList(const QStringList &paths);
        void scanAudioUnitPlugins();
#endif

        void addDefaultPluginsScanPath(); // add vst path from registry

        void clearPluginsList();

        void clearPluginsCache();
        QStringList getSteinbergRecommendedPaths();
        bool vstScanIsNeeded() const; // plugins cache is empty OR we have new plugins in scan folders?

        void quit();

        void start() override;
        void stop() override;

        void updateInputTracksRange(); // called when input range or method (audio or midi) are changed in preferences

        float getSampleRate() const override;

        inline AudioDriver *getAudioDriver() const
        {
            return audioDriver.data();
        }

        inline midi::MidiDriver *getMidiDriver() const
        {
            return midiDriver.data();
        }

        void stopNinjamController() override;

        QString getJamtabaFlavor() const override;

        void setInputTrackToMono(const QSharedPointer<audio::LocalInputNode>& inputTrack, int inputIndexInAudioDevice);
        void setInputTrackToStereo(const QSharedPointer<audio::LocalInputNode>& inputTrack, int firstInputIndex);
        void setInputTrackToMIDI(const QSharedPointer<audio::LocalInputNode>& inputTrack, const audio::MidiInputProps& midiInpuProps);
        void setInputTrackToNoInput(const QSharedPointer<audio::LocalInputNode>& inputTrack);

        bool isUsingNullAudioDriver() const;

        void useNullAudioDriver(); // use when the audio driver fails

        void setMainWindow(MainWindow *mainWindow) override;

        void cancelPluginFinders();

        inline VSTPluginFinder *getVstPluginFinder() const
        {
            return vstPluginFinder.data();
        }

        QMap<QString, QList<PluginDescriptor> > getPluginsDescriptors(
            PluginDescriptor::Category category);

        QVector<midi::MidiMessage> pullMidiMessagesFromPlugins() override;

        QSharedPointer<Plugin> createPluginInstance(const PluginDescriptor &descriptor);

    public slots:
        void startMidiClock() const override;
        void stopMidiClock() const override;
        void continueMidiClock() const override;
        void sendMidiClockPulse() const override;

        void setSampleRate(int newSampleRate) override;
        void setBufferSize(int newBufferSize);

        void removePluginsScanPath(const QString &path);
        void addPluginsScanPath(const QString &path);

        void addBlackVstToSettings(const QString &path);
        void removeBlackVstFromSettings(const QString &pluginPath);

        void scanAllVstPlugins();
        void scanOnlyNewVstPlugins();

        void openExternalAudioControlPanel();

        void connectInNinjamServer(const ninjam::client::ServerInfo &server) override;

    protected:
        midi::MidiDriver *createMidiDriver();

        QSharedPointer<audio::AudioDriver> createAudioDriver();

        controller::NinjamController *createNinjamController() override;

        void setCSS(const QString &css) override;

        void setupNinjamControllerSignals() override;
        void clearNinjamControllerSignals() override;

        QVector<midi::MidiMessage> pullMidiMessagesFromDevices() override;

    protected slots:
        void updateBpm(int newBpm) override;

        void handleNewNinjamInterval() override;

        // TODO After the big refatoration these 3 slots can be private slots
        void on_audioDriverStopped();
        void on_audioDriverStarted();
        void on_ninjamStartProcessing(int intervalPosition);

        void addFoundedVstPlugin(const QString &name, const QString &path);
#ifdef Q_OS_MAC
        void addFoundedAudioUnitPlugin(const QString &name, const QString &path);
#endif

    private slots:
        void setVstPluginWindowSize(QString pluginName, int newWidht, int newHeight);

    private:
        // VST and AU hosts
        QList<Host *> hosts;
        QApplication *application;

        QSharedPointer<AudioDriver> audioDriver;
        QScopedPointer<midi::MidiDriver> midiDriver;

        QList<PluginDescriptor> pluginsDescriptors;

        MainWindowStandalone *window;

        bool inputIndexIsValid(int inputIndex);

        QScopedPointer<VSTPluginFinder> vstPluginFinder;
#ifdef Q_OS_MAC
        QScopedPointer<audio::AudioUnitPluginFinder> auPluginFinder;
 #endif

        // used to sort plugins list
        static bool pluginDescriptorLessThan(const PluginDescriptor &d1,
                                             const PluginDescriptor &d2);

        void scanVstPlugins(bool scanOnlyNewVstPlugins);
    };
} // namespace

#endif // STANDALONEMAINCONTROLLER_H
