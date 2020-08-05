#include "common.h"
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
#include <QToolBox>

using namespace NovelBase;
namespace NovelBase {
    struct MyHref{
        int despline_id;
        int point_id;
    };
}

Q_DECLARE_METATYPE(NovelBase::MyHref);

MainFrame::MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent)
    : QMainWindow(parent),
      timer_autosave(new QTimer(this)),
      novel_core(core),
      config(host),
      functions_split_base(new QSplitter(Qt::Horizontal, this)),
      outlines_navigate_treeview(new QTreeView(this)),
      volume_outlines_present(new QTextEdit(this)),                 // 卷宗细纲显示
      chapters_navigate_view(new QTreeView(this)),                  // 章卷导航与打开与切换
      search_result_navigate_view(new QTableView(this)),            // 搜索结果视图显示
      search_text_enter(new QLineEdit(this)),                       // 搜索内容键入框
      search(new QPushButton("检索")),
      clear(new QPushButton("清除")),
      chapter_textedit_present(new CQTextEdit(config, this)),               // 章节内容编辑
      chapter_outlines_present(new CQTextEdit(config, this)),               // 章节细纲视图1
      empty_document(chapter_textedit_present->document()),                 // 空白占位
      desplines_stack(new QTabWidget(this)),
      desplines_under_volume_view(new QTreeView(this)),                  // 卷内支线汇集
      desplines_remains_until_volume_view(new QTreeView(this)),
      desplines_remains_until_chapter_view(new QTreeView(this)),
      novel_outlines_present(new CQTextEdit(config, this)),                  // 全书大纲
      file(new QMenu("文件", this)),
      func(new QMenu("功能", this))
{
    setWindowTitle(novel_core->novelTitle());
    const QString splitter_style = "QSplitter::handle{background-color:lightgray;}"
                                   "QSplitter::handle:horizontal { width: 3px;}"
                                   "QSplitter::handle:vertical { height: 3px; }";

    connect(novel_core, &NovelHost::messagePopup,   this,   &MainFrame::acceptMessage);
    connect(novel_core, &NovelHost::warningPopup,   this,   &MainFrame::acceptWarning);
    connect(novel_core, &NovelHost::errorPopup,   this,   &MainFrame::acceptError);

    // 定制菜单
    {
        menuBar()->addMenu(file);
        file->addAction("新建卷宗",    this, &MainFrame::append_volume);
        file->addSeparator();
        file->addAction("保存",       this, &MainFrame::saveOp);
        file->addSeparator();
        file->addAction("重命名小说",  this, &MainFrame::rename_novel_title);

        menuBar()->addMenu(func);
        func->addAction("自动保存间隔", this, &MainFrame::autosave_timespan_reset);
        func->addAction("介质转换2.0-2.1", this, &MainFrame::convert20_21);
    }

    setCentralWidget(functions_split_base);
    functions_split_base->setStyleSheet(splitter_style);

    // 定制导航区域视图
    {
        auto click_navigate_cube = new QTabWidget(this);
        click_navigate_cube->setTabPosition(QTabWidget::West);

        click_navigate_cube->addTab(chapters_navigate_view, "卷章结构树");
        chapters_navigate_view->setContextMenuPolicy(Qt::CustomContextMenu);
        chapters_navigate_view->setModel(novel_core->chaptersNavigateTree());

        click_navigate_cube->addTab(outlines_navigate_treeview, "故事结构树");
        outlines_navigate_treeview->setModel(novel_core->outlineNavigateTree());
        outlines_navigate_treeview->setContextMenuPolicy(Qt::CustomContextMenu);

        auto search_pane = new QWidget(this);
        auto layout = new QGridLayout(search_pane);
        layout->setMargin(0);
        layout->setSpacing(2);
        search_result_navigate_view->setModel(novel_core->findResultTable());
        layout->addWidget(search_result_navigate_view, 0, 0, 5, 3);
        layout->addWidget(search_text_enter, 5, 0, 1, 3);
        layout->addWidget(search, 6, 0, 1, 1);
        layout->addWidget(clear, 6, 1, 1, 1);
        connect(search, &QPushButton::clicked,  this,   &MainFrame::search_text);
        connect(clear,  &QPushButton::clicked,  this,   &MainFrame::clear_search_result);
        click_navigate_cube->addTab(search_pane, "搜索结果");

        functions_split_base->addWidget(click_navigate_cube);
    }

    //定制右方内容编辑区域
    {
        auto edit_split_base = new QSplitter(Qt::Vertical, this);
        edit_split_base->setStyleSheet(splitter_style);
        functions_split_base->addWidget(edit_split_base);

        // 添加主编辑区域
        {
            auto edit_main_cube = new QSplitter(Qt::Horizontal, this);
            edit_main_cube->setStyleSheet(splitter_style);
            auto content_stack_tab = new QTabWidget(this);

            // 添加正文编辑界面
            {
                content_stack_tab->addTab(chapter_textedit_present, "正文编辑");

                content_stack_tab->addTab(volume_outlines_present, "卷宗细纲");
                volume_outlines_present->setDocument(novel_core->volumeOutlinesPresent());

                edit_main_cube->addWidget(content_stack_tab);
            }

            // 右方小区域
            {
                auto tabwidget = new QTabWidget(this);
                tabwidget->setTabPosition(QTabWidget::TabPosition::East);
                edit_main_cube->addWidget(tabwidget);
                //toolbox->setStyleSheet("QToolBox::tab{background-color: rgb(220, 220, 220);border-width: 1px;border-style: solid;"
                //                       "border-color: lightgray;}QToolBox::tab:selected{background-color: rgb(250, 250, 250);}");

                tabwidget->addTab(chapter_outlines_present, "章节细纲");
                chapter_outlines_present->setDocument(novel_core->chapterOutlinePresent());
            }
            edit_split_base->addWidget(edit_main_cube);
        }

        // 添加支线视图
        {
            desplines_stack->addTab(desplines_under_volume_view, "卷宗内建支线汇总");
            desplines_under_volume_view->setItemDelegateForColumn(5, new DesplineRedirect(novel_core));
            desplines_under_volume_view->setModel(novel_core->desplinesUnderVolume());
            desplines_under_volume_view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

            desplines_stack->addTab(desplines_remains_until_volume_view, "卷宗可见支线汇总");
            desplines_remains_until_volume_view->setModel(novel_core->desplinesUntilVolumeRemain());
            desplines_remains_until_volume_view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
            desplines_remains_until_volume_view->setItemDelegateForColumn(5, new DesplineRedirect(novel_core));

            desplines_stack->addTab(desplines_remains_until_chapter_view, "章节可见支线汇总");
            desplines_remains_until_chapter_view->setModel(novel_core->desplinesUntilChapterRemain());
            desplines_remains_until_chapter_view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
            desplines_remains_until_chapter_view->setItemDelegateForColumn(5, new DesplineRedirect(novel_core));

            desplines_stack->addTab(novel_outlines_present, "作品大纲");
            novel_outlines_present->setDocument(novel_core->novelOutlinesPresent());

            edit_split_base->addWidget(desplines_stack);
        }
    }

    connect(novel_core,                     &NovelHost::documentPrepared,           this,   &MainFrame::documentPresent);
    connect(chapters_navigate_view,         &QTreeView::clicked,                    this,   &MainFrame::chapters_navigate_jump);
    connect(chapters_navigate_view,         &QTreeView::customContextMenuRequested, this,   &MainFrame::show_chapters_operate);
    connect(outlines_navigate_treeview,     &QTreeView::clicked,                    this,   &MainFrame::outlines_navigate_jump);
    connect(outlines_navigate_treeview,     &QTreeView::customContextMenuRequested, this,   &MainFrame::outlines_manipulation);
    connect(search_result_navigate_view,    &QTableView::clicked,                   this,   &MainFrame::search_jump);
    connect(timer_autosave,                 &QTimer::timeout,                       this,   &MainFrame::saveOp);
    connect(novel_core,                     &NovelHost::currentVolumeActived,       this,   &MainFrame::currentVolumeOutlinesPresent);
    connect(novel_core,                     &NovelHost::currentChaptersActived,     this,   &MainFrame::currentChaptersAboutPresent);
    connect(desplines_under_volume_view,  &QWidget::customContextMenuRequested,   this,   &MainFrame::show_despline_operate);
    connect(desplines_remains_until_volume_view,&QWidget::customContextMenuRequested,   this,   &MainFrame::show_despline_operate);
    connect(desplines_remains_until_chapter_view,&QWidget::customContextMenuRequested,   this,   &MainFrame::show_despline_operate);

    {
        timer_autosave->start(5000*60);
        chapter_outlines_present->setEnabled(false);
        chapter_textedit_present->setEnabled(false);
        volume_outlines_present->setEnabled(false);

        desplines_under_volume_view->setEnabled(false);
        desplines_remains_until_volume_view->setEnabled(false);
        desplines_remains_until_chapter_view->setEnabled(false);
    }
}

