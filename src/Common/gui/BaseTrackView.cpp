#include "BaseTrackView.h"

#include "Utils.h"
#include "audio/core/AudioPeak.h"
#include "audio/core/AudioNode.h"
#include "widgets/BoostSpinBox.h"
#include "widgets/PeakMeter.h"
#include "IconFactory.h"

#include <QStyleOption>
#include <QPainter>
#include <QDebug>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QEvent>

const uint BaseTrackView::FADER_HEIGHT = 12;

const int BaseTrackView::NARROW_WIDTH = 85;
const int BaseTrackView::WIDE_WIDTH = 120;

using audio::AudioNode;

// -----------------------------------------------------------------------------------------

BaseTrackView::BaseTrackView(const QSharedPointer<AudioNode>& trackNode) :
    trackNode(trackNode),
    activated(true),
    narrowed(false),
    tintColor(Qt::black)
{
    createLayoutStructure();
    setupVerticalLayout();

    setMuteStatus(trackNode->isMuted());
    setSoloStatus(trackNode->isSoloed());
    setGainSliderPosition(trackNode->getGain());
    setPanKnobPosition(trackNode->getPan());
    setBoostStatus(trackNode->getBoost());

    connect(muteButton, &QPushButton::clicked, this, &BaseTrackView::toggleMuteStatus);
    connect(soloButton, &QPushButton::clicked, this, &BaseTrackView::toggleSoloStatus);
    connect(levelSlider, &QSlider::valueChanged, this, &BaseTrackView::setGain);
    connect(panSlider, &QSlider::valueChanged, this, &BaseTrackView::setPan);
    connect(boostSpinBox, &BoostSpinBox::boostChanged, this, &BaseTrackView::updateBoostValue);
    connect(trackNode.data(), &AudioNode::audioPeakChanged, this, &BaseTrackView::updateAudioPeak);
}

void BaseTrackView::setTintColor(const QColor &color)
{
    this->tintColor = color;
}

void BaseTrackView::setupVerticalLayout()
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    levelSlider->setOrientation(Qt::Vertical);
    levelSlider->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    muteSoloLayout->setDirection(QBoxLayout::TopToBottom);

    mainLayout->setVerticalSpacing(9);
    mainLayout->setContentsMargins(3, 0, 3, 3);
    mainLayout->setColumnStretch(0, 1);
}

void BaseTrackView::createLayoutStructure()
{
    setObjectName(QStringLiteral("BaseTrackView"));

    mainLayout = new QGridLayout(this);

    panWidgetsLayout = new QHBoxLayout();
    panWidgetsLayout->setSpacing(0);
    panWidgetsLayout->setContentsMargins(0, 0, 0, 0);

    labelPanL = new QLabel();
    labelPanL->setObjectName(QStringLiteral("labelPanL"));
    labelPanL->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    labelPanL->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

    labelPanR = new QLabel();
    labelPanR->setObjectName(QStringLiteral("labelPanR"));
    labelPanR->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    labelPanR->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

    panSlider = new PanSlider();
    panSlider->setObjectName(QStringLiteral("panSlider"));
    panSlider->setMinimum(-4);
    panSlider->setMaximum(4);
    panSlider->setOrientation(Qt::Horizontal);

    panWidgetsLayout->addWidget(labelPanL);
    panWidgetsLayout->addWidget(panSlider);
    panWidgetsLayout->addWidget(labelPanR);

    levelSlider = new AudioSlider();
    levelSlider->setObjectName(QStringLiteral("levelSlider"));
    levelSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    levelSlider->setValue(100);
    levelSlider->setTickPosition(QSlider::NoTicks);

    muteButton = new QPushButton();
    muteButton->setObjectName(QStringLiteral("muteButton"));
    muteButton->setEnabled(true);
    muteButton->setLayoutDirection(Qt::LeftToRight);
    muteButton->setCheckable(true);

    soloButton = new QPushButton();
    soloButton->setObjectName(QStringLiteral("soloButton"));
    soloButton->setLayoutDirection(Qt::LeftToRight);
    soloButton->setCheckable(true);

    muteSoloLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    muteSoloLayout->setSpacing(1);
    muteSoloLayout->setContentsMargins(0, 0, 0, 0);
    muteSoloLayout->addWidget(muteButton, 0, Qt::AlignCenter);
    muteSoloLayout->addWidget(soloButton, 0, Qt::AlignCenter);

    boostSpinBox = new BoostSpinBox(this);

    secondaryChildsLayout = new QVBoxLayout();
    secondaryChildsLayout->setSpacing(6);
    secondaryChildsLayout->setContentsMargins(0, 0, 0, 0);

    secondaryChildsLayout->addLayout(muteSoloLayout);
    secondaryChildsLayout->addWidget(boostSpinBox, 0, Qt::AlignCenter);

    mainLayout->addLayout(panWidgetsLayout, 0, 0, 1, 2);
    mainLayout->addWidget(levelSlider, 1, 0);
    mainLayout->addLayout(secondaryChildsLayout, 1, 1, 1, 1, Qt::AlignBottom);

    translateUI();
}

