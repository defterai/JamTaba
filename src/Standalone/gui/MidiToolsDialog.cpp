#include "MidiToolsDialog.h"
#include "ui_MidiToolsDialog.h"
#include "persistence/LocalInputTrackSettings.h"
#include <QRegularExpressionValidator>

#include "LocalTrackViewStandalone.h"

static QString getMidiNoteText(quint8 midiNoteNumber)
{
    int octave = midiNoteNumber / 12;
    QString noteName = "";
    switch (midiNoteNumber % 12) {
        case 0: noteName = QStringLiteral("C"); break;
        case 1: noteName = QStringLiteral("C#"); break;
        case 2: noteName = QStringLiteral("D"); break;
        case 3: noteName = QStringLiteral("D#"); break;
        case 4: noteName = QStringLiteral("E"); break;
        case 5: noteName = QStringLiteral("F"); break;
        case 6: noteName = QStringLiteral("F#"); break;
        case 7: noteName = QStringLiteral("G"); break;
        case 8: noteName = QStringLiteral("G#"); break;
        case 9: noteName = QStringLiteral("A"); break;
        case 10: noteName = QStringLiteral("A#"); break;
        case 11: noteName = QStringLiteral("B"); break;
    }
    return noteName + QString::number(octave);
}

static int getMidiNoteNumber(const QString &midiNote)
{
    if (midiNote.length() < 2) {
        return -1;
    }
    int noteNumber = 0;
    switch (midiNote.at(0).toLatin1()) {
    case 'c':
    case 'C':
        noteNumber = 0;
        break;
    case 'd':
    case 'D':
        noteNumber = 2;
        break;
    case 'e':
    case 'E':
        noteNumber = 4;
        break;
    case 'f':
    case 'F':
        noteNumber = 5;
        break;
    case 'g':
    case 'G':
        noteNumber = 7;
        break;
    case 'a':
    case 'A':
        noteNumber = 9;
        break;
    case 'b':
    case 'B':
        noteNumber = 11;
        break;
    default:
        return -1;
    }
    int octaveOffset = 1;
    char secondChar = midiNote.at(1).toLatin1();
    if (secondChar == 'b') {
        noteNumber--;
        octaveOffset++;
    } else if (secondChar == '#') {
        noteNumber++;
        octaveOffset++;
    }
    if (octaveOffset >= midiNote.length()) {
        return -1;
    }
    bool success = true;
    uint octave = midiNote.rightRef(midiNote.length() - octaveOffset).toUInt(&success);
    if (!success) {
        return -1;
    }
    return (octave * 12) + noteNumber;
}

MidiToolsDialog::MidiToolsDialog(quint8 lowerNote, quint8 higherNote, qint8 transpose, bool routingMidiInput) :
    QDialog(nullptr),
    ui(new Ui::MidiToolsDialog)
{
    ui->setupUi(this);
    setWindowFlags((windowFlags() | Qt::WindowMinimizeButtonHint) & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    QRegularExpression regex("^[A-G][#b]?[1]?[0-9]");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regex);

    ui->textFieldHigherNote->setValidator(validator);
    ui->textFieldLowerNote->setValidator(validator);
    ui->textFieldLowerNote->setText(getMidiNoteText(lowerNote));
    ui->textFieldHigherNote->setText(getMidiNoteText(higherNote));

    connect(ui->textFieldHigherNote, SIGNAL(editingFinished()), this, SLOT(higherNoteEditionFinished()));
    connect(ui->textFieldLowerNote, SIGNAL(editingFinished()), this, SLOT(lowerNoteEditionFinished()));
    connect(ui->buttonLearnLowerNote, SIGNAL(toggled(bool)), this, SLOT(learnLowerMidiNoteToggled(bool)));
    connect(ui->buttonLearnHigherNote, SIGNAL(toggled(bool)), this, SLOT(learnHigherMidiNoteToggled(bool)));

    ui->spinBoxTranspose->setMinimum(persistence::MIN_MIDI_TRANSPOSE);
    ui->spinBoxTranspose->setMaximum(persistence::MAX_MIDI_TRANSPOSE);
    ui->spinBoxTranspose->setValue(transpose);
    connect(ui->spinBoxTranspose, SIGNAL(valueChanged(int)), SLOT(transposeValueChanged(int)));

    ui->checkBoxMidiRouting->setChecked(routingMidiInput);
    connect(ui->checkBoxMidiRouting, &QCheckBox::toggled, this, &MidiToolsDialog::midiRoutingCheckBoxClicked);
}

void MidiToolsDialog::hideMidiRoutingControls()
{
    ui->checkBoxMidiRouting->hide();
}

MidiToolsDialog::~MidiToolsDialog()
{
    delete ui;
}

void MidiToolsDialog::learnLowerMidiNoteToggled(bool buttonChecked)
{
    ui->textFieldLowerNote->setReadOnly(buttonChecked);
    if (buttonChecked){
        ui->buttonLearnHigherNote->setChecked(false);
    }
    emit learnMidiNoteClicked(buttonChecked);
}

void MidiToolsDialog::learnHigherMidiNoteToggled(bool buttonChecked)
{
    ui->textFieldHigherNote->setReadOnly(buttonChecked);
    if (buttonChecked){
        ui->buttonLearnLowerNote->setChecked(false);
    }
    emit learnMidiNoteClicked(buttonChecked);
}

void MidiToolsDialog::setLearnedMidiNote(quint8 learnedNote)
{
    if (ui->buttonLearnHigherNote->isChecked()) {
        ui->textFieldHigherNote->setText(getMidiNoteText(learnedNote));
        emit higherNoteChanged(learnedNote);
    } else if (ui->buttonLearnLowerNote->isChecked()) {
        ui->textFieldLowerNote->setText(getMidiNoteText(learnedNote));
        emit lowerNoteChanged(learnedNote);
    }
}

void MidiToolsDialog::transposeValueChanged(int transposeValue)
{
    emit transposeChanged((qint8)transposeValue);
}

void MidiToolsDialog::lowerNoteEditionFinished()
{
    int midiNote = getMidiNoteNumber(ui->textFieldLowerNote->text());
    if (midiNote >= 0 && midiNote <= 127) {
        emit lowerNoteChanged((quint8)midiNote);
    } else {
        ui->textFieldLowerNote->setText(getMidiNoteText(0));
    }
}

void MidiToolsDialog::higherNoteEditionFinished()
{
    int midiNote = getMidiNoteNumber(ui->textFieldHigherNote->text());
    if (midiNote >= 0 && midiNote <= 127) {
        emit higherNoteChanged((quint8)midiNote);
    } else {
        ui->textFieldHigherNote->setText(getMidiNoteText(127));
    }
}

void MidiToolsDialog::closeEvent(QCloseEvent *event)
{
    emit dialogClosed();
    event->accept();
}
