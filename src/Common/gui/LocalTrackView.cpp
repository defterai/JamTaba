#include "LocalTrackView.h"
#include "MainController.h"
#include "audio/core/LocalInputNode.h"
#include "audio/core/LocalInputGroup.h"
#include "GuiUtils.h"
#include "widgets/BoostSpinBox.h"
#include "IconFactory.h"

#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QStyle>
#include <QPainter>
#include <QIcon>
#include <QFontMetrics>

class LocalTrackView::LooperIconFactory
{
public:

    explicit LooperIconFactory(const QString &originalIconPath)
        : originalIconPath(originalIconPath)
    {
        //
    }

    QIcon createRecordingIcon() const
    {
        // create recording icon
        QList<QSize> iconSizes = originalIcon.availableSizes();
        if (!iconSizes.isEmpty()) {
            QPixmap recPixmap = originalIcon.pixmap(iconSizes.first());

            QPainter painter(&recPixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QColor(255, 0, 0, 120));
            painter.setPen(Qt::NoPen);
            QRectF rect(recPixmap.rect());
            painter.drawEllipse(rect.marginsAdded(QMarginsF(-2, -5, -2, -2)));
            //painter.drawEllipse(recPixmap.width() - radius, recPixmap.height() - radius, radius, radius);

            return QIcon(recPixmap);
        }

        qCritical() << "icon available sizes is empty!";
        return originalIcon;
    }

    QIcon createCurrentLooperLayerIcon(quint8 currentLayer, const QFontMetricsF &fontMetrics)
    {
        QIcon layerIcon = layersIcons[currentLayer];
        if (layerIcon.isNull()) {
            QList<QSize> sizes = originalIcon.availableSizes();
            if (!sizes.isEmpty()) {
                QPixmap pixmap = originalIcon.pixmap(sizes.first());
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::TextAntialiasing);
                painter.setRenderHint(QPainter::Antialiasing);

                QString text(QString::number(currentLayer + 1));
                qreal rectWidth = fontMetrics.width(text) * 2;

                QRectF textRect(pixmap.width() - rectWidth, pixmap.height() - fontMetrics.height(), rectWidth, fontMetrics.height());

                painter.setBrush(QColor(255, 255, 255, 150));
                painter.setPen(Qt::black);
                painter.drawEllipse(textRect);
                painter.drawText(textRect, text, QTextOption(Qt::AlignCenter));

                layerIcon = QIcon(pixmap);
                layersIcons.insert(currentLayer, layerIcon);
            }
            else {
                qCritical() << "sizes is empty!";
            }
        }

        return layerIcon;
    }

    QIcon getIcon(audio::Looper *looper, const QFontMetricsF &fontMetrics)
    {
        if (originalIcon.isNull()) {
            this->originalIcon = QIcon(originalIconPath);
            this->recordingIcon = createRecordingIcon();
        }

        if (looper) {
            if (looper->isRecording() || looper->isWaitingToRecord()) {
                return recordingIcon;
            }
            else if (looper->isPlaying()) {
                quint8 currentLayer = looper->getCurrentLayerIndex();
                return createCurrentLooperLayerIcon(currentLayer, fontMetrics);
            }
        }

        return originalIcon;
    }

private:
    QString originalIconPath;
    QIcon originalIcon;
    QIcon recordingIcon;
    QMap<quint8, QIcon> layersIcons;
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++

LocalTrackView::LooperIconFactory LocalTrackView::looperIconFactory(":/images/loop.png");

LocalTrackView::LocalTrackView(const QSharedPointer<audio::LocalInputNode>& inputNode) :
    BaseTrackView(inputNode.staticCast<audio::AudioNode>()),
    buttonStereoInversion(createStereoInversionButton()),
    buttonLooper(createLooperButton()),
    peakMetersOnly(false)
{
    buttonStereoInversion->setChecked(inputNode->getAudioInputProps().isStereoInverted());

    bindThisViewWithTrackNodeSignals();// now is secure bind this LocalTrackView with the respective TrackNode model

    setActivatedStatus(false);

    secondaryChildsLayout->addWidget(buttonLooper, 0, Qt::AlignCenter);
    secondaryChildsLayout->addWidget(buttonStereoInversion, 0, Qt::AlignCenter);

    auto looper = inputNode->getLooper();
    connect(looper.data(), &audio::Looper::stateChanged, this, &LocalTrackView::updateLooperButtonIcon);
    connect(looper.data(), &audio::Looper::currentLayerChanged, this, &LocalTrackView::updateLooperButtonIcon);
}

void LocalTrackView::updateLooperButtonIcon()
{
    // get a new icon based in looper state
    auto inputNode = getInputNode();
    if (inputNode) {
        auto looper = inputNode->getLooper();
        QIcon newIcon = looperIconFactory.getIcon(looper.data(), buttonLooper->fontMetrics());
        buttonLooper->setIcon(newIcon);
    }
}

void LocalTrackView::bindThisViewWithTrackNodeSignals()
{
    auto inputNode = getInputNode();
    if (inputNode) {
        BaseTrackView::bindThisViewWithTrackNodeSignals(inputNode.data());
        connect(inputNode.data(), &audio::LocalInputNode::stereoInversionChanged, this, &LocalTrackView::setStereoInversion);
    }
}

void LocalTrackView::setInitialValues(float initialGain, int initialBoost,
                                      float initialPan, bool muted, bool stereoInverted)
{
    auto inputNode = getInputNode();
    if (inputNode) {
        emit inputNode->postGain(Utils::linearGainToPower(initialGain), nullptr);
        emit inputNode->postPan(initialPan, nullptr);
        emit inputNode->postBoost(Utils::dbToLinear(initialBoost), nullptr);
        emit inputNode->postMute(muted, nullptr);
        setStereoInversion(stereoInverted);
    }
}