MainFrame::~MainFrame(){}


void MainFrame::acceptMessage(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void MainFrame::acceptWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}

void MainFrame::acceptError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
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
    setWindowTitle(name);
}

void MainFrame::resize_foreshadows_tableitem_width()
{
    desplines_under_volume_view->resizeColumnToContents(0);
    desplines_under_volume_view->resizeColumnToContents(1);
    desplines_under_volume_view->resizeColumnToContents(4);
    desplines_under_volume_view->resizeColumnToContents(5);

    desplines_remains_until_volume_view->resizeColumnToContents(0);
    desplines_remains_until_volume_view->resizeColumnToContents(1);
    desplines_remains_until_volume_view->resizeColumnToContents(4);
    desplines_remains_until_volume_view->resizeColumnToContents(5);
    desplines_remains_until_volume_view->resizeColumnToContents(6);

    desplines_remains_until_chapter_view->resizeColumnToContents(0);
    desplines_remains_until_chapter_view->resizeColumnToContents(1);
    desplines_remains_until_chapter_view->resizeColumnToContents(4);
    desplines_remains_until_chapter_view->resizeColumnToContents(5);
    desplines_remains_until_chapter_view->resizeColumnToContents(6);
}

void MainFrame::chapters_navigate_jump(const QModelIndex &index0)
{
    chapters_navigate_view->resizeColumnToContents(0);

    QModelIndex index = index0;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    try {
        novel_core->setCurrentChaptersNode(index);
        resize_foreshadows_tableitem_width();
    } catch (WsException *e) {
        QMessageBox::critical(this, "切换当前章节", e->reason());
    }

}

