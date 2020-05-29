#include "confighost.h"
#include "mainframe.h"
#include "novelhost.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ConfigHost host;
    NovelHost novel(host);

    StructureDescription one;
    one.newDescription();

    QString err;
    novel.loadDescription(err, &one);

    MainFrame w(&novel);
    w.show();

    return a.exec();
}
