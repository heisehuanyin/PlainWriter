#include "mainframe.h"

#include <QtDebug>
#include <QScrollBar>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QGridLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

MainFrame::MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent)
    : QMainWindow(parent),
      timer_autosave(new QTimer(this)),
      novel_core(core),
      split_panel(new QSplitter(this)),
      node_navigate_view(new QTreeView(this)),
      search_result_view(new QTableView(this)),
      search_text_enter(new QLineEdit(this)),
      search(new QPushButton("搜索", this)),
      clear(new QPushButton("清空", this)),
      text_edit_block(new CQTextEdit(host,this)),
      empty_document(text_edit_block->document()),
      file(new QMenu("文件", this)),
      func(new QMenu("功能", this))
{
    setWindowTitle(novel_core->novelTitle());
    menuBar()->addMenu(file);
    file->addAction("新建卷宗",    this, &MainFrame::append_volume);
    file->addSeparator();
    file->addAction("保存",       this, &MainFrame::saveOp);
    file->addSeparator();
    file->addAction("重命名小说",  this, &MainFrame::rename_novel_title);
    menuBar()->addMenu(func);
    func->addAction("自动保存间隔", this, &MainFrame::autosave_timespan_reset);

    setCentralWidget(split_panel);
    auto search_pane = new QWidget(this);
    auto layout = new QGridLayout(search_pane);
    layout->setMargin(0);
    layout->setSpacing(2);
    layout->addWidget(search_result_view, 0, 0, 5, 3);
    layout->addWidget(search_text_enter, 5, 0, 1, 3);
    layout->addWidget(search, 6, 0, 1, 1);
    layout->addWidget(clear, 6, 1, 1, 1);
    connect(search, &QPushButton::clicked,  this,   &MainFrame::search_text);
    connect(clear,  &QPushButton::clicked,  this,   &MainFrame::clear_search_result);

    node_navigate_view->setModel(novel_core->navigateTree());
    search_result_view->setModel(novel_core->searchResultPresent());
    auto tab = new QTabWidget(this);
    tab->setTabPosition(QTabWidget::West);
    tab->addTab(node_navigate_view, "小说结构");
    tab->addTab(search_pane, "搜索结果");

    node_navigate_view->setContextMenuPolicy(Qt::CustomContextMenu);
    split_panel->addWidget(tab);
    auto w = split_panel->width();
    QList<int> ws;
    ws.append(40);ws.append(w-40);
    split_panel->setSizes(ws);

    split_panel->addWidget(text_edit_block);

    connect(node_navigate_view,     &QTreeView::clicked,        this,   &MainFrame::navigate_jump);
    connect(node_navigate_view,     &QTreeView::customContextMenuRequested, this,   &MainFrame::show_manipulation);
    connect(search_result_view,     &QTableView::clicked,       this,   &MainFrame::search_jump);
    connect(novel_core,             &NovelHost::documentOpened, this,   &MainFrame::documentOpened);
    connect(novel_core,     &NovelHost::documentActived,        this,   &MainFrame::documentActived);
    connect(novel_core,     &NovelHost::documentAboutToBeClosed,this,   &MainFrame::documentClosed);
    connect(timer_autosave,         &QTimer::timeout,           this,   &MainFrame::saveOp);
    timer_autosave->start(5000*60);
}

MainFrame::~MainFrame()
{

}

void MainFrame::rename_novel_title()
{
start:
    bool ok;
    auto name = QInputDialog::getText(this, "重命名小说", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok) return;

    if(name == ""){
        QMessageBox::information(this, "重命名小说", "未检测到新名");
        goto start;
    }

    novel_core->resetNovelTitle(name);
}

void MainFrame::navigate_jump(const QModelIndex &index0)
{
    QModelIndex index = index0;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    if(!(novel_core->navigateTree()->itemFromIndex(index))->parent())
        return;

    QString err;
    if(novel_core->openDocument(err, index))
        QMessageBox::critical(this, "打开文档❌", err);
}

