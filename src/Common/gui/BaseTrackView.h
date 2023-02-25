#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include "audio/core/AudioPeak.h"
#include "audio/core/AudioNode.h"
#include "widgets/Slider.h"

#include <QFrame>
#include <QGridLayout>
#include <QToolButton>

class AudioMeter;
class QLabel;
class QPushButton;
class QGroupBox;
class QSpacerItem;
class QVBoxLayout;
class QHBoxLayout;
class QBoxLayout;
class QGridLayout;
class BoostSpinBox;

namespace audio {
class AudioNode;
}

class BaseTrackView : public QFrame
{
    Q_OBJECT

public:
    explicit BaseTrackView(const QSharedPointer<audio::AudioNode>& trackNode);
    virtual ~BaseTrackView();

    enum Boost {
        ZERO, MINUS, PLUS
    };

    virtual void setToNarrow();
    virtual void setToWide();

    virtual void updateStyleSheet();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    virtual void updateGuiElements();

    virtual void setActivatedStatus(bool deactivated);

    QSharedPointer<audio::AudioNode> getTrack() const;

    template<class T>
    QSharedPointer<T> getTrack() const;

    int getTrackID() const;

    bool isActivated() const;

    virtual void setTintColor(const QColor &color);
    QColor getTintColor() const;

    static const int NARROW_WIDTH;
    static const int WIDE_WIDTH;

protected:

    void changeEvent(QEvent *e) override;

    virtual void translateUI();

    QWeakPointer<audio::AudioNode> trackNode;

    bool activated;
    bool narrowed;

    virtual void setPeaks(float peakLeft, float peakRight, float rmsLeft, float rmsRight);

    // this is called in inherited classes [LocalTrackView, NinjamTrackView]
    void bindThisViewWithTrackNodeSignals(audio::AudioNode* inputNode);

    void createLayoutStructure();

    virtual QPoint getDbValuePosition(const QString &dbValueText, const QFontMetrics &metrics) const;

    // level slider
    AudioSlider *levelSlider;

    // pan slider
    PanSlider *panSlider;
    QLabel *labelPanL;
    QLabel *labelPanR;
    QHBoxLayout *panWidgetsLayout;

    // mute solo
    QPushButton *muteButton;
    QPushButton *soloButton;
    QBoxLayout *muteSoloLayout;

    // boost
    BoostSpinBox *boostSpinBox;

    // main layout buildind blocks
    QGridLayout *mainLayout;
    QBoxLayout *secondaryChildsLayout; // right side widgets in vertical layout, bottom widgets (2nd row) in horizontal layout

    virtual void setupVerticalLayout();

    static const uint FADER_HEIGHT;

    QColor tintColor;

private:
    audio::AudioPeak audioPeak;

protected slots:
    virtual void toggleMuteStatus(bool enabled);
    virtual void toggleSoloStatus(bool enabled);
    virtual void setGain(int value);
    virtual void setPan(int value);
    virtual void updateBoostValue(int boostValue);
    virtual void updateAudioPeak(const audio::AudioPeak& audioPeak);

private slots:
    // signals emitted by AudioNode class when user activate the control with mouse, or midi CCs, or using joystick, etc.
    void setPanKnobPosition(float newPanValue, void* sender = nullptr);
    void setGainSliderPosition(float newGainValue, void* sender = nullptr);
    void setMuteStatus(bool newMuteStatus);
    void setSoloStatus(bool newSoloStatus);
    void setBoostStatus(float newBoostValue, void* sender = nullptr);
};

inline QColor BaseTrackView::getTintColor() const
{
    return tintColor;
}

template<class T>
inline QSharedPointer<T> BaseTrackView::getTrack() const
{
    return getTrack().dynamicCast<T>();
}

#endif // TRACKVIEW_H
