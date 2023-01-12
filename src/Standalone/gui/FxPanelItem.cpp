#include "FxPanelItem.h"
#include "gui/plugins/Guis.h"
#include "audio/core/Plugins.h"
#include "MainController.h"
#include "gui/LocalTrackView.h"
#include "audio/core/PluginDescriptor.h"
#include "LocalTrackViewStandalone.h"

#include <QDebug>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QAction>
#include <QMouseEvent>
#include <QMenu>
#include <QScreen>
#include <QStyle>

using controller::MainControllerStandalone;

static const int FX_MENU_ACTION_MOVE_UP = 1;
static const int FX_MENU_ACTION_MOVE_DOWN = 2;
static const int FX_MENU_ACTION_BYPASS = 3;
static const int FX_MENU_ACTION_REMOVE = 4;

FxPanelItem::FxPanelItem(LocalTrackViewStandalone *parent, MainControllerStandalone *mainController) :
    QFrame(parent),
    plugin(nullptr),
    bypassButton(new QPushButton(this)),
    label(new QLabel()),
    mainController(mainController),
    localTrackView(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(on_contextMenu(QPoint)));

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(2);

    layout->addWidget(this->label, 1);
    layout->addWidget(this->bypassButton);

    this->bypassButton->setVisible(false);
    this->bypassButton->setCheckable(true);
    this->bypassButton->setChecked(true);

    QObject::connect(this->bypassButton, SIGNAL(clicked()), this, SLOT(on_buttonClicked()));
}

bool FxPanelItem::pluginIsBypassed()
{
    return containPlugin() && plugin->isBypassed();
}

void FxPanelItem::on_buttonClicked()
{
    if (plugin) {
        this->plugin->setBypass(!this->bypassButton->isChecked());
        updateStyleSheet();
    }
}

FxPanelItem::~FxPanelItem()
{
}

void FxPanelItem::updateStyleSheet()
{
    style()->unpolish(this);
    style()->polish(this);
    update();
}

bool FxPanelItem::setPlugin(const QSharedPointer<audio::Plugin>& plugin)
{
    if (plugin && !this->plugin) {
        attachPlugin(plugin);
        return true;
    }
    return this->plugin == plugin;
}

void FxPanelItem::attachPlugin(const QSharedPointer<audio::Plugin> &plugin)
{
    assert(plugin);
    assert(!this->plugin);
    this->plugin = plugin;
    this->label->setText(plugin->getName());
    this->bypassButton->setVisible(true);
    this->bypassButton->setChecked(!plugin->isBypassed());
    updateStyleSheet();
}

QSharedPointer<audio::Plugin> FxPanelItem::detachPlugin()
{
    QSharedPointer<audio::Plugin> detachedPlugin;
    qSwap(this->plugin, detachedPlugin);
    this->label->setText("");
    this->bypassButton->setVisible(false);
    updateStyleSheet();
    return detachedPlugin;
}

void FxPanelItem::removePlugin()
{
    auto detachedPlugin = detachPlugin();
    if (detachedPlugin) {
        detachedPlugin->closeEditor();
        mainController->removePlugin(localTrackView->getInputIndex(), detachedPlugin);
    }
}

void FxPanelItem::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled())
        return;

    if (event->button() == Qt::LeftButton) {
        if (!containPlugin()) {
            on_contextMenu(event->pos());
        } else {
            if (plugin)
                showPluginGui(plugin.get());
        }
    }
}

void FxPanelItem::enterEvent(QEvent *)
{
    if (!isEnabled())
        return;

    if (!containPlugin())
        label->setText(tr("new effect..."));
}

void FxPanelItem::leaveEvent(QEvent *)
{
    if (!isEnabled())
        return;

    if (!containPlugin())
        label->setText("");
}

void FxPanelItem::showPluginsListMenu(const QPoint &p)
{
    QMenu menu;

    QList<PluginDescriptor::Category> categories;
    categories << PluginDescriptor::VST_Plugin;
#ifdef Q_OS_MAC
    categories << PluginDescriptor::AU_Plugin;
#endif
    //categories << PluginDescriptor::Native_Plugin; // native plugins are not implemented yet

    for (PluginDescriptor::Category category : qAsConst(categories)) { // category = VST, NATIVE, AU

        QMenu *categoryMenu = &menu;
        if (categories.size() > 1) {
            QString categoryName = audio::PluginDescriptor::categoryToString(category);
            categoryMenu = new QMenu(categoryName);
            menu.addMenu(categoryMenu);
        }

        auto plugins = mainController->getPluginsDescriptors(category);

        for (auto manufacturerIt = plugins.begin(); manufacturerIt != plugins.end(); ++manufacturerIt) {
            const auto& manufacturerName = manufacturerIt.key();
            const auto& manufacturerPlugins = manufacturerIt.value();
            QMenu *parentMenu = categoryMenu;
            // when the manufacturer has only one plugin this plugin is showed in the Root menu
            if (!manufacturerName.isEmpty() && manufacturerPlugins.size() > 1) {
                parentMenu = new QMenu(manufacturerName);
                categoryMenu->addMenu(parentMenu);
            }
            for (const auto &pluginDescriptor : manufacturerPlugins) {
                QAction *action = parentMenu->addAction(pluginDescriptor.getName(), this, SLOT(loadSelectedPlugin()));
                action->setData(pluginDescriptor.toString());
            }
        }
    }

    menu.move(mapToGlobal(p));
    menu.exec();
}

