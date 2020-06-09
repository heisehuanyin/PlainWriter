#ifndef COMMON_H
#define COMMON_H

#include <QString>
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

#endif // COMMON_H