void MainFrame::show_chapters_operate(const QPoint &point)
{
    auto index = chapters_navigate_view->indexAt(point);

    QMenu xmenu;
    switch (novel_core->indexDepth(index)) {
        case 0:
            xmenu.addAction(QIcon(":/outlines/icon/卷.png"), "添加卷宗", this, &MainFrame::append_volume);
            break;
        case 1:{
                xmenu.addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
                xmenu.addSeparator();
                xmenu.addAction(QIcon(":/outlines/icon/卷.png"), "增加卷宗", this,  &MainFrame::append_volume);
                xmenu.addAction(QIcon(":/outlines/icon/卷.png"), "插入卷宗", this,  &MainFrame::insert_volume);
                xmenu.addSeparator();
                xmenu.addAction(QIcon(":/outlines/icon/章.png"), "增加章节", this,  &MainFrame::append_chapter);
                xmenu.addSeparator();
                xmenu.addAction(QIcon(":/outlines/icon/伏.png"), "新建支线", this,  &MainFrame::append_despline_from_chapters);

                xmenu.addSeparator();
                xmenu.addAction("删除章卷", this, &MainFrame::remove_selected_chapters);
            }
            break;
        case 2:{
                xmenu.addAction("刷新字数统计", novel_core, &NovelHost::refreshWordsCount);
                xmenu.addSeparator();
                xmenu.addAction(QIcon(":/outlines/icon/章.png"), "增加章节", this,  &MainFrame::append_chapter);
                xmenu.addAction(QIcon(":/outlines/icon/章.png"), "插入章节", this,  &MainFrame::insert_chapter);
                xmenu.addSeparator();
                xmenu.addAction(QIcon(":/outlines/icon/伏.png"), "新建支线", this,  &MainFrame::append_despline_from_chapters);
                xmenu.addSeparator();

                QList<QPair<QString,int>> templist;
                novel_core->sumDesplinesUnderVolume(index, templist);

                auto point_chapter_attach_set = xmenu.addMenu("吸附驻点");
                for(auto despline_one : templist){
                    QList<QPair<QString,int>> attached_list;
                    novel_core->sumPointWithChapterSuspend(despline_one.second, attached_list);

                    if(attached_list.size()){
                        auto despline_menu = point_chapter_attach_set->addMenu(despline_one.first);
                        connect(despline_menu,   &QMenu::triggered,  this,   &MainFrame::pointattach_from_chapter);

                        for(auto attach_point : attached_list){
                            auto act = despline_menu->addAction(attach_point.first);
                            MyHref x = {despline_one.second, attach_point.second};
                            QVariant href; href.setValue(x);
                            act->setData(href);
                        }
                    }
                }

                auto point_chapter_attach_clear = xmenu.addMenu("分离驻点");
                for (auto despline_one : templist) {
                    QList<QPair<QString,int>> attached_list;
                    novel_core->sumPointWithChapterAttached(index, despline_one.second, attached_list);

                    if(attached_list.size()){
                        auto despline_menu = point_chapter_attach_clear->addMenu(despline_one.first);
                        connect(despline_menu,  &QMenu::triggered,  this,   &MainFrame::pointclear_from_chapter);

                        for(auto attach_point : attached_list){
                            auto act = despline_menu->addAction(attach_point.first);
                            MyHref x = {despline_one.second, attach_point.second};
                            QVariant href; href.setValue(x);
                            act->setData(href);
                        }
                    }
                }


                xmenu.addSeparator();
                xmenu.addAction("删除", this, &MainFrame::remove_selected_chapters);
                xmenu.addSeparator();
                xmenu.addAction("输出到剪切板", this, &MainFrame::content_output);
            }
            break;
    }

    xmenu.exec(mapToGlobal(point));
}

