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

ForeshadowConfig::ForeshadowConfig(const QList<QPair<QString, QModelIndex> > &keystoryList, QWidget *parent)
    :QDialog (parent), keystory_list(keystoryList), combox(new QComboBox(this)),
      name_enter(new QLineEdit(this)), desp0(new QTextEdit(this)), desp1(new QTextEdit(this))
{
    auto layout = new QGridLayout(this);

    for (auto pak : keystory_list) {
        combox->addItem(pak.first, pak.second);
    }
    layout->addWidget(combox, 0, 0, 1, 2);
    layout->addWidget(name_enter, 1, 0, 1, 2);
    layout->addWidget(desp0, 2, 0, 2, 2);
    layout->addWidget(desp1, 4, 0, 2, 2);

    auto okbtn = new QPushButton("确定", this);
    auto cancelbtn = new QPushButton("取消", this);
    layout->addWidget(okbtn, 6, 0);
    layout->addWidget(cancelbtn, 6, 1);
    if(!keystoryList.size())
        okbtn->setEnabled(false);

    connect(okbtn,      &QPushButton::clicked,  this,   &QDialog::accept);
    connect(cancelbtn,  &QPushButton::clicked,  this,   &QDialog::reject);
}

QDialog::DialogCode ForeshadowConfig::getForeshadowDescription(QModelIndex &index, QString &name, QString &desp0, QString &desp1)
{
    auto x = exec();

    index = combox->currentData().toModelIndex();
    name = name_enter->text();
    desp0 = this->desp0->toPlainText();
    desp1 = this->desp1->toPlainText();

    return static_cast<QDialog::DialogCode>(x);
}
