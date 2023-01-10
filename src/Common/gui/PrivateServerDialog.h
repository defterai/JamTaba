#ifndef PRIVATESERVERDIALOG_H
#define PRIVATESERVERDIALOG_H

#include <QDialog>

namespace Ui {
class PrivateServerDialog;
}

namespace persistence {
class PrivateServerSettings;
}

class PrivateServerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrivateServerDialog(QWidget *parent, const persistence::PrivateServerSettings& settings, const QString& userName);
    ~PrivateServerDialog();
    QString getServer() const;
    QString getPassword() const;
    int getServerPort() const;

public slots:
    void accept() override;

signals:
    void connectionAccepted(const QString &server, quint16 serverPort, const QString &userName, const QString &password);

private:
    Ui::PrivateServerDialog *ui;

    void buildComboBoxItems(const persistence::PrivateServerSettings& settings);
};

#endif // PRIVATESERVERDIALOG_H
