#include "NinjamTrackView.h"

#include <QLineEdit>
#include <QLabel>
#include <QDebug>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QSlider>
#include <QStyle>
#include <QDateTime>
#include <QMenu>
#include <QWidgetAction>
#include <QPainter>

#include "BaseTrackView.h"
#include "MainController.h"
#include "Utils.h"
#include "IconFactory.h"
#include "audio/NinjamTrackNode.h"
#include "NinjamController.h"
#include "widgets/BoostSpinBox.h"
#include "widgets/PeakMeter.h"
#include "widgets/InstrumentsMenu.h"
#include "IconFactory.h"

const int NinjamTrackView::WIDE_HEIGHT = 70; // height used in horizontal layout for wide tracks

quint32 NinjamTrackView::networkUsageUpdatePeriod = 4000;

using controller::MainController;
using persistence::CacheEntry;

NinjamTrackView::NinjamTrackView(controller::MainController* mainController,
                                 const QSharedPointer<NinjamTrackNode>& trackNode,
                                 const QSharedPointer<persistence::UsersDataCache>& userDataCache) :
    BaseTrackView(trackNode),
    mainController(mainController),
    userDataCache(userDataCache),
    lowCutState(trackNode->getLowCutState()),
    orientation(Qt::Vertical),
    downloadingFirstInterval(true),
    lastNetworkUsageUpdate(0)
{

    chunksDisplay = new IntervalChunksDisplay(this);
    chunksDisplay->setVisible(false);

    buttonLowCut = createLowCutButton(false);
    updateLowCutButtonToolTip();

    buttonReceive = createReceiveButton();

    networkUsageLabel = new QLabel();
    networkUsageLabel->setObjectName("receiveLabel");
    networkUsageLabel->setAlignment(Qt::AlignCenter);

    networkUsageLayout = new QHBoxLayout();
    networkUsageLayout->setContentsMargins(0, 0, 0, 0);
    networkUsageLayout->setSpacing(2);
    networkUsageLayout->addWidget(buttonReceive);
    networkUsageLayout->addWidget(networkUsageLabel);
    secondaryChildsLayout->addLayout(networkUsageLayout);
    secondaryChildsLayout->setAlignment(networkUsageLayout, Qt::AlignCenter);

    connect(buttonReceive, &QPushButton::toggled, this, &NinjamTrackView::setReceiveState);

    instrumentsButton = createInstrumentsButton();
    connect(instrumentsButton, &InstrumentsButton::iconChanged, this, &NinjamTrackView::instrumentIconChanged);

    setupVerticalLayout();

    BaseTrackView::setActivatedStatus(true); // disabled/grayed until receive the first bytes.

    voiceChatIcon = IconFactory::createVoiceChatIcon();

    buttonLowCut->setState(static_cast<quint8>(lowCutState));

    connect(trackNode.data(), &NinjamTrackNode::xmitStateChanged, this, &NinjamTrackView::xmitStateChanged);
    connect(trackNode.data(), &NinjamTrackNode::lowCutStateChanged, this, &NinjamTrackView::lowCutStateChanged);
}

void NinjamTrackView::setPeaks(float peakLeft, float peakRight, float rmsLeft, float rmsRight)
{
    BaseTrackView::setPeaks(peakLeft, peakRight, rmsLeft, rmsRight);

    static const float AUTO_MUTE_THRESHOLD = 1; // 0 dB RMS
    static const float AUTO_BOOST_THRESHOLD = 0.707946; // -3 dB RMS

    if (rmsLeft >= AUTO_BOOST_THRESHOLD || rmsRight >= AUTO_BOOST_THRESHOLD) {
        boostSpinBox->setToMin(); // auto lower boost gain by -12db  when rms peaks are high
    }
    if (rmsLeft >= AUTO_MUTE_THRESHOLD || rmsRight >= AUTO_MUTE_THRESHOLD) {
        muteButton->click(); // auto mute when rms peaks are very high
    }
}

