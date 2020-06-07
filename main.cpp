#include "common.h"
#include "confighost.h"
#include "mainframe.h"
#include "novelhost.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyleFactory>
#include <QtDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    /*qDebug() << QStyleFactory::keys();*/
    a.setStyle("fusion");

    //config check;
    QDir software_root(QDir::home().filePath(".PlainWriter"));
    if(!software_root.exists()){
        QDir::home().mkdir(".PlainWriter");
    }
    auto keywords_doc = software_root.filePath("keywords.txt");
    auto warrings_doc = software_root.filePath("warrings.txt");

    ConfigHost host;
    QString errmsg;
    if(host.loadBaseFile(errmsg, keywords_doc, warrings_doc)){
        QMessageBox::critical(nullptr, "载入配置过程出错", errmsg);
        qDebug() << errmsg << "loadbase err";
        return -1;
    }

    NovelHost novel(host);
    NovelBase::FStruct one;


    // actually work-code
start:
    auto opt = QMessageBox::information(nullptr, "打开已有小说？",
                                        "“确定”打开已有小说，\n“否定”新建空白小说，\n“取消”关闭软件!",
                                        QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes);

    if(opt == QMessageBox::Cancel) return 0;
    if(opt == QMessageBox::Yes){
        QString path = QFileDialog::getOpenFileName(nullptr, "选择打开的小说描述文件", QDir::homePath(),
                                                    "NovelStruct(*.nml)",nullptr, QFileDialog::DontResolveSymlinks);

        if(path == "") {
            QMessageBox::critical(nullptr, "未选择有效文件", "请重新选择有效文件");
            goto start;
        }

        try {
            one.openFile(path);
            novel.loadDescription(&one);
        } catch (WsException *e) {
            QMessageBox::critical(nullptr, "打开过程错误", e->reason());
            return -1;
        }
    }
    else if (opt == QMessageBox::No) {
select:
        auto dir_path = QFileDialog::getExistingDirectory(nullptr, "选择基准文件夹", QDir::homePath(),
                                                          QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);

        if(dir_path=="") goto start;
        auto target_path = QDir(dir_path).filePath("NovelStruct.nml");
        if(QFile(target_path).exists()){
            QMessageBox::critical(nullptr, "保存过程错误", "指定路径已存在内容，请重选路径！");
            goto select;
        }

        one.newEmptyFile();
        try {
            novel.loadDescription(&one);
            novel.save(target_path);
        } catch (WsException *e) {
            QMessageBox::critical(nullptr, "新建过程出错", e->reason());
            return -1;
        }
    }


    MainFrame w(&novel, host);
    w.show();

    return a.exec();
}