void LocalTrackView::closeAllPlugins()
{
    auto inputNode = getInputNode();
    if (inputNode) {
        inputNode->closeProcessorsWindows(); // close vst editors
    }
}

QSize LocalTrackView::sizeHint() const
{
    if (peakMetersOnly) {
        return QSize(16, height());
    }
    return BaseTrackView::sizeHint();
}

void LocalTrackView::setupMetersLayout()
{
    mainLayout->addWidget(levelSlider, 1, 0); // put the levelSlider in the original place
}

void LocalTrackView::setPeakMetersOnlyMode(bool peakMetersOnly)
{
    if (this->peakMetersOnly != peakMetersOnly) {
        this->peakMetersOnly = peakMetersOnly;

        gui::setLayoutItemsVisibility(secondaryChildsLayout, !this->peakMetersOnly);

        levelSlider->setShowMeterOnly(peakMetersOnly);

        gui::setLayoutItemsVisibility(panWidgetsLayout, !this->peakMetersOnly);

        if (peakMetersOnly) { // add the peak meters directly in main layout, so these meters are horizontally centered
            mainLayout->addWidget(levelSlider, 0, 0, mainLayout->rowCount(), mainLayout->columnCount());
        }
        else { // put the meter in the original layout
            setupMetersLayout();
        }

        const int spacing = peakMetersOnly ? 0 : 3;

        mainLayout->setHorizontalSpacing(spacing);

        levelSlider->setVisible(true); // peak meter are always visible

        levelSlider->setPaintingDbMarkers(!peakMetersOnly);

        QMargins margins = layout()->contentsMargins();
        margins.setLeft(spacing);
        margins.setRight(spacing);
        layout()->setContentsMargins(margins);

        soloButton->setVisible(!peakMetersOnly);
        muteButton->setVisible(!peakMetersOnly);
        Qt::Alignment alignment = peakMetersOnly ? Qt::AlignRight : Qt::AlignHCenter;
        levelSlider->parentWidget()->layout()->setAlignment(levelSlider, alignment);

        updateGeometry();

        setProperty("peakMetersOnly", QVariant(peakMetersOnly));
        style()->unpolish(this);
        style()->polish(this);
    }
}

void LocalTrackView::togglePeakMetersOnlyMode()
{
    setPeakMetersOnlyMode(!peakMetersOnly);
}

void LocalTrackView::setActivatedStatus(bool unlighted)
{
    BaseTrackView::setActivatedStatus(unlighted);
    update();
}

int LocalTrackView::getInputIndex() const
{
    auto inputNode = getInputNode();
    if (inputNode) {
        return inputNode->getID();
    }
    return -1;
}

QSharedPointer<audio::LocalInputNode> LocalTrackView::getInputNode() const
{
    return this->trackNode.lock().staticCast<audio::LocalInputNode>();
}

void LocalTrackView::reset()
{
    auto inputNode = getInputNode();
    if (inputNode) {
        inputNode->reset();
    }
}

LocalTrackView::~LocalTrackView()
{
}

void LocalTrackView::setTintColor(const QColor &color)
{
    BaseTrackView::setTintColor(color);

    buttonLooper->setIcon(IconFactory::createLooperButtonIcon(color));

    buttonStereoInversion->setIcon(IconFactory::createStereoInversionIcon(color));
}

QPushButton *LocalTrackView::createLooperButton()
{
    QPushButton *button = new QPushButton(IconFactory::createLooperButtonIcon(getTintColor()), "");
    button->setObjectName(QStringLiteral("buttonLooper"));
    button->setEnabled(false); // disabled by default

    connect(button, &QPushButton::clicked, [=]{
        auto inputNode = getInputNode();
        if (inputNode) {
            emit openLooperEditor(inputNode->getID());
        }
    });

    return button;
}

void LocalTrackView::enableLopperButton(bool enabled)
{
    buttonLooper->setEnabled(enabled && !getInputNode()->isRoutingMidiInput());
}

QPushButton *LocalTrackView::createStereoInversionButton()
{
    QPushButton *button = new QPushButton();
    button->setObjectName(QStringLiteral("buttonStereoInversion"));
    button->setCheckable(true);
    connect(button, &QPushButton::clicked, this, &LocalTrackView::setStereoInversion);
    return button;
}

void LocalTrackView::translateUI()
{
    BaseTrackView::translateUI();

    buttonStereoInversion->setToolTip(tr("Invert stereo"));
    buttonLooper->setToolTip(tr("Looper (Available when jamming)"));

}

void LocalTrackView::setStereoInversion(bool stereoInverted)
{
    auto inputNode = getInputNode();
    if (inputNode) {
        inputNode->postSetStereoInversion(stereoInverted, this);
        buttonStereoInversion->setChecked(stereoInverted);
    }
}

void LocalTrackView::updateGuiElements()
{
    BaseTrackView::updateGuiElements();
    auto trackNode = getInputNode();
    if (trackNode) {
        // update the track processors. In this moment the VST plugins GUI are updated.
        // Some plugins need this to run your animations (see Ez Drummer, for example);
        trackNode->updateProcessorsGui();  // call idle in VST plugins
    }
}

void LocalTrackView::updateStyleSheet()
{
    BaseTrackView::updateStyleSheet();

    buttonLooper->setIcon(IconFactory::createLooperButtonIcon(getTintColor()));

    style()->unpolish(buttonStereoInversion); // this is necessary to change the stereo inversion button colors when the transmit button is clicled
    style()->polish(buttonStereoInversion);

    style()->unpolish(buttonLooper);
    style()->polish(buttonLooper);
}