void NinjamTrackView::paintEvent(QPaintEvent *ev)
{
    BaseTrackView::paintEvent(ev);

    auto trackNode = getTrackNode();
    if (trackNode && trackNode->isVoiceChat()) {

        if (updateCounter % 10 == 0) // blinking voice chat icon every 10 updates
            return;

        auto isVertical = orientation == Qt::Vertical;

        auto scaleSize = isVertical ? (muteButton->width()) : 1;

        auto icon = isVertical ? voiceChatIcon.scaledToWidth(scaleSize, Qt::SmoothTransformation)
                               : voiceChatIcon.scaledToHeight(height(), Qt::SmoothTransformation);

        auto x = isVertical ? (width() - icon.width() - mainLayout->contentsMargins().right()) : 0;
        auto y = isVertical ? (muteButton->y() - icon.height() - 6) : 0;

        QPainter painter(this);
        painter.drawPixmap(x, y, icon);
    }
}

void NinjamTrackView::instrumentIconChanged(quint8 instrumentIndex)
{
    cacheEntry.setInstrumentIndex(instrumentIndex);
    updateUserCacheEntry();
}

void NinjamTrackView::setTintColor(const QColor &color)
{
    BaseTrackView::setTintColor(color);

    buttonReceive->setIcon(IconFactory::createReceiveIcon(color));
    buttonLowCut->setIcons(IconFactory::createLowCutIcons(color));
}

void NinjamTrackView::setNinjamChannelData(const QString &userFullName, quint8 channelIndex)
{
    this->userFullName = userFullName;
    this->channelIndex = channelIndex;
}

void NinjamTrackView::setReceiveState(bool receive)
{
    setActivatedStatus(!receive);

    // send a message to ninjam server disabling channel receive status
    mainController->setChannelReceiveStatus(userFullName, channelIndex, receive);
}

QSharedPointer<NinjamTrackNode> NinjamTrackView::getTrackNode() const
{
    return trackNode.lock().dynamicCast<NinjamTrackNode>();
}

QPushButton *NinjamTrackView::createReceiveButton() const
{
    QPushButton *button = new QPushButton();
    button->setIcon(IconFactory::createReceiveIcon(getTintColor()));
    button->setToolTip(tr("Receive"));
    button->setObjectName(QStringLiteral("receiveButton"));
    button->setCheckable(true);
    button->setChecked(true); // receiving by default
    button->setEnabled(false); // disabled until receive the first interval
    return button;
}

MultiStateButton *NinjamTrackView::createLowCutButton(bool checked)
{
    auto icons = IconFactory::createLowCutIcons(getTintColor());
    MultiStateButton *button = new MultiStateButton(this, icons); // 3 states: Low cut OFF, NORMAL, and DRASTIC
    button->setCheckable(true);
    button->setChecked(checked);
    secondaryChildsLayout->addWidget(button, 0, Qt::AlignCenter);
    button->setObjectName("lowCutButton");
    connect(button, &QPushButton::clicked, this, &NinjamTrackView::setLowCutToNextState);
    return button;
}

void NinjamTrackView::updateLowCutButtonToolTip()
{
    Q_ASSERT(buttonLowCut);

    QString toolTipText = tr("Low cut") + " [" + getLowCutStateText() + "]";
    buttonLowCut->setToolTip(toolTipText);
}

QString NinjamTrackView::getLowCutStateText() const
{
    switch (lowCutState) {
    case LowCutState::Off:
        return tr("Off");
    case LowCutState::Normal:
        return tr("Normal");
    case LowCutState::Drastic:
        return tr("Drastic");
    default:
        return tr("Off"); // just to be shure
    }
}

void NinjamTrackView::updateStyleSheet()
{
    style()->unpolish(buttonLowCut);
    style()->polish(buttonLowCut);

    style()->unpolish(buttonReceive);
    style()->polish(buttonReceive);

    BaseTrackView::updateStyleSheet();
}

