#include "FxPanel.h"
#include "FxPanelItem.h"
#include "LocalTrackViewStandalone.h"
#include "audio/core/Plugins.h"
#include "audio/core/AudioNode.h"

#include <QVBoxLayout>
#include <QPainter>

FxPanel::FxPanel(LocalTrackViewStandalone *parent, MainControllerStandalone *mainController) :
    QWidget(parent),
    controller(mainController),
    localTrackView(parent)
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(QMargins(0, 0, 0, 0));
    mainLayout->setSpacing(1);

    for (int i = 0; i < audio::AudioNode::MAX_PROCESSORS_PER_TRACK; i++) {
        auto item = new FxPanelItem(localTrackView, mainController);
        items.append(item);
        mainLayout->addWidget(item);
    }
}

void FxPanel::removePlugins()
{
    auto items = findChildren<FxPanelItem *>();
    for (auto item : qAsConst(items)) {
        if (item->containPlugin()) {
            item->removePlugin();
        }
    }
}

qint32 FxPanel::getPluginSlotIndex(const QSharedPointer<Plugin> &plugin) const
{
    auto items = findChildren<FxPanelItem *>();
    int slotIndex = 0;
    for (auto item : qAsConst(items)) {
        if (item->getAudioPlugin() == plugin) {
            return slotIndex;
        }
        slotIndex++;
    }
    return -1; // not found
}

void FxPanel::addPlugin(const QSharedPointer<Plugin> &plugin, quint32 pluginSlotIndex)
{
    assert(pluginSlotIndex < audio::AudioNode::MAX_PROCESSORS_PER_TRACK);
    auto items = findChildren<FxPanelItem *>();
    if (pluginSlotIndex < (quint32)items.count()) {
        auto fxPanelItem = items.at(pluginSlotIndex);
        if (!fxPanelItem->containPlugin())
            fxPanelItem->setPlugin(plugin);
        else
            qCritical() << "Can't add " << plugin->getName() << " in slot index " << pluginSlotIndex;
    }
}

void FxPanel::swapPlugins(quint32 firstSlotIndex, quint32 secondSlotIndex)
{
    assert(firstSlotIndex < audio::AudioNode::MAX_PROCESSORS_PER_TRACK);
    assert(secondSlotIndex < audio::AudioNode::MAX_PROCESSORS_PER_TRACK);
    if (firstSlotIndex != secondSlotIndex) {
        auto items = findChildren<FxPanelItem *>();
        auto firstPanelItem = items.at(firstSlotIndex);
        auto secondPanelItem = items.at(secondSlotIndex);
        auto firstPlugin = firstPanelItem->detachPlugin();
        auto secondPlugin = secondPanelItem->detachPlugin();
        if (firstPlugin) {
            secondPanelItem->attachPlugin(firstPlugin);
        }
        if (secondPlugin) {
            firstPanelItem->attachPlugin(secondPlugin);
        }
    }
}

FxPanel::~FxPanel()
{
    // delete ui;
}


