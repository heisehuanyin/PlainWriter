#include "common.h"
#include "confighost.h"
#include "mainframe.h"
#include "novelhost.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyleFactory>
#include <QtDebug>

using namespace NovelBase;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    /*qDebug() << QStyleFactory::keys();*/
    a.setStyle("fusion");
    int retval = 0;

    try {
        //config check;
        QDir software_root(QDir::home().filePath(".PlainWriter"));
        if(!software_root.exists()){
            QDir::home().mkdir(".PlainWriter");
        }
        auto keywords_doc = software_root.filePath("keywords.txt");
        auto warrings_doc = software_root.filePath("warrings.txt");

        ConfigHost config_base;
        QString errmsg;
        if(config_base.loadBaseFile(errmsg, keywords_doc, warrings_doc)){
            QMessageBox::critical(nullptr, "载入配置过程出错", errmsg);
            qDebug() << errmsg << "loadbase err";
            return -1;
        }

        NovelHost novel_core(config_base);
        NovelBase::DBAccess db_access;

        // actually work-code
start:
        auto opt = QMessageBox::information(nullptr, "打开已有小说？", "“确定”打开已有小说，\n“否定”新建空白小说，\n“取消”关闭软件!",
                                            QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes);

        if(opt == QMessageBox::Cancel)
            return 0;

        if(opt == QMessageBox::Yes){
            QString path = QFileDialog::getOpenFileName(nullptr, "选择小说描述文件", QDir::homePath(), "NovelStruct(*.wsnf)",
                                                        nullptr, QFileDialog::DontResolveSymlinks);

            if(path == "") {
                QMessageBox::critical(nullptr, "未选择有效文件", "请重新选择有效文件");
                goto start;
            }

            db_access.loadFile(path);
            novel_core.loadBase(&db_access);
        }
        else if (opt == QMessageBox::No) {
select:
            auto path = QFileDialog::getSaveFileName(nullptr, "选择基准文件夹", QDir::homePath(), "NovelStruct(*.wsnf)", nullptr,
                                                     QFileDialog::DontResolveSymlinks);

            if(path=="")
                goto start;


            if(!path.endsWith(".wsnf"))
                path += ".wsnf";

            if(QFile(path).exists()){
                QMessageBox::critical(nullptr, "保存过程错误", "指定路径已存在内容，请重选路径！");
                goto select;
            }

            db_access.createEmptyFile(path);
            novel_core.loadBase(&db_access);
        }


        MainFrame w(&novel_core, config_base);
        w.show();

        retval = a.exec();
        novel_core.save();

    } catch (WsException *e0) {
        QMessageBox::critical(nullptr, "可预料未捕捉异常", e0->reason());
    } catch (std::exception *e1){
        QMessageBox::critical(nullptr, "意料之外异常", e1->what());
    }

    return retval;
}
