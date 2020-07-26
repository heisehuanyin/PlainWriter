#ifndef COMMON_H
#define COMMON_H

#include <QComboBox>
#include <QDialog>
#include <QString>
#include <QTextEdit>
#include <exception>

namespace NovelBase {
    class WsException : std::exception
    {
    public:
        WsException(const QString &str);

        const QString reason() const;

        // exception interface
    public:
        virtual const char *what() const noexcept override;


    private:
        const QString reason_stored;
        const QByteArray charbuf;
    };

    class DescriptionIn : public QDialog
    {
    public:
        //                                      title,   keypath, modelindex
        DescriptionIn(const QString &title, QWidget *parent=nullptr);

        QDialog::DialogCode getDetailsDescription(QString &name, QString &desp);

    private:
        QLineEdit *const name_enter;
        QTextEdit *const desp0;
    };

}


#endif // COMMON_H