void MainFrame::show_manipulation(const QPoint &point)
{
    auto index = node_navigate_view->indexAt(point);
    if(!index.isValid())
        return;

    auto xmenu = new QMenu("节点操控", this);
    xmenu->addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
    xmenu->addSeparator();
    xmenu->addAction("增加卷宗", this, &MainFrame::append_volume);
    xmenu->addAction("增加章节", this, &MainFrame::append_chapter);
    xmenu->addSeparator();
    xmenu->addAction("删除当前", this, &MainFrame::remove_selected);
    xmenu->addSeparator();
    xmenu->addAction("输出到剪切板", this, &MainFrame::content_output);

    xmenu->exec(mapToGlobal(point));
    delete xmenu;
}

void MainFrame::append_volume()
{
    bool ok;
    auto title = QInputDialog::getText(this, "新建卷宗", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    QString err;
    if(novel_core->appendVolume(err, title))
        QMessageBox::critical(this, "新增卷宗过程出错", err);
}

void MainFrame::append_chapter()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    bool ok;
    auto title = QInputDialog::getText(this, "新建章节", "输入名称", QLineEdit::Normal, QString(), &ok);
    if(!ok || !title.size()) return;

    QString err;
    if(novel_core->appendChapter(err, title, index))
        QMessageBox::critical(this, "新增章节过程出错", err);
}

void MainFrame::remove_selected()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    QMessageBox msgBox;
    msgBox.setText("选中节点极其子节点（包含磁盘文件）将被删除！");
    msgBox.setInformativeText("是否确定执行?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    int ret = msgBox.exec();
    if(ret == QMessageBox::No)
        return;

    QString err;
    if((ret = novel_core->removeNode(err, index)))
        QMessageBox::critical(this, "删除过程出错", err);
}

void MainFrame::content_output()
{
    auto index = node_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    QClipboard *x = QApplication::clipboard();
    QString text, err;
    novel_core->chapterTextContent(err, index, text);
    text.replace("\u2029", "\n");
    x->setText(text);
}

void MainFrame::search_text()
{
    auto text = search_text_enter->text();
    if(!text.length())
        return;

    novel_core->searchText(text);
    search_result_view->resizeColumnsToContents();
}

void MainFrame::clear_search_result()
{
    novel_core->searchResultPresent()->clear();
}

void MainFrame::search_jump(const QModelIndex &xindex)
{
    QModelIndex index = xindex;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto item = novel_core->searchResultPresent()->itemFromIndex(index);
    QString err;
    if(novel_core->openDocument(err, item->data().toModelIndex())){
        QMessageBox::critical(this, "搜索跳转❌", err);
        return;
    }

    QTextCursor cursor = text_edit_block->textCursor();
    cursor.clearSelection();
    auto pos = item->data(Qt::UserRole+2).toInt();
    auto len = item->data(Qt::UserRole+3).toInt();
    cursor.setPosition(pos);
    cursor.setPosition(pos+len, QTextCursor::KeepAnchor);
    text_edit_block->setTextCursor(cursor);
}

void MainFrame::saveOp()
{
    QString err;

    if(novel_core->save(err))
        QMessageBox::critical(this, "保存过程出错", err, QMessageBox::Ok);
}

void MainFrame::autosave_timespan_reset()
{
    bool ok;
    auto timespan = QInputDialog::getInt(this, "重设自动保存间隔", "输入时间间隔(分钟)", 5, 1, 30, 1, &ok);
    if(!ok) return;

    timer_autosave->start(timespan*1000*60);
}

void MainFrame::documentOpened(QTextDocument *doc, const QString &title){}

void MainFrame::documentClosed(QTextDocument *)
{
    text_edit_block->setDocument(empty_document);
}

void MainFrame::documentActived(QTextDocument *doc, const QString &title)
{
    auto title_novel = novel_core->novelTitle();
    setWindowTitle(title_novel+":"+title);

    text_edit_block->setDocument(doc);
}



CQTextEdit::CQTextEdit(ConfigHost &config, QWidget *parent)
    :QTextEdit(parent),host(config){}

void CQTextEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasText() ) {
        QTextBlockFormat format0;
        QTextCharFormat format1;
        host.textFormat(format0, format1);

        QString context = source->text();
        QTextCursor cursor = this->textCursor();
        cursor.setBlockFormat(format0);
        cursor.setBlockCharFormat(format1);
        cursor.insertText(context);
    }
}
