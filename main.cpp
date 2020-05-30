#include "confighost.h"
#include "mainframe.h"
#include "novelhost.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QtDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle("Fusion");

    start:
    auto opt = QMessageBox::information(nullptr, "打开已有小说？", "“确定”打开已有小说，“否定”新建空白小说，“取消”关闭软件!",
                             QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes);

    if(opt == QMessageBox::Cancel)
        return 0;


    ConfigHost host;
    NovelHost novel(host);
    StructDescription one;

    if(opt == QMessageBox::Yes){
        QString path = QFileDialog::getOpenFileName(nullptr, "选择打开的小说描述文件", QDir::homePath(),
                                                    "NovelStruct(*.nml)",nullptr, QFileDialog::DontResolveSymlinks);

        if(path == "") goto start;

        QString err;
        int code;
        if((code = one.openDescription(err, path))){
            QMessageBox::critical(nullptr, "打开过程错误", err);
            qDebug() << err << path;
            goto start;
        }
    }
    else if (opt == QMessageBox::No) {
        one.newDescription();
    }


    QString err;
    int code;
    if((code = novel.loadDescription(err, &one))){
        QMessageBox::critical(nullptr, "加载过程出错", err);
        goto start;
    }

    MainFrame w(&novel);
    w.show();

    return a.exec();
}
