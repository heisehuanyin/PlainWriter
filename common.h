#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <exception>


class WsException : public std::exception
{
public:
    explicit WsException(const QString &msg);
    virtual ~WsException() override = default;

    virtual QString why() const;

private:
    const QString &msg;

};

#endif // COMMON_H
