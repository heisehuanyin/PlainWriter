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

#define WsExcept(ex) \
    try{ \
        ex; \
    }\
    catch(NovelBase::WsException *e){\
        qDebug() << e->reason();\
    }


#endif // COMMON_H