void NinjamTrackView::setInitialValues(const persistence::CacheEntry &initialValues)
{
    cacheEntry = initialValues;

    const auto& settings = mainController->getSettings().rememberSettings;

    if (settings.isRememberingLevel())
        levelSlider->setValue(initialValues.getGain() * 100);

    if (settings.isRememberingPan())
        panSlider->setValue(initialValues.getPan() * panSlider->maximum());

    if (settings.isRememberingMute() && initialValues.isMuted())
        muteButton->click();

    if (settings.isRememberingBoost()) {
        if (initialValues.getBoost() < 1.0) {
            boostSpinBox->setToMin(); // -12 dB
        } else {
            if (initialValues.getBoost() > 1.0) // +12 dB
                boostSpinBox->setToMax();
            else
                boostSpinBox->setToOff();
        }
    }

    if (settings.isRememberingLowCut()) {
        lowCutState = initialValues.getLowCutState();
        buttonLowCut->setState(static_cast<quint8>(lowCutState));
    }

    auto instrumentIconIndex = initialValues.hasValidInstrumentIndex() ? initialValues.getInstrumentIndex() : guessInstrumentIcon();
    instrumentsButton->setInstrumentIcon(instrumentIconIndex);

}

void NinjamTrackView::setChannelMode(NinjamTrackNode::ChannelMode mode)
{
    auto trackNode = getTrackNode();
    if (trackNode) {
        trackNode->scheduleSetChannelMode(mode);
        update();
    }
}

bool NinjamTrackView::isVideoChannel() const
{
    return instrumentsButton->getSelectedIcon() == static_cast<int>(InstrumentIndex::Video);
}

qint8 NinjamTrackView::guessInstrumentIcon() const
{
    auto channelName = instrumentsButton->toolTip();

    return static_cast<qint8>(stringToInstrumentIndex(channelName));
}

void NinjamTrackView::updateGuiElements()
{
    auto trackNode = getTrackNode();

    if (isActivated()) {
        BaseTrackView::updateGuiElements();


        if (trackNode)
            levelSlider->setStereo(trackNode->isStereo());
    }

    // update network usage label
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ellapsedTime = now - lastNetworkUsageUpdate;
    if (ellapsedTime >= NinjamTrackView::networkUsageUpdatePeriod) {
        lastNetworkUsageUpdate = now;

        auto bytesDownloaded = mainController->getDownloadTransferRate(userFullName, channelIndex);
        long downloadTransferRate = (bytesDownloaded > 0) ? (bytesDownloaded / 1024 * 8) : 0;
        networkUsageLabel->setText(QString::number(downloadTransferRate).leftJustified(3, QChar(' ')));

        QString toolTipText = QString("%1 %2 Kbps").arg(tr("Downloading")).arg(downloadTransferRate);
        if (trackNode) {
            toolTipText += QString(" (%1, %2 KHz)")
                    .arg(trackNode->isStereo() ? tr("Stereo") : tr("Mono"),
                         QString::number(trackNode->getDecoderSampleRate()/1000.0, 'f', 1));
        }

        networkUsageLabel->setToolTip(toolTipText);
    }

    if (trackNode && trackNode->isVoiceChat()) {
        updateCounter++; // used to blink the voice chat icon
        update();
    }
}

void NinjamTrackView::setNetworkUsageUpdatePeriod(quint32 periodInMilliseconds)
{
    NinjamTrackView::networkUsageUpdatePeriod = periodInMilliseconds;
}

void NinjamTrackView::setActivatedStatus(bool deactivated)
{
    BaseTrackView::setActivatedStatus(deactivated);

    if (deactivated) { // remote user stop xmiting and the track is greyed/unlighted?
        downloadingFirstInterval = true; // waiting for the first interval
        chunksDisplay->reset();
    }

    // stop rendering downloaded audio
    auto trackNode = getTrackNode();
    if (trackNode) {
        emit trackNode->postReceiveState(!deactivated);
        emit trackNode->postResetLastPeak(); // reset the internal node last peak to avoid getting the last peak calculated when the remote user was transmiting.
    }
}

