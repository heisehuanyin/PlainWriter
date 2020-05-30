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

    //config check;
    QDir software_root(QDir::home().filePath(".PlainWriter"));
    if(!software_root.exists()){
        QDir::home().mkdir(".PlainWriter");
    }
    auto keywords_doc = software_root.filePath("keywords.txt");
    auto warrings_doc = software_root.filePath("warrings.txt");


    // actually work-code
    start:
    auto opt = QMessageBox::information(nullptr, "打开已有小说？",
                                        "“确定”打开已有小说，\n“否定”新建空白小说，\n“取消”关闭软件!",
                             QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes);

    if(opt == QMessageBox::Cancel)
        return 0;


    ConfigHost host;
    NovelHost novel(host);
    StructDescription one;

    QString errmsg;
    if(host.loadBaseFile(errmsg, keywords_doc, warrings_doc)){
        QMessageBox::critical(nullptr, "载入配置过程出错", errmsg);
        qDebug() << errmsg << "loadbase err";
        return -1;
    }

    if(opt == QMessageBox::Yes){
        QString path = QFileDialog::getOpenFileName(nullptr, "选择打开的小说描述文件", QDir::homePath(),
                            "NovelStruct(*.nml)",nullptr, QFileDialog::DontResolveSymlinks);

        if(path == "") {
            QMessageBox::critical(nullptr, "未选择有效文件", "请重新选择有效文件");
            goto start;
        }

        QString err;
        int code;
        if((code = one.openDescription(err, path))){
            QMessageBox::critical(nullptr, "打开过程错误", err);
            qDebug() << err << path;
            goto start;
        }

        if((code = novel.loadDescription(err, &one))){
            QMessageBox::critical(nullptr, "加载过程出错", err);
            goto start;
        }
    }
    else if (opt == QMessageBox::No) {
        one.newDescription();

        QString err; int code;
        if((code = novel.loadDescription(err, &one))){
            QMessageBox::critical(nullptr, "加载过程出错", err);
            return -1;
        }

        select:
        auto dir_path = QFileDialog::getExistingDirectory(nullptr, "选择基准文件夹", QDir::homePath(),
                                     QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);

        auto target_path = QDir(dir_path).filePath("NovelStruct.nml");
        if(QFile(target_path).exists()){
            QMessageBox::critical(nullptr, "保存过程错误", "指定路径已存在内容，请重选路径！");
            goto select;
        }
        if(novel.save(err, target_path)){
            auto opcode = QMessageBox::critical(nullptr, "保存过程出错", err+"\n是否关闭软件",
                                                QMessageBox::Yes|QMessageBox::No, QMessageBox::No);

            if(opcode == QMessageBox::Yes)
                return 0;
            goto select;
        }
    }


    MainFrame w(&novel);
    w.show();

    return a.exec();
}
