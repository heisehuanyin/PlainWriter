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
}


class ForeshadowConfig : public QDialog
{
public:
    //                                      title,   keypath, modelindex
    ForeshadowConfig(const QList<QPair<QString, QModelIndex> > &keystoryList, QWidget *parent=nullptr);

    QDialog::DialogCode getForeshadowDescription(QModelIndex &index, QString &name, QString &desp0, QString &desp1);

private:
    const QList<QPair<QString, QModelIndex> > &keystory_list;
    QComboBox *const combox;
    QLineEdit *const name_enter;
    QTextEdit *const desp0, *const desp1;
};

#endif // COMMON_H