void MainFrame::append_volume()
{
    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertVolume(title, desp);
    } catch (WsException *e) {
        QMessageBox::critical(this, "增加卷宗", e->reason());
    }
}

void MainFrame::insert_volume()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    if(novel_core->indexDepth(index)==2)  // 目标为章节
        index = index.parent();

    try {
        novel_core->insertVolume(title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入卷宗", e->reason());
    }
}

void MainFrame::append_chapter()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    DescriptionIn dlg("插入新章节", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        if(novel_core->indexDepth(index)==1){  // volume-node
            novel_core->insertChapter(index, title, desp);
        }
        else {  // chapter-node
            novel_core->insertChapter(index.parent(), title, desp);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加章节", e->reason());
    }
}

void MainFrame::insert_chapter()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    DescriptionIn dlg("插入新章节", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertChapter(index.parent(), title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入章节", e->reason());
    }
}

void MainFrame::append_despline_from_chapters()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QString name="键入支线名称", desp0="键入支线总体描述";
    DescriptionIn dialog("输入支线信息", this);
    auto result = dialog.getDetailsDescription(name, desp0);
    if(result == QDialog::Rejected)
        return;

    if(novel_core->indexDepth(index) == 1)
        novel_core->appendDespline(index, name, desp0);
    else
        novel_core->appendDespline(index.parent(), name, desp0);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::pointattach_from_chapter(QAction *item)
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    auto href = item->data().value<MyHref>();
    novel_core->chapterAttachSet(index, href.point_id);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::pointclear_from_chapter(QAction *item)
{
    auto href = item->data().value<MyHref>();
    novel_core->chapterAttachClear(href.point_id);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::remove_selected_chapters()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QMessageBox msgBox;
    msgBox.setText("选中节点极其子节点（包含磁盘文件）将被删除！");
    msgBox.setInformativeText("是否确定执行?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    int ret = msgBox.exec();
    if(ret == QMessageBox::No)
        return;

    try {
        QList<QString> msgList;
        novel_core->checkChaptersRemoveEffect(index, msgList);
        if(msgList.size()){
            QString msg;
            for (auto line:msgList) {
                msg += line+"\n";
            }
            auto res = QMessageBox::warning(this, "移除卷章节点,影响如下：", msg, QMessageBox::Ok|QMessageBox::No, QMessageBox::No);
            if(res == QMessageBox::No)
                return;
        }

        novel_core->removeChaptersNode(index);
    } catch (WsException *e) {
        QMessageBox::critical(this, "删除文件", e->reason());
    }
}

void MainFrame::content_output()
{
    auto index = chapters_navigate_view->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    QClipboard *x = QApplication::clipboard();
    auto content = novel_core->chapterActiveText(index);
    x->setText(content);
}

void MainFrame::search_text()
{
    auto text = search_text_enter->text();
    if(!text.length())
        return;

    novel_core->searchText(text);
    search_result_navigate_view->resizeColumnsToContents();
}

void MainFrame::clear_search_result()
{
    novel_core->findResultTable()->clear();
}

void MainFrame::search_jump(const QModelIndex &xindex)
{
    QModelIndex index = xindex;
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    auto target_item = novel_core->findResultTable()->itemFromIndex(index);
    auto chapters_index = target_item->data().toModelIndex();
    auto select_start = target_item->data(Qt::UserRole+2).toInt();
    auto select_len = target_item->data(Qt::UserRole+3).toInt();

    novel_core->setCurrentChaptersNode(chapters_index);
    auto text_cursor = chapter_textedit_present->textCursor();
    text_cursor.setPosition(select_start);
    text_cursor.setPosition(select_start+select_len, QTextCursor::KeepAnchor);
    chapter_textedit_present->setTextCursor(text_cursor);
}

void MainFrame::outlines_navigate_jump(const QModelIndex &_index)
{
    auto index = _index;
    if(!index.isValid())
        return;

    try {
        novel_core->setCurrentOutlineNode(index);
        auto blk = novel_core->volumeOutlinesPresent()->firstBlock();
        while (blk.isValid()) {
            if(blk.userData()){
                auto ndata = static_cast<NovelBase::WsBlockData*>(blk.userData());
                if(ndata->navigateIndex() == index){
                    QTextCursor cursor(blk);
                    volume_outlines_present->setTextCursor(cursor);

                    auto at_value = volume_outlines_present->verticalScrollBar()->value();
                    volume_outlines_present->verticalScrollBar()->setValue(at_value + volume_outlines_present->cursorRect().y());
                    break;
                }
            }
            blk = blk.next();
        }
        resize_foreshadows_tableitem_width();
    } catch (WsException *e) {
        QMessageBox::critical(this, "大纲跳转", e->reason());
    }
}

void MainFrame::outlines_manipulation(const QPoint &point)
{
    auto index = outlines_navigate_treeview->indexAt(point);

    QMenu menu;
    switch (novel_core->indexDepth(index)) {
        case 0:
            menu.addAction(QIcon(":/outlines/icon/卷.png"), "添加分卷", this,   &MainFrame::append_volume);
            break;
        case 1:
            menu.addAction(QIcon(":/outlines/icon/卷.png"), "添加分卷", this,   &MainFrame::append_volume);
            menu.addAction(QIcon(":/outlines/icon/卷.png"), "插入分卷", this,   &MainFrame::insert_volume2);
            menu.addAction(QIcon(":/outlines/icon/情.png"), "添加情节", this,   &MainFrame::append_storyblock);
            break;
        case 2:{
                menu.addAction(QIcon(":/outlines/icon/情.png"), "添加情节", this,   &MainFrame::append_storyblock);
                menu.addAction(QIcon(":/outlines/icon/情.png"), "插入情节", this,   &MainFrame::insert_storyblock);
                menu.addAction(QIcon(":/outlines/icon/点.png"), "添加分解点",    this,   &MainFrame::append_keypoint);
                menu.addSeparator();
                QList<QPair<QString,int>> despline_list;
                novel_core->sumDesplinesUnderVolume(index, despline_list);

                auto point_attach = menu.addMenu("吸附驻点");
                for(auto despline_one : despline_list){
                    QList<QPair<QString,int>> suspend_list;
                    novel_core->sumPointWithStoryblcokSuspend(despline_one.second, suspend_list);

                    if(suspend_list.size()){
                        auto despline_item = point_attach->addMenu(despline_one.first);
                        connect(despline_item,     &QMenu::triggered,  this,   &MainFrame::pointattach_from_storyblock);

                        for(auto suspend_point : suspend_list){
                            auto action = despline_item->addAction(suspend_point.first);
                            MyHref x{despline_one.second, suspend_point.second};
                            QVariant href; href.setValue(x);
                            action->setData(href);
                        }
                    }
                }

                auto point_clear = menu.addMenu("分离驻点");
                for(auto despline_one : despline_list){
                    QList<QPair<QString,int>> attached_list;
                    novel_core->sumPointWithStoryblockAttached(index, despline_one.second, attached_list);

                    if(attached_list.size()){
                        auto despline_item = point_clear->addMenu(despline_one.first);
                        connect(despline_item,  &QMenu::triggered,  this,   &MainFrame::pointclear_from_storyblock);

                        for(auto attached_point : attached_list){
                            auto action = despline_item->addAction(attached_point.first);
                            MyHref x{despline_one.second, attached_point.second};
                            QVariant href; href.setValue(x);
                            action->setData(href);
                        }
                    }
                }
            }
            break;
        case 3:
            menu.addAction(QIcon(":/outlines/icon/点.png"), "添加分解点",    this,   &MainFrame::append_keypoint);
            menu.addAction(QIcon(":/outlines/icon/点.png"), "插入分解点",    this,   &MainFrame::insert_keypoint);
            break;
    }
    if(novel_core->indexDepth(index)){
        menu.addSeparator();
        menu.addAction(QIcon(":/outlines/icon/伏.png"), "添加支线", this,   &MainFrame::append_despline_from_outlines);
        menu.addSeparator();
        menu.addAction("删除",   this,   &MainFrame::remove_selected_outlines);
    }

    menu.exec(mapToGlobal(point));
}

void MainFrame::insert_volume2()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertVolume(title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入分卷", e->reason());
    }
}