void BaseTrackView::translateUI()
{
    labelPanL->setText(tr("L"));
    labelPanR->setText(tr("R"));

    muteButton->setText(tr("M"));
    soloButton->setText(tr("S"));

    boostSpinBox->updateToolTip();
}

void BaseTrackView::bindThisViewWithTrackNodeSignals(audio::AudioNode* inputNode)
{
    connect(inputNode, &AudioNode::gainChanged, this, &BaseTrackView::setGainSliderPosition);
    connect(inputNode, &AudioNode::panChanged, this, &BaseTrackView::setPanKnobPosition);
    connect(inputNode, &AudioNode::muteChanged, this, &BaseTrackView::setMuteStatus);
    connect(inputNode, &AudioNode::soloChanged, this, &BaseTrackView::setSoloStatus);
    connect(inputNode, &AudioNode::boostChanged, this, &BaseTrackView::setBoostStatus);
}

// ++++++  signals emitted by Audio Node +++++++
/*
    The values are changed in the model, so the view (this class) need
react and update. This changes in the model can done in initialization (when
Jamtaba is opened and the last gain, pan values, etc. are loaded) or by
a midi message for example. So we can't expect the gain and pan values are
changed only by user mouse interaction, these values can be changed using another
methods (like midi messages).

*/

void BaseTrackView::setBoostStatus(float newBoostValue, void* sender)
{
    if (sender != this) {
        if (newBoostValue > 1.0) // boost value is a gain multiplier, 1.0 means 0 dB boost (boost OFF)
            boostSpinBox->setToMax();
        else if (newBoostValue < 1.0)
            boostSpinBox->setToMin();
        else
            boostSpinBox->setToOff(); // 0 dB - OFF
    }
}

void BaseTrackView::setGainSliderPosition(float newGainValue, void* sender)
{
    if (sender != this) {
        levelSlider->setValue(Utils::poweredGainToLinear(newGainValue) * 100);
        update(); // repaint to update the Db value
    }
}

void BaseTrackView::setPanKnobPosition(float newPanValue, void* sender)
{
    if (sender != this) {
        // pan range is[-4,4], zero is center
        panSlider->setValue(newPanValue * 4);
    }
}

void BaseTrackView::setMuteStatus(bool newMuteStatus)
{
    muteButton->setChecked(newMuteStatus);
}

void BaseTrackView::setSoloStatus(bool newSoloStatus)
{
    soloButton->setChecked(newSoloStatus);
}

void BaseTrackView::updateBoostValue(int boostValue)
{
    auto trackNode = this->trackNode.lock();
    if (trackNode) {
        emit trackNode->postBoost(Utils::dbToLinear(boostValue), this);
    }
}

