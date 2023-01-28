#ifndef LOCAL_TRACK_VIEW_H
#define LOCAL_TRACK_VIEW_H

#include "BaseTrackView.h"
#include "gui/widgets/PeakMeter.h"

#include <QPushButton>

class FxPanel;

namespace audio {
class LocalInputNode;
}

class LooperWindow;

class LocalTrackView : public BaseTrackView
{
    Q_OBJECT

public:
    explicit LocalTrackView(const QSharedPointer<audio::LocalInputNode>& trackNode);

    void setInitialValues(float initialGain, int initialBoost, float initialPan, bool muted, bool stereoInverted);

    virtual ~LocalTrackView();
    virtual void updateGuiElements() override;

    void enableLopperButton(bool enabled);

    void updateStyleSheet() override;

    void closeAllPlugins();

    int getInputIndex() const;

    QSharedPointer<audio::LocalInputNode> getInputNode() const;

    virtual void setActivatedStatus(bool unlighted) override;

    virtual void setPeakMetersOnlyMode(bool peakMetersOnly);
    void togglePeakMetersOnlyMode();

    bool isShowingPeakMetersOnly() const;

    QSize sizeHint() const override;

    virtual void reset();

    void setTintColor(const QColor &color) override;

signals:
    void openLooperEditor(int trackIndex);

protected:
    QPushButton *buttonStereoInversion;
    QPushButton *buttonLooper;

    bool peakMetersOnly;

    virtual void setupMetersLayout();

    void bindThisViewWithTrackNodeSignals();

    void translateUI() override;

private:
    QPushButton *createStereoInversionButton();
    QPushButton *createLooperButton();

    bool inputIsUsedByThisTrack(int inputIndexInAudioDevice) const;
    void deleteWidget(QWidget *widget);

    class LooperIconFactory;

    static LooperIconFactory looperIconFactory;

private slots:
    void setStereoInversion(bool stereoInverted);
    void updateLooperButtonIcon();

};


inline bool LocalTrackView::isShowingPeakMetersOnly() const
{
    return peakMetersOnly;
}

#endif