void MainFrame::append_storyblock()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    DescriptionIn dlg("插入新剧情", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        if(novel_core->indexDepth(index)==1) {                          // 分卷节点
            novel_core->insertStoryblock(index, title, desp);
        }
        else if (novel_core->indexDepth(index)==2) {                    // 剧情节点
            novel_core->insertStoryblock(index.parent(), title, desp);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加剧情", e->reason());
    }
}

void MainFrame::insert_storyblock()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertStoryblock(index.parent(), title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入剧情", e->reason());
    }
}

void MainFrame::append_keypoint()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        if(novel_core->indexDepth(index)==2){
            novel_core->insertKeypoint(index, title, desp);
        }
        else {
            novel_core->insertKeypoint(index.parent(), title, desp);
        }
    } catch (WsException *e) {
        QMessageBox::critical(this, "添加分解点", e->reason());
    }
}

void MainFrame::insert_keypoint()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertKeypoint(index.parent(), title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入分解点", e->reason());
    }
}

void MainFrame::append_despline_from_outlines()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    while (novel_core->indexDepth(index)!=1) {
        index = index.parent();
    }

    novel_core->appendDespline(index, title, desp);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::pointattach_from_storyblock(QAction *item)
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    auto href = item->data().value<MyHref>();
    novel_core->storyblockAttachSet(index, href.point_id);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::pointclear_from_storyblock(QAction *item)
{
    auto href = item->data().value<MyHref>();
    novel_core->storyblockAttachClear(href.point_id);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::remove_selected_outlines()
{
    auto index = outlines_navigate_treeview->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    try {
        QList<QString> result;
        novel_core->checkOutlinesRemoveEffect(index, result);
        if(result.size()){
            QString msg;
            for (auto line:result) {
                msg += line+"\n";
            }
            auto res = QMessageBox::warning(this, "移除大纲节点,影响如下：", msg, QMessageBox::Ok|QMessageBox::No, QMessageBox::No);
            if(res == QMessageBox::No)
                return;
        }

        novel_core->removeOutlinesNode(index);
    } catch (WsException *e) {
        QMessageBox::critical(this, "移除卷章", e->reason());
    }
}

void MainFrame::saveOp()
{
    try {
        novel_core->save();
    } catch (WsException *e) {
        QMessageBox::critical(this, "保存过程出错", e->reason(), QMessageBox::Ok);
    }
}

void MainFrame::autosave_timespan_reset()
{
    bool ok;
    auto timespan = QInputDialog::getInt(this, "重设自动保存间隔", "输入时间间隔(分钟)", 5, 1, 30, 1, &ok);
    if(!ok) return;

    timer_autosave->start(timespan*1000*60);
}

void MainFrame::documentClosed(QTextDocument *)
{
    setWindowTitle(novel_core->novelTitle());
    chapter_textedit_present->setDocument(empty_document);
}

void MainFrame::documentPresent(QTextDocument *doc, const QString &title)
{
    auto title_novel = novel_core->novelTitle();
    setWindowTitle(title_novel+":"+title);

    this->chapter_textedit_present->setDocument(doc);
}

void MainFrame::currentChaptersAboutPresent()
{
    chapter_outlines_present->setEnabled(true);
    chapter_textedit_present->setEnabled(true);

    desplines_under_volume_view->setEnabled(true);
    desplines_remains_until_volume_view->setEnabled(true);
    desplines_remains_until_chapter_view->setEnabled(true);
}

void MainFrame::currentVolumeOutlinesPresent()
{
    volume_outlines_present->setEnabled(true);

    desplines_under_volume_view->setEnabled(true);
    desplines_remains_until_volume_view->setEnabled(true);
}

void MainFrame::convert20_21()
{
    auto source_path = QFileDialog::getOpenFileName(this, "转换目标");
    if(source_path == "" || !QFile(source_path).exists()){
        QMessageBox::critical(this, "介质转换：指定源头", "目标为空");
        return;
    }

    auto target_path = QFileDialog::getSaveFileName(this, "选择存储位置");
    if(QFile(target_path).exists()){
        QMessageBox::critical(this, "介质转换：指定结果", "不允许覆盖文件");
        return;
    }

    if(!target_path.endsWith(".wsnf"))
        target_path += ".wsnf";

    novel_core->convert20_21(target_path, source_path);
}

void MainFrame::show_despline_operate(const QPoint &point)
{
    QMenu menu(this);
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());

    menu.addAction("添加新支线", this,   &MainFrame::append_despline_from_desplineview);
    menu.addAction("删除支线", this, &MainFrame::remove_despline_from_desplineview);
    menu.addSeparator();
    menu.addAction("刷新支线模型", this,  &MainFrame::refresh_desplineview);
    menu.addSeparator();

    auto index_point = widget->indexAt(point);
    if(index_point.isValid()){
        auto index_kind = index_point.sibling(index_point.row(), 0);
        auto node_kind = widget->model()->data(index_kind, Qt::UserRole+1).toInt();
        menu.addAction("添加驻点",  this,   &MainFrame::append_attachpoint_from_desplineview);

        if(node_kind == 2){
            menu.addAction("插入驻点",  this,   &MainFrame::insert_attachpoint_from_desplineview);
            menu.addAction("移除驻点", this, &MainFrame::remove_attachpoint_from_desplineview);

            menu.addSeparator();
            menu.addAction("驻点上移",  this,   &MainFrame::attachpoint_moveup);
            menu.addAction("驻点下移",  this,   &MainFrame::attachpoint_movedown);
        }
    }

    menu.exec(widget->mapToGlobal(point));
}

