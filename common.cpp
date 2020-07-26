#include "common.h"

#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace NovelBase;

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

DescriptionIn::DescriptionIn(const QString &title, QWidget *parent)
    :QDialog (parent), name_enter(new QLineEdit(this)), desp0(new QTextEdit(this))
{
    setWindowTitle(title);
    auto layout = new QGridLayout(this);

    layout->addWidget(name_enter, 0, 0, 1, 2);
    layout->addWidget(desp0, 1, 0, 2, 2);

    auto okbtn = new QPushButton("确定", this);
    auto cancelbtn = new QPushButton("取消", this);
    layout->addWidget(okbtn, 3, 0);
    layout->addWidget(cancelbtn, 3, 1);

    connect(okbtn,      &QPushButton::clicked,  this,   &QDialog::accept);
    connect(cancelbtn,  &QPushButton::clicked,  this,   &QDialog::reject);
}

QDialog::DialogCode DescriptionIn::getDetailsDescription(QString &name, QString &desp)
{
    name_enter->setPlaceholderText(name);
    this->desp0->setPlaceholderText(desp);
    auto x = exec();

    name = name_enter->text();
    desp = this->desp0->toPlainText();

    return static_cast<QDialog::DialogCode>(x);
}