QSize NinjamTrackView::sizeHint() const
{
    if (orientation == Qt::Horizontal) {
        if (narrowed)
            return QWidget::sizeHint();
        else
            return QSize(BaseTrackView::sizeHint().width(), WIDE_HEIGHT);
    }
    return BaseTrackView::sizeHint();
}

void NinjamTrackView::setOrientation(Qt::Orientation newOrientation)
{
    if (this->orientation == newOrientation)
        return;

    this->orientation = newOrientation;
    if (newOrientation == Qt::Horizontal)
        setupHorizontalLayout();
    else
        setupVerticalLayout();

    setProperty("horizontal", newOrientation == Qt::Horizontal ? true : false);
    updateStyleSheet();

    updateExtraWidgetsVisibility();
}

void NinjamTrackView::setupVerticalLayout()
{
    BaseTrackView::setupVerticalLayout();

    mainLayout->removeItem(secondaryChildsLayout);
    mainLayout->removeWidget(chunksDisplay);
    mainLayout->removeItem(panWidgetsLayout);
    mainLayout->removeWidget(levelSlider);
    mainLayout->removeWidget(instrumentsButton);

    // reset collumn stretch
    for (int c = 0; c < mainLayout->columnCount(); ++c) {
        mainLayout->setColumnStretch(c, 0);
    }

    auto columnCount = mainLayout->columnCount();

    mainLayout->addWidget(instrumentsButton, 0, 0, 1, columnCount, Qt::AlignCenter);
    mainLayout->addLayout(panWidgetsLayout, 1, 0, 1, columnCount);
    mainLayout->addWidget(levelSlider, 2, 0);
    mainLayout->addLayout(secondaryChildsLayout, 2, 1, 1, columnCount - 1, Qt::AlignBottom);
    mainLayout->addWidget(chunksDisplay, 3, 0, 1, columnCount); // append chunks display in bottom

    secondaryChildsLayout->setDirection(QBoxLayout::TopToBottom);

    boostSpinBox->setOrientation(Qt::Vertical);

    networkUsageLayout->setDirection(QBoxLayout::TopToBottom);
}

void NinjamTrackView::setupHorizontalLayout()
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    mainLayout->removeWidget(levelSlider);
    mainLayout->removeItem(panWidgetsLayout);
    mainLayout->removeItem(secondaryChildsLayout);
    mainLayout->removeWidget(chunksDisplay);
    mainLayout->removeWidget(instrumentsButton);

    auto rowCount = mainLayout->rowCount();

    mainLayout->addWidget(instrumentsButton, 0, 0, rowCount, 1, Qt::AlignCenter);
    mainLayout->addLayout(panWidgetsLayout, 0, 1, 1, 1);
    mainLayout->addWidget(levelSlider, 0, 2);
    mainLayout->addWidget(chunksDisplay, 1, 1, rowCount-1, 1);
    mainLayout->addLayout(secondaryChildsLayout, 1, 2, rowCount-1, 1, Qt::AlignRight | Qt::AlignBottom);

    mainLayout->setColumnStretch(0, 0); // instrument button
    mainLayout->setColumnStretch(1, 1); // pan slider
    mainLayout->setColumnStretch(2, 3); // level slider

    auto vMargin = narrowed ? 3 : 6;
    mainLayout->setContentsMargins(vMargin, 3, vMargin, 3);
    mainLayout->setVerticalSpacing(vMargin);

    secondaryChildsLayout->setDirection(QBoxLayout::RightToLeft);

    levelSlider->setOrientation(Qt::Horizontal);
    levelSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    panSlider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    muteSoloLayout->setDirection(QHBoxLayout::RightToLeft);

    boostSpinBox->setOrientation(Qt::Horizontal);

    networkUsageLayout->setDirection(QBoxLayout::RightToLeft);
}

QPoint NinjamTrackView::getDbValuePosition(const QString &dbValueText,
                                           const QFontMetrics &metrics) const
{
    if (orientation == Qt::Vertical)
        return BaseTrackView::getDbValuePosition(dbValueText, metrics);

    float sliderPosition = (double)levelSlider->value()/levelSlider->maximum();
    int textX = sliderPosition * levelSlider->width() + levelSlider->x()
                - metrics.width(dbValueText) - FADER_HEIGHT;
    int textY = levelSlider->y() + levelSlider->height() + 3;
    return QPoint(textX, textY);
}