void MainFrame::append_despline_from_desplineview()
{
    DescriptionIn dlg("插入新卷宗", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    novel_core->appendDesplineUnderCurrentVolume(title, desp);

    novel_core->refreshDesplinesSummary();
    resize_foreshadows_tableitem_width();
}

void MainFrame::remove_despline_from_desplineview()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    auto id_index = disp_index.sibling(disp_index.row(), 1);

    try {
        novel_core->removeDespline(id_index.data(Qt::UserRole+1).toInt());
        widget->model()->removeRow(disp_index.row(), disp_index.parent());
    } catch (WsException *e) {
        QMessageBox::critical(this, "移除支线", e->reason());
    }
}

void MainFrame::append_attachpoint_from_desplineview()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    auto id_index = disp_index.sibling(disp_index.row(), 1);
    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    DescriptionIn dlg("插入驻点", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    auto type = disp_index.data(Qt::UserRole+1);
    if(type == 1){
        disp_index = widget->model()->index(widget->model()->rowCount(disp_index)-1, 0);
        novel_core->insertAttachpoint(id_index.data(Qt::UserRole+1).toInt(), title, desp);
    }
    else {
        auto despline_id = disp_index.parent().sibling(disp_index.parent().row(), 1);
        novel_core->insertAttachpoint(despline_id.data(Qt::UserRole+1).toInt(), title, desp);
    }

    auto poslist = extractPositionData(disp_index);
    novel_core->refreshDesplinesSummary();
    scrollToSamePosition(widget, poslist);
    resize_foreshadows_tableitem_width();
}

