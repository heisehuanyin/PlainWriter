#include "common.h"


WsException::WsException(const QString &str)
    :reason_stored(str),
      std_reason_stored(reason_stored.toStdString()){}

const QString WsException::reason() const
{
    return reason_stored;
}

const char *WsException::what() const noexcept
{
    return std_reason_stored.c_str();
}
