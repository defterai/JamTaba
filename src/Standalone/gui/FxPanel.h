#ifndef FXPANEL_H
#define FXPANEL_H

#include <QWidget>
//#include "MainControllerStandalone.h"

class FxPanelItem;
class LocalTrackViewStandalone;

namespace controller {
class MainControllerStandalone;
}

namespace audio {
class Plugin;
}

using controller::MainControllerStandalone;
using audio::Plugin;

class FxPanel : public QWidget
{
    Q_OBJECT

public:
    FxPanel(LocalTrackViewStandalone *parent, MainControllerStandalone *mainController);

    virtual ~FxPanel();

    void addPlugin(const QSharedPointer<Plugin> &plugin, quint32 pluginSlotIndex);
    void swapPlugins(quint32 firstSlotIndex, quint32 secondSlotIndex);
    qint32 getPluginSlotIndex(const QSharedPointer<Plugin> &plugin) const;
    void removePlugins();

    inline LocalTrackViewStandalone *getLocalTrackView() const
    {
        return localTrackView;
    }

    inline QList<FxPanelItem *> getItems() const
    {
        return items;
    }

private:
    QList<FxPanelItem *> items;
    controller::MainControllerStandalone *controller; // storing a 'casted' controller for convenience
    LocalTrackViewStandalone *localTrackView;
};

#endif // FXPANEL_H