void MainFrame::insert_attachpoint_from_desplineview()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    auto id_index = disp_index.sibling(disp_index.row(), 1);
    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    DescriptionIn dlg("插入驻点", this);
    QString title = "键入名称", desp = "键入描述";
    if(dlg.getDetailsDescription(title, desp)==QDialog::Rejected)
        return;

    auto despline_id = disp_index.parent().sibling(disp_index.parent().row(), 1);
    novel_core->insertAttachpoint(despline_id.data(Qt::UserRole+1).toInt(), title, desp, id_index.data().toInt());

    auto poslist = extractPositionData(disp_index);
    novel_core->refreshDesplinesSummary();
    scrollToSamePosition(widget, poslist);
    resize_foreshadows_tableitem_width();
}

void MainFrame::remove_attachpoint_from_desplineview()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    auto id_index = disp_index.sibling(disp_index.row(), 1);

    try {
        novel_core->removeAttachpoint(id_index.data(Qt::UserRole+1).toInt());
        widget->model()->removeRow(disp_index.row(), disp_index.parent());
    } catch (WsException *e) {
        QMessageBox::critical(this, "移除驻点", e->reason());
    }
}

void MainFrame::attachpoint_moveup()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;
    auto id_index = disp_index.sibling(disp_index.row(), 1);

    novel_core->attachPointMoveup(id_index.data(Qt::UserRole+1).toInt());

    auto poslist = extractPositionData(disp_index);
    novel_core->refreshDesplinesSummary();
    scrollToSamePosition(widget, poslist);
    resize_foreshadows_tableitem_width();
}