void FxPanelItem::on_contextMenu(QPoint p)
{
    if (!containPlugin()) { // show plugins list
        showPluginsListMenu(p);
    } else { // show actions list if contain a plugin
        QMenu menu(plugin->getName(), this);
        menu.connect(&menu, SIGNAL(triggered(QAction*)), this, SLOT(on_actionMenuTriggered(QAction*)));
        qint32 pluginIndex = localTrackView->getPluginSlotIndex(plugin);
        auto moveUpItem = menu.addAction(tr("move up"));
        moveUpItem->setData(FX_MENU_ACTION_MOVE_UP);
        moveUpItem->setEnabled(pluginIndex > 0);
        auto moveDownItem = menu.addAction(tr("move down"));
        moveDownItem->setData(FX_MENU_ACTION_MOVE_DOWN);
        moveDownItem->setEnabled(pluginIndex < 4 - 1);
        menu.addAction(tr("bypass"))->setData(FX_MENU_ACTION_BYPASS);
        menu.addAction(tr("remove"))->setData(FX_MENU_ACTION_REMOVE);
        menu.move(mapToGlobal(p));
        menu.exec();
    }
}

void FxPanelItem::loadSelectedPlugin()
{
    auto action = qobject_cast<QAction *>(QObject::sender());

    if (!action || action->data().toString().isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // force the cursor change

    // add a new plugin
    auto descriptor = audio::PluginDescriptor::fromString(action->data().toString());
    qint32 pluginSlotIndex = localTrackView->getPluginSlotIndex(nullptr); // search empty
    if (pluginSlotIndex >= 0) { // valid plugin slot (-1 will be returned when no free plugin slots are available)
        quint32 trackIndex = localTrackView->getInputIndex();
        auto plugin = mainController->addPlugin(trackIndex, pluginSlotIndex, descriptor);
        if (plugin) {
            localTrackView->addPlugin(plugin, pluginSlotIndex);
            showPluginGui(plugin.get());

            // if newProcessor is the first added processor, and is a virtual instrument (VSTi), and the subchannel is 'no input' then change the input selection to midi
            if (pluginSlotIndex == 0 && plugin->isVirtualInstrument()) {
                if (localTrackView->isNoInput()) {
                    localTrackView->setToMidi(); // select the first midi device, all channels
                }
            }
        }
        else {
            qCritical() << "Can´t instantiate the plugin " << descriptor.getName() << " -> " << descriptor.getPath();
        }
    }
    QApplication::restoreOverrideCursor();
}

void FxPanelItem::on_actionMenuTriggered(QAction *action)
{
    if (containPlugin()) {
        int actionIndex = action->data().toInt();
        switch (actionIndex) {
        case FX_MENU_ACTION_MOVE_UP: {
            qint32 pluginIndex = localTrackView->getPluginSlotIndex(plugin);
            if (pluginIndex >= 1) {
                localTrackView->swapPlugins(pluginIndex, pluginIndex - 1);
                quint32 trackIndex = localTrackView->getInputIndex();
                mainController->swapPlugins(trackIndex, pluginIndex, pluginIndex - 1);
            }
            break;
        }
        case FX_MENU_ACTION_MOVE_DOWN: {
            qint32 pluginIndex = localTrackView->getPluginSlotIndex(plugin);
            if (pluginIndex >= 0 && pluginIndex < 4 - 1) {
                localTrackView->swapPlugins(pluginIndex, pluginIndex + 1);
                quint32 trackIndex = localTrackView->getInputIndex();
                mainController->swapPlugins(trackIndex, pluginIndex, pluginIndex + 1);
            }
            break;
        }
        case FX_MENU_ACTION_BYPASS:
            bypassButton->click();
            break;
        case FX_MENU_ACTION_REMOVE:
            removePlugin(); // set this->plugin to nullptr AND remove from mainController
            break;
        }
    }
}

void FxPanelItem::showPluginGui(audio::Plugin *plugin)
{
    if (plugin) {
        plugin->openEditor(screen()->geometry().center());
    }
}
