#include "common.h"


WsException::WsException(const QString &str)
    :reason_stored(str),
      charbuf(reason_stored.toLocal8Bit()){}

const QString WsException::reason() const
{
    return reason_stored;
}

const char *WsException::what() const noexcept
{
    return charbuf.data();
}