void MainFrame::attachpoint_movedown()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    auto id_index = disp_index.sibling(disp_index.row(), 1);
    novel_core->attachPointMovedown(id_index.data(Qt::UserRole+1).toInt());

    auto poslist = extractPositionData(disp_index);
    novel_core->refreshDesplinesSummary();
    scrollToSamePosition(widget, poslist);
    resize_foreshadows_tableitem_width();
}

void MainFrame::refresh_desplineview()
{
    auto widget = static_cast<QTreeView*>(desplines_stack->currentWidget());
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    auto poslist = extractPositionData(disp_index);
    novel_core->refreshDesplinesSummary();
    scrollToSamePosition(widget, poslist);
    resize_foreshadows_tableitem_width();
}

QList<QPair<int, int> > MainFrame::extractPositionData(const QModelIndex &index) const
{
    QList<QPair<int, int>> stack;
    auto node = index;
    while (node.isValid()) {
        stack.insert(0, qMakePair(node.row(), node.column()));
        node = node.parent();
    }
    return stack;
}

void MainFrame::scrollToSamePosition(QAbstractItemView *view, const QList<QPair<int, int>> &poslist) const
{
    QModelIndex pos_index_temp;
    for (auto pos : poslist) {
        auto pos_index = view->model()->index(pos.first, pos.second, pos_index_temp);
        if(!pos_index.isValid()){
            if(!view->model()->rowCount(pos_index_temp))
                break;
            pos_index = view->model()->index(0, 0, pos_index_temp);
        }
        pos_index_temp = pos_index;
    }
    view->scrollTo(pos_index_temp, QAbstractItemView::PositionAtCenter);
}



CQTextEdit::CQTextEdit(ConfigHost &config, QWidget *parent)
    :QTextEdit(parent),host(config){}

void CQTextEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasText()) {
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