void NinjamTrackView::finishCurrentDownload()
{
     if(downloadingFirstInterval){
        chunksDisplay->finish();
        downloadingFirstInterval = false;
        setDownloadedChunksDisplayVisibility(false);
     }
}

void NinjamTrackView::incrementDownloadedChunks()
{
    if (downloadingFirstInterval){
        chunksDisplay->incrementDownloadedChunks();

        if (!chunksDisplay->isVisible())
            setDownloadedChunksDisplayVisibility(true);

        if (!buttonReceive->isEnabled())
            buttonReceive->setEnabled(true);
    }
}

void NinjamTrackView::setEstimatedChunksPerInterval(int estimatedChunks)
{
    chunksDisplay->setEstimatedTotalChunks(estimatedChunks);
}

void NinjamTrackView::setDownloadedChunksDisplayVisibility(bool visible)
{
    chunksDisplay->reset();
    chunksDisplay->setVisible(visible);
}

void NinjamTrackView::setChannelName(const QString &name)
{
    instrumentsButton->setToolTip(name);

    instrumentsButton->setInstrumentIcon(guessInstrumentIcon());
}

void NinjamTrackView::updateUserCacheEntry()
{
    auto userDataCache = this->userDataCache.lock();
    if (userDataCache) {
        userDataCache->updateUserCacheEntry(cacheEntry);
    }
}

void NinjamTrackView::setPan(int value)
{
    BaseTrackView::setPan(value);
    cacheEntry.setPan(value / (float)panSlider->maximum());
    updateUserCacheEntry();
}

void NinjamTrackView::setGain(int value)
{
    BaseTrackView::setGain(value);
    cacheEntry.setGain(value/100.0);
    updateUserCacheEntry();
}

void NinjamTrackView::toggleMuteStatus(bool enabled)
{
    BaseTrackView::toggleMuteStatus(enabled);
    cacheEntry.setMuted(enabled);
    updateUserCacheEntry();
}

void NinjamTrackView::updateBoostValue(int index)
{
    BaseTrackView::updateBoostValue(index);
    cacheEntry.setBoost(Utils::dbToLinear(index));
    updateUserCacheEntry();
}

void NinjamTrackView::xmitStateChanged(bool transmiting)
{
    setActivatedStatus(!transmiting);
}

void NinjamTrackView::lowCutStateChanged(LowCutState newState)
{
    lowCutState = newState;
    cacheEntry.setLowCutState(newState);
    updateUserCacheEntry();
    updateLowCutButtonToolTip();
    buttonLowCut->style()->unpolish(this);
    buttonLowCut->style()->polish(this);
}

void NinjamTrackView::setLowCutToNextState()
{
    auto node = getTrackNode();
    if (node) {
        emit node->postNextLowCutState();
    }
}

InstrumentsButton *NinjamTrackView::createInstrumentsButton()
{
    auto defaultIcon = IconFactory::getDefaultInstrumentIcon();
    auto icons = IconFactory::getInstrumentIcons();

    return new InstrumentsButton(defaultIcon, icons, this);
}

void NinjamTrackView::resizeEvent(QResizeEvent *ev)
{
    BaseTrackView::resizeEvent(ev);

    updateExtraWidgetsVisibility();
}

void NinjamTrackView::updateExtraWidgetsVisibility()
{
    bool showExtraWidgets = orientation == Qt::Horizontal || height() >= 260; // hide channel name label and network usage if the height is small (VST/AU plugin small window with camera activated);

    mainLayout->setVerticalSpacing(showExtraWidgets ? 6 : 3);

    networkUsageLabel->setVisible(showExtraWidgets);
    buttonReceive->setVisible(showExtraWidgets);
    instrumentsButton->setVisible(showExtraWidgets);
    buttonLowCut->setVisible(showExtraWidgets);
}