void BaseTrackView::updateAudioPeak(const audio::AudioPeak& audioPeak)
{
    this->audioPeak = audioPeak;
}

void BaseTrackView::updateGuiElements()
{
    // update the track peaks
    setPeaks(audioPeak.getLeftPeak(), audioPeak.getRightPeak(),
             audioPeak.getLeftRMS(), audioPeak.getRightRMS());
}

QSize BaseTrackView::sizeHint() const
{
    auto hint = QFrame::sizeHint();

    if (narrowed)
        return QSize(NARROW_WIDTH, hint.height());

    return QSize(WIDE_WIDTH, hint.height());
}

QSize BaseTrackView::minimumSizeHint() const
{
    return sizeHint();
}

void BaseTrackView::setToNarrow()
{
    if (!this->narrowed) {
        this->narrowed = true;
        updateGeometry();
    }
}

void BaseTrackView::setToWide()
{
    if (narrowed) {
        this->narrowed = false;
        updateGeometry();
    }
}

void BaseTrackView::updateStyleSheet()
{
    style()->unpolish(this);
    style()->polish(this);

    style()->unpolish(levelSlider);
    style()->polish(levelSlider);
    levelSlider->updateStyleSheet();

    style()->unpolish(panSlider);
    style()->polish(panSlider);

    style()->unpolish(muteButton);
    style()->polish(muteButton);

    style()->unpolish(soloButton);
    style()->polish(soloButton);

    boostSpinBox->updateStyleSheet();

    update();
}

void BaseTrackView::setActivatedStatus(bool deactivated)
{
    setProperty("unlighted", QVariant(deactivated));
    this->activated = !deactivated;
    updateStyleSheet();
}

QSharedPointer<audio::AudioNode> BaseTrackView::getTrack() const
{
    return this->trackNode.lock();
}

int BaseTrackView::getTrackID() const
{
    auto trackNode = this->trackNode.lock();
    return trackNode ? trackNode->getID() : -1;
}

bool BaseTrackView::isActivated() const
{
    return activated;
}

void BaseTrackView::setPeaks(float peakLeft, float peakRight, float rmsLeft, float rmsRight)
{
    levelSlider->setPeak(peakLeft, peakRight, rmsLeft, rmsRight);
}

BaseTrackView::~BaseTrackView()
{

}

void BaseTrackView::setPan(int value)
{
    auto trackNode = this->trackNode.lock();
    if (trackNode) {
        float sliderValue = value / (float)panSlider->maximum();
        emit trackNode->postPan(sliderValue, this);
    }
}

void BaseTrackView::setGain(int value)
{
    // signals are blocked [the third parameter] to avoid a loop in signal/slot scheme.
    auto trackNode = this->trackNode.lock();
    if (trackNode) {
        emit trackNode->postGain(Utils::linearGainToPower(value / 100.0), this);
    }
}

void BaseTrackView::toggleMuteStatus(bool enabled)
{
    auto trackNode = this->trackNode.lock();
    if (trackNode) {
        emit trackNode->postMute(enabled, this);
    }
}

void BaseTrackView::toggleSoloStatus(bool enabled)
{
    auto trackNode = this->trackNode.lock();
    if (trackNode) {
        emit trackNode->postSolo(enabled, this);
    }
}

QPoint BaseTrackView::getDbValuePosition(const QString &dbValueText,
                                         const QFontMetrics &fontMetrics) const
{
    int textWidth = fontMetrics.width(dbValueText);
    int textX = levelSlider->x() + levelSlider->width()/2 - textWidth - ((!narrowed) ? 14 : 11);
    float sliderPosition = (double)levelSlider->value()/levelSlider->maximum();
    int offset = levelSlider->y() + fontMetrics.height();
    int textY = (1 - sliderPosition) * levelSlider->height() + offset;
    return QPoint(textX, textY);
}

void BaseTrackView::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        translateUI();
    }
    QWidget::changeEvent(e);
}
