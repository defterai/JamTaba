#include "PrivateServerDialog.h"
#include "ui_PrivateServerDialog.h"
#include "persistence/PrivateServerSettings.h"
#include <QDebug>

PrivateServerDialog::PrivateServerDialog(QWidget *parent, const persistence::PrivateServerSettings& settings, const QString& userName) :
    QDialog(parent),
    ui(new Ui::PrivateServerDialog)
{
    ui->setupUi(this);

    ui->okButton->setAutoDefault(true);
    connect(ui->okButton, &QPushButton::clicked, this, &PrivateServerDialog::accept);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags((windowFlags() | Qt::WindowMinMaxButtonsHint) & ~Qt::WindowContextHelpButtonHint);

    buildComboBoxItems(settings);

    // setting initial values
    ui->textFieldPassword->setText(settings.getLastPassword());
    ui->textFieldPort->setText(QString::number(settings.getLastPort()));
    ui->textFieldUserName->setText(userName);

    ui->textFieldUserName->forceCenterAlignment(false);
    ui->textFieldUserName->setAlignment(Qt::AlignLeft);

    ui->textFieldPassword->setEchoMode(QLineEdit::Password);

    ui->textFieldPassword->setAttribute(Qt::WA_MacShowFocusRect, 0); // remove focus border in Mac
    ui->textFieldPort->setAttribute(Qt::WA_MacShowFocusRect, 0); // remove focus border in Mac
}

void PrivateServerDialog::buildComboBoxItems(const persistence::PrivateServerSettings& settings)
{
    ui->comboBoxServers->clear();
    const QList<QString>& servers = settings.getLastServers();
    for (const QString &server : qAsConst(servers)) {
        ui->comboBoxServers->addItem(server, server);
    }
}

void PrivateServerDialog::accept()
{
    QDialog::accept();
    QString serverName = ui->comboBoxServers->currentText();
    int serverPort = ui->textFieldPort->text().toInt();
    QString serverPassword = ui->textFieldPassword->text();
    QString userName = ui->textFieldUserName->text();

    emit connectionAccepted(serverName, serverPort, userName, serverPassword);
}

QString PrivateServerDialog::getPassword() const
{
    return ui->textFieldPassword->text();
}

QString PrivateServerDialog::getServer() const
{
    return ui->comboBoxServers->currentText();
}

PrivateServerDialog::~PrivateServerDialog()
{
    delete ui;
}
