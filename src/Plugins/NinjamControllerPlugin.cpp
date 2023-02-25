#include "NinjamControllerPlugin.h"
#include "MainControllerPlugin.h"
#include "controller/AudioController.h"
#include "audio/MetronomeTrackNode.h"
#include "audio/NinjamTrackNode.h"

NinjamControllerPlugin::NinjamControllerPlugin(MainControllerPlugin *controller) :
    NinjamController(controller),
    controller(controller),
    waitingForHostSync(false)
{
    //
}

void NinjamControllerPlugin::stopAndWaitForHostSync()
{
    if (!waitingForHostSync) {
        waitingForHostSync = true;

        reset(); // discard the intervals but keep the most recent

        deactivateAudioNodes(); // metronome and ninjam audio nodes will not be rendered
    }
}

void NinjamControllerPlugin::deactivateAudioNodes()
{
    emit metronomeTrackNode->postSetActivated(false);
    {
        QMutexLocker locker(&mutex);
        for (const auto& node : qAsConst(trackNodes)) {
            emit node->postSetActivated(false);
        }
    }
    emit mainController->getAudioController()->postSetAllLoopersStatus(false); // deactivate all loopers
}

void NinjamControllerPlugin::activateAudioNodes()
{
    emit metronomeTrackNode->postSetActivated(true);
    {
        QMutexLocker locker(&mutex);
        for (const auto& node : qAsConst(trackNodes)) {
            emit node->postSetActivated(true);
        }
    }
    emit mainController->getAudioController()->postSetAllLoopersStatus(true); // activate all loopers
}

void NinjamControllerPlugin::disableHostSync()
{
    waitingForHostSync = false;
    activateAudioNodes();
}

void NinjamControllerPlugin::startSynchronizedWithHost(qint32 startPosition)
{
    if (waitingForHostSync){
        waitingForHostSync = false;
        if (startPosition >= 0)
            intervalPosition = startPosition % samplesInInterval;
        else
            intervalPosition = samplesInInterval - qAbs(startPosition % samplesInInterval);

        activateAudioNodes();
    }
}

void NinjamControllerPlugin::process(const QSharedPointer<audio::SamplesBuffer>& in, const QSharedPointer<audio::SamplesBuffer>& out)
{
    if (!waitingForHostSync)
        NinjamController::process(in, out);
    else
        controller->process(in, out); // will process input only, ninjam related audio nodes will not be rendered, interval will not progress, etc.
}
