#include "common.h"


WsException::WsException(const QString &msg):msg(msg){}

QString WsException::why() const{
    return msg;
}
