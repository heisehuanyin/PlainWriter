#include "common.h"
#include "mainframe.h"
#include "float.h"

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
#include <QDoubleSpinBox>
#include <QStackedLayout>

using namespace NovelBase;
using namespace WidgetBase;

#define CHAPTERS_NAV_VIEW "项目视图"
#define STORYBLK_NAV_VIEM "剧情结构"
#define NOVEL_OUTLINE_EDIT_VIEW "作品大纲"
#define VOLUME_OUTLINES_EDIT_VIEW "卷宗大纲"
#define CHPS_OUTLINES_EDIT_VIEW "章节大纲"
#define DESPLINES_SUM_UNDER_VOLUME "卷宗内支线汇总"
#define DESPLINES_SUM_UNTIL_VOLUME "卷宗继承支线汇总"
#define DESPLINES_SUM_UNTIL_CHAPTER "章节继承支线汇总"
#define KEYWORDS_MANAGER_VIEW "关键字管理"
#define KEYWORDS_QUICKLOOK_VIEW "关键字提示"
#define SEARCH_RESULT_LABLE_VIEW "搜索操作"

const QString treeview_style = "QTreeView { alternate-background-color: #f7f7f7; show-decoration-selected: 1; }"
                               "QTreeView::item {  border: 1px solid #d9d9d9; border-right-color: transparent; "
                               "    border-top-color: transparent; border-bottom-color: transparent; }"
                               "QTreeView::item:hover { border: 1px solid #bfcde4; "
                               "    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #e7effd, stop: 1 #cbdaf1); }"
                               "QTreeView::item:selected { border: 1px solid #567dbc; }"
                               "QTreeView::item:selected:active{"
                               "    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #6ea1f1, stop: 1 #567dbc); }"
                               "QTreeView::item:selected:!active { color: white;"
                               "    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #6b9be8, stop: 1 #577fbf); }";


#define NEW_VIEWSELECTOR(name) \
    auto (name) = new ViewSelector(this);\
    connect((name),  &ViewSelector::focusLockReport,        this,   &MainFrame::acceptFocusLockRequest);\
    connect((name),  &ViewSelector::splitLeft,              this,   &MainFrame::acceptSplitLeftRequest);\
    connect((name),  &ViewSelector::splitRight,             this,   &MainFrame::acceptSplitRightRequest);\
    connect((name),  &ViewSelector::splitTop,               this,   &MainFrame::acceptSplitTopRequest);\
    connect((name),  &ViewSelector::splitBottom,            this,   &MainFrame::acceptSplitBottomRequest);\
    connect((name),  &ViewSelector::splitCancel,            this,   &MainFrame::acceptSplitCancelRequest);\
    connect((name),  &ViewSelector::closeViewRequest,       this,   &MainFrame::acceptViewCloseRequest);\
    connect((name),  &ViewSelector::currentViewItemChanged, this,   &MainFrame::acceptViewmItemChanged);\
    connect(this,   &MainFrame::itemNameAvaliable,          (name), &ViewSelector::acceptItemInsert);\
    connect(this,   &MainFrame::itemNameUnavaliable,        (name), &ViewSelector::acceptItemRemove);\
    for (auto tuple : views_group) {\
    auto hold = std::get<2>(tuple);\
    if(!hold)\
    (name)->acceptItemInsert(std::get<0>(tuple));\
    }


#define NEW_VIEWSPLITTER(name, orientation) \
    auto (name) = new ViewSplitter(orientation, this); \
    (name)->setChildrenCollapsible(false);\
    connect((name), &ViewSplitter::splitterPositionMoved,   this,   &MainFrame::acceptSplitterPostionChanged);


MainFrame::MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent)
    : QMainWindow(parent),
      timer_autosave(new QTimer(this)),
      novel_core(core),
      config(host),
      file(new QMenu("文件", this)),
      func(new QMenu("功能", this))
{
    setWindowTitle(novel_core->novelTitle());

    {
        menuBar()->addMenu(file);
        menuBar()->addMenu(func);
        file->addAction("void");
        func->addAction("保存视图结构", [this]{this->_structure_output();});
    }

    connect(novel_core,             &NovelHost::messagePopup,           this,   &MainFrame::acceptMessage);
    connect(novel_core,             &NovelHost::warningPopup,           this,   &MainFrame::acceptWarning);
    connect(novel_core,             &NovelHost::errorPopup,             this,   &MainFrame::acceptError);

    connect(novel_core,             &NovelHost::documentPrepared,       this,   &MainFrame::documentPresent);
    connect(novel_core,             &NovelHost::documentAboutToBoClosed,this,   &MainFrame::documentClosed);
    connect(timer_autosave,         &QTimer::timeout,                   this,   &MainFrame::saveOp);
    connect(novel_core,             &NovelHost::currentVolumeActived,   this,   &MainFrame::currentVolumeOutlinesPresent);
    connect(novel_core,             &NovelHost::currentChaptersActived, this,   &MainFrame::currentChaptersAboutPresent);


    load_all_predefine_views();

    NEW_VIEWSELECTOR(basic);
    setCentralWidget(basic);

    timer_autosave->start(5000*60);
}

MainFrame::~MainFrame(){}

QWidget *MainFrame::locateAndViewlockChangeViaTitlename(ViewSelector *poster, const QString &name)
{
    for (auto index=0; index<views_group.size(); ++index) {
        auto tuple = views_group.at(index);

        auto title = std::get<0>(tuple);
        auto view = std::get<1>(tuple);
        auto host = std::get<2>(tuple);

        // 解除原锁定
        if(host == poster){
            views_group.insert(index, std::make_tuple(title, view, nullptr));
            views_group.removeAt(index+1);

            disconnect(this,&MainFrame::itemNameAvaliable,  poster,     &ViewSelector::acceptItemInsert);
            emit this->itemNameAvaliable(title);
            connect(this,&MainFrame::itemNameAvaliable,  poster,     &ViewSelector::acceptItemInsert);
            break;
        }
    }

    for (auto index=0; index<views_group.size(); ++index) {
        auto tuple = views_group.at(index);

        auto title = std::get<0>(tuple);
        auto view = std::get<1>(tuple);
        // 构建锁定关联
        if(title == name){
            views_group.insert(index, std::make_tuple(title, view, poster));
            views_group.removeAt(index+1);

            disconnect(this,&MainFrame::itemNameUnavaliable,    poster, &ViewSelector::acceptItemRemove);
            emit this->itemNameUnavaliable(name);
            connect(this,   &MainFrame::itemNameUnavaliable,    poster, &ViewSelector::acceptItemRemove);
            return view;
        }
    }

    return nullptr;
}

QWidget *MainFrame::get_view_according_name(const QString &name) const {
    for (auto tuple : views_group) {
        if(std::get<0>(tuple) == name)
            return std::get<1>(tuple);
    }
    throw new NovelBase::WsException("非法名称: "+name);
}

void MainFrame::load_all_predefine_views(){
    auto chps_nav_model = novel_core->chaptersNavigateTree();
    auto chps_nav_view = group_chapters_navigate_view(chps_nav_model);
    views_group.append(std::make_tuple(CHAPTERS_NAV_VIEW, chps_nav_view, nullptr));

    auto outlines_nav_model = novel_core->outlineNavigateTree();
    auto outlines_nav_view = group_storyblks_navigate_view(outlines_nav_model);
    views_group.append(std::make_tuple(STORYBLK_NAV_VIEM, outlines_nav_view, nullptr));

    auto novel_outlines_edit = novel_core->novelOutlinesPresent();
    auto novel_outlines_edit_view = group_textedit_panel(novel_outlines_edit);
    views_group.append(std::make_tuple(NOVEL_OUTLINE_EDIT_VIEW, novel_outlines_edit_view, nullptr));

    auto volume_outlines_edit = novel_core->volumeOutlinesPresent();
    auto volume_outlines_edit_view = group_textedit_panel(volume_outlines_edit);
    volume_outlines_edit_view->setEnabled(false);
    views_group.append(std::make_tuple(VOLUME_OUTLINES_EDIT_VIEW, volume_outlines_edit_view, nullptr));

    auto chps_outlines_edit = novel_core->chapterOutlinePresent();
    auto chps_outlines_edit_view = group_textedit_panel(chps_outlines_edit);
    chps_outlines_edit_view->setEnabled(false);
    views_group.append(std::make_tuple(CHPS_OUTLINES_EDIT_VIEW, chps_outlines_edit_view, nullptr));

    auto desplines_under_volume_sum = novel_core->desplinesUnderVolume();
    auto desplines_under_volume_sum_view = group_desplines_manage_view(desplines_under_volume_sum);
    desplines_under_volume_sum_view->setEnabled(false);
    views_group.append(std::make_tuple(DESPLINES_SUM_UNDER_VOLUME, desplines_under_volume_sum_view, nullptr));

    auto desplines_until_volume_sum = novel_core->desplinesUntilVolumeRemain();
    auto desplines_until_volume_sum_view = group_desplines_manage_view(desplines_until_volume_sum);
    desplines_until_volume_sum_view->setEnabled(false);
    views_group.append(std::make_tuple(DESPLINES_SUM_UNTIL_VOLUME, desplines_until_volume_sum_view, nullptr));

    auto desplines_until_chapter_sum = novel_core->desplinesUntilChapterRemain();
    auto desplines_until_chapter_sum_view = group_desplines_manage_view(desplines_until_chapter_sum);
    desplines_until_chapter_sum_view->setEnabled(false);
    views_group.append(std::make_tuple(DESPLINES_SUM_UNTIL_CHAPTER, desplines_until_chapter_sum_view, nullptr));

    views_group.append(std::make_tuple(KEYWORDS_MANAGER_VIEW, group_keywords_manager_view(novel_core), nullptr));
    views_group.append(std::make_tuple(KEYWORDS_QUICKLOOK_VIEW, group_keywords_quicklook_view(novel_core->quicklookItemsModel()), nullptr));
    views_group.append(std::make_tuple(SEARCH_RESULT_LABLE_VIEW, group_search_result_summary_panel(), nullptr));

    for(auto tuple : views_group) std::get<1>(tuple)->setVisible(false);
}

QWidget *MainFrame::group_chapters_navigate_view(QAbstractItemModel *model)
{
    auto view = new QTreeView(this);

    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setModel(model);
    connect(view, &QTreeView::expanded,   [view]{
        view->resizeColumnToContents(0);
        view->resizeColumnToContents(1);
    });

    connect(view,         &QTreeView::clicked,                    this,   &MainFrame::chapters_navigate_jump);
    connect(view,         &QTreeView::customContextMenuRequested, this,   &MainFrame::show_chapters_operate);

    return view;
}

QWidget *MainFrame::group_storyblks_navigate_view(QAbstractItemModel *model)
{
    auto view = new QTreeView(this);

    view->setModel(model);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view,     &QTreeView::clicked,                    this,   &MainFrame::outlines_navigate_jump);
    connect(view,     &QTreeView::customContextMenuRequested, this,   &MainFrame::outlines_manipulation);

    return view;
}

QWidget *MainFrame::group_desplines_manage_view(QAbstractItemModel *model)
{
    auto view = new QTreeView(this);

    view->setAlternatingRowColors(true);
    view->setStyleSheet(treeview_style);
    view->setAllColumnsShowFocus(true);

    view->setItemDelegateForColumn(5, new StoryblockRedirect(novel_core));
    view->setModel(model);
    view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(view,   &QWidget::customContextMenuRequested,   [view, this](const QPoint &point){
        QMenu menu(this);

        menu.addAction("添加新支线", this,   &MainFrame::append_despline_from_desplineview);
        menu.addAction("删除支线", [this, view]{this->remove_despline_from_desplineview(view);});
        menu.addSeparator();
        menu.addAction("刷新支线模型", [this, view]{this->refresh_desplineview(view);});
        menu.addSeparator();

        auto index_point = view->indexAt(point);
        if(index_point.isValid()){
            auto index_kind = index_point.sibling(index_point.row(), 0);
            auto node_kind = view->model()->data(index_kind, Qt::UserRole+1).toInt();
            menu.addAction("添加驻点",  [this, view]{this->append_attachpoint_from_desplineview(view);});

            if(node_kind == 2){
                menu.addAction("插入驻点",  [this, view]{this->insert_attachpoint_from_desplineview(view);});
                menu.addAction("移除驻点", [this, view]{this->remove_attachpoint_from_desplineview(view);});

                menu.addSeparator();
                menu.addAction("驻点上移",  [this, view]{this->attachpoint_moveup(view);});
                menu.addAction("驻点下移",  [this, view]{this->attachpoint_movedown(view);});
            }
        }

        menu.exec(view->mapToGlobal(point));
    });

    return view;
}

QWidget *MainFrame::group_keywords_manager_view(NovelHost *novel_core)
{
    auto mgrpanel = new QTabWidget(this);
    mgrpanel->setTabPosition(QTabWidget::West);
    {
        auto *typeSelect = new QComboBox(this);
        // tab-0
        {
            QWidget *panel = new QWidget(this);
            mgrpanel->addTab(panel, "关键词管理");

            auto layout = new QGridLayout(panel);
            layout->setMargin(1);
            layout->setSpacing(1);

            layout->addWidget(typeSelect);

            auto view = new QTreeView(panel);
            layout->addWidget(view, 1, 0, 4, 4);

            view->setSortingEnabled(true);
            view->setWordWrap(true);
            view->setAllColumnsShowFocus(true);
            view->setAlternatingRowColors(true);
            view->setStyleSheet(treeview_style);
            view->setItemDelegate(new ValueAssignDelegate(novel_core, view));

            connect(view, &QTreeView::expanded,   [view]{
                view->resizeColumnToContents(0);
                view->resizeColumnToContents(1);
            });
            connect(typeSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), [typeSelect, view, novel_core]{
                auto index = typeSelect->currentData().toModelIndex();
                if(index == QModelIndex()) return ;

                auto vsel = view->selectionModel();
                view->setModel(novel_core->keywordsModelViaTheList(index));
                delete vsel;
            });




            auto enter = new QLineEdit(panel);
            layout->addWidget(enter, 0, 1, 1, 3);
            enter->setPlaceholderText("键入关键字查询或新建");
            connect(enter,  &QLineEdit::textChanged, [typeSelect, novel_core, view](const QString &str){
                auto mindex = typeSelect->currentData().toModelIndex();
                if(mindex == QModelIndex()) return ;

                WsExcept(novel_core->queryKeywordsViaTheList(mindex, str););

                view->resizeColumnToContents(0);
                view->resizeColumnToContents(1);
            });




            auto addItem = new QPushButton("添加新条目", panel);
            layout->addWidget(addItem, 5, 0, 1, 2);
            connect(addItem,    &QPushButton::clicked,  [typeSelect, novel_core, enter]{
                auto mindex = typeSelect->currentData().toModelIndex();
                if(mindex == QModelIndex()) return ;

                QString name = enter->text();
                WsExcept(novel_core->appendNewItemViaTheList(mindex, name));

                enter->setText("清空");
                enter->setText(name);
            });

            auto removeItem = new QPushButton("移除指定条目", panel);
            layout->addWidget(removeItem, 5, 2, 1, 2);
            connect(removeItem, &QPushButton::clicked,  [view, novel_core, enter, typeSelect]{
                auto mindex = typeSelect->currentData().toModelIndex();
                if(mindex == QModelIndex()) return ;

                auto index = view->currentIndex();
                if(!index.isValid()) return ;

                WsExcept(novel_core->removeTargetItemViaTheList(mindex, index.row()));

                auto temp = enter->text();
                enter->setText("清空");
                enter->setText(temp);
            });
        }


        // tab-1
        QWidget *base = new QWidget(this);
        mgrpanel->addTab(base, "条目配置");
        {
            auto layout = new QGridLayout(base);
            layout->setMargin(3);

            auto table_view = new QTreeView(this);
            table_view->setAlternatingRowColors(true);
            table_view->setAllColumnsShowFocus(true);
            table_view->setStyleSheet(treeview_style);
            connect(table_view, &QTreeView::expanded,   [table_view]{
                table_view->resizeColumnToContents(0);
                table_view->resizeColumnToContents(1);
            });
            table_view->setModel(novel_core->keywordsTypeslistModel());
            layout->addWidget(table_view, 1, 0, 4, 4);
            auto novel_core = this->novel_core;

            auto config_typeName = new QPushButton("重置类型名称", this);
            layout->addWidget(config_typeName, 0, 0, 1, 2);
            connect(config_typeName,    &QPushButton::clicked,  [table_view, novel_core, this, typeSelect]{
                try{
                    auto index = table_view->currentIndex();
                    if(!index.isValid()) return ;
                    if(index.column())
                        index = index.sibling(index.row(), 0);

                    auto name = QInputDialog::getText(this, "重置类型名称", "新类型名称");
                    if(name=="") return ;

                    novel_core->renameKeywordsViaTheList(index, name);

                    for (auto nindex=0; nindex<typeSelect->count();++nindex) {
                        if(typeSelect->itemData(nindex).toModelIndex() == index){
                            typeSelect->setItemText(nindex, name);
                        }
                    }

                    table_view->resizeColumnToContents(0);
                } catch (WsException *e) {
                    QMessageBox::critical(this, "重置类型名称", e->reason());
                }
            });

            auto fieldsConfig = new QPushButton("配置自定义字段", this);
            layout->addWidget(fieldsConfig, 0, 2, 1, 2);
            connect(fieldsConfig,   &QPushButton::clicked,  [table_view, novel_core]{
                auto index = table_view->currentIndex();
                if(!index.isValid()) return ;

                auto fields = novel_core->customedFieldsListViaTheList(index);

                FieldsAdjustDialog dlg(fields, novel_core);
                if(dlg.exec() == QDialog::Rejected)
                    return ;

                dlg.extractFieldsDefine(fields);
                novel_core->adjustKeywordsFieldsViaTheList(index, fields);

                table_view->resizeColumnToContents(0);
                table_view->resizeColumnToContents(1);
            });

            auto addType = new QPushButton("添加新类型", this);
            layout->addWidget(addType, 5, 0, 1, 2);
            connect(addType,    &QPushButton::clicked,  [this, novel_core, table_view, typeSelect]{
                try {
                    auto name = QInputDialog::getText(this, "增加管理类型", "新类型名称");
                    if(name=="") return ;

                    auto view_model = table_view->model();
                    novel_core->appendKeywordsModelToTheList(name);
                    typeSelect->addItem(name, view_model->index(view_model->rowCount()-1, 0));

                    table_view->resizeColumnToContents(1);
                } catch (WsException *e) {
                    QMessageBox::critical(this, "增加管理类型", e->reason());
                }
            });

            auto removeType = new QPushButton("移除指定类型", this);
            layout->addWidget(removeType, 5, 2, 1, 2);
            connect(removeType, &QPushButton::clicked,  [table_view, novel_core, this, typeSelect]{
                try{
                    auto index = table_view->currentIndex();
                    if(!index.isValid()) return;
                    if(index.column())
                        index = index.sibling(index.row(), 0);

                    novel_core->removeKeywordsModelViaTheList(index);

                    if(typeSelect->count() == 1){
                        typeSelect->addItem("无数据", QModelIndex());
                        typeSelect->removeItem(0);
                        return ;
                    }

                    int _temp_idx=0;
                    while (_temp_idx < typeSelect->count() && typeSelect->currentData().toModelIndex() == index) {
                        typeSelect->setCurrentIndex(_temp_idx);
                        _temp_idx++;
                    }
                    for (auto nindex=0; nindex<typeSelect->count(); ++nindex) {
                        if(typeSelect->itemData(nindex).toModelIndex() == index){
                            typeSelect->removeItem(nindex);
                            break;
                        }
                    }
                } catch (WsException *e) {
                    QMessageBox::critical(this, "移除管理类型", e->reason());
                }
            });
        }

        auto model = novel_core->keywordsTypeslistModel();
        for (auto index=0; index<model->rowCount(); ++index) {
            auto ftmidx = model->index(index, 0);
            typeSelect->addItem(ftmidx.data().toString(), ftmidx);
        }
        if(!typeSelect->count())
            typeSelect->addItem("无数据", QModelIndex());

    }

    return mgrpanel;
}

QWidget *MainFrame::group_keywords_quicklook_view(QAbstractItemModel *model)
{
    auto view = new QTreeView(this);

    view->setModel(model);
    view->setAllColumnsShowFocus(true);
    view->setAlternatingRowColors(true);
    view->setStyleSheet(treeview_style);

    connect(model,   &QAbstractItemModel::rowsInserted, [view]{
        view->expandAll();
        view->resizeColumnToContents(0);
        view->resizeColumnToContents(1);
    });

    return view;
}

QWidget *MainFrame::group_search_result_summary_panel()
{
    auto search_pane = new QWidget(this);

    auto search_result_navigate_view(new QTableView(this));            // 搜索结果视图显示
    auto search_text_enter(new QLineEdit(this));                       // 搜索内容键入框
    auto search(new QPushButton("检索", this));
    auto clear(new QPushButton("清除", this));

    auto layout_0 = new QGridLayout(search_pane);
    layout_0->setMargin(0);
    layout_0->setSpacing(2);
    search_result_navigate_view->setModel(novel_core->findResultTable());
    layout_0->addWidget(search_result_navigate_view, 0, 0, 5, 3);
    layout_0->addWidget(search_text_enter, 5, 0, 1, 3);
    layout_0->addWidget(search, 6, 0, 1, 1);
    layout_0->addWidget(clear, 6, 1, 1, 1);

    auto novel_core = this->novel_core;
    connect(search, &QPushButton::clicked,  [search_text_enter, search_result_navigate_view, novel_core]{
        auto text = search_text_enter->text();
        if(!text.length())
            return;

        novel_core->searchText(text);
        search_result_navigate_view->resizeColumnsToContents();
    });
    connect(clear,  &QPushButton::clicked,  [novel_core]{
        novel_core->findResultTable()->clear();
    });
    connect(search_result_navigate_view,    &QTableView::clicked,  [novel_core, this](const QModelIndex &xindex){
        QModelIndex index = xindex;
        if(!index.isValid()) return;

        if(index.column())
            index = index.sibling(index.row(), 0);

        auto target_item = novel_core->findResultTable()->itemFromIndex(index);
        auto chapters_index = target_item->data().toModelIndex();
        auto select_start = target_item->data(Qt::UserRole+2).toInt();
        auto select_len = target_item->data(Qt::UserRole+3).toInt();

        novel_core->setCurrentChaptersNode(chapters_index);
        auto text_cursor = get_active_textedit_view()->textCursor();
        text_cursor.setPosition(select_start);
        text_cursor.setPosition(select_start+select_len, QTextCursor::KeepAnchor);
        get_active_textedit_view()->setTextCursor(text_cursor);
    });

    return search_pane;
}

CQTextEdit *MainFrame::group_textedit_panel(QTextDocument *doc)
{
    auto editor = new CQTextEdit(config, this);
    editor->setDocument(doc);

    return editor;
}

QTextEdit *MainFrame::get_active_textedit_view() const
{
    return active_text_edit_view;
}

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

void MainFrame::acceptSplitterPostionChanged(ViewSplitter *poster, int index, int pos)
{
    qDebug() << "acceptSplitterPostionChanged" << index << pos;
}

void MainFrame::acceptSplitLeftRequest(ViewSelector *poster)
{
    auto basic = centralWidget();
    if(basic == poster){
        takeCentralWidget();
        NEW_VIEWSELECTOR(view);

        NEW_VIEWSPLITTER(splitter, Qt::Horizontal);
        setCentralWidget(splitter);
        splitter->addWidget(view);
        splitter->addWidget(basic);
    }
    else{
        auto parent_splitter = this->find_parent_splitter(static_cast<ViewSplitter*>(basic), poster);
        if(!parent_splitter) throw new WsException("传入了不受管理的节点");

        NEW_VIEWSELECTOR(view);

        auto pos_index = parent_splitter->indexOf(poster);
        auto orientation = parent_splitter->orientation();
        switch (orientation) {
            case Qt::Horizontal:{
                    parent_splitter->insertWidget(pos_index, view);
                }break;
            case Qt::Vertical:
                NEW_VIEWSPLITTER(hsplitter, Qt::Horizontal);
                auto sizes = parent_splitter->sizes();
                parent_splitter->insertWidget(pos_index, hsplitter);
                hsplitter->addWidget(view);
                hsplitter->addWidget(poster);
                parent_splitter->setSizes(sizes);
                break;
        }
    }
}

void MainFrame::acceptSplitRightRequest(ViewSelector *poster)
{
    auto basic = centralWidget();
    if(basic == poster){
        takeCentralWidget();
        NEW_VIEWSELECTOR(view);

        NEW_VIEWSPLITTER(splitter, Qt::Horizontal);
        setCentralWidget(splitter);
        splitter->addWidget(basic);
        splitter->addWidget(view);
    }
    else{
        auto parent_splitter = this->find_parent_splitter(static_cast<ViewSplitter*>(basic), poster);
        if(!parent_splitter) throw new WsException("传入了不受管理的节点");

        NEW_VIEWSELECTOR(view);

        auto pos_index = parent_splitter->indexOf(poster);
        auto orientation = parent_splitter->orientation();
        switch (orientation) {
            case Qt::Horizontal:{
                    parent_splitter->insertWidget(pos_index+1, view);
                }break;
            case Qt::Vertical:
                NEW_VIEWSPLITTER(hsplitter, Qt::Horizontal);
                auto sizes = parent_splitter->sizes();
                parent_splitter->insertWidget(pos_index+1, hsplitter);
                hsplitter->addWidget(poster);
                hsplitter->addWidget(view);
                parent_splitter->setSizes(sizes);
                break;
        }
    }
}

void MainFrame::acceptSplitTopRequest(ViewSelector *poster)
{
    auto basic = centralWidget();
    if(basic == poster){
        takeCentralWidget();
        NEW_VIEWSELECTOR(view);

        NEW_VIEWSPLITTER(splitter, Qt::Vertical);
        setCentralWidget(splitter);
        splitter->addWidget(view);
        splitter->addWidget(basic);
    }
    else{
        auto parent_splitter = this->find_parent_splitter(static_cast<ViewSplitter*>(basic), poster);
        if(!parent_splitter) throw new WsException("传入了不受管理的节点");

        NEW_VIEWSELECTOR(view);

        auto pos_index = parent_splitter->indexOf(poster);
        auto orientation = parent_splitter->orientation();
        switch (orientation) {
            case Qt::Vertical:{
                    parent_splitter->insertWidget(pos_index, view);
                }break;
            case Qt::Horizontal:
                NEW_VIEWSPLITTER(vsplitter, Qt::Vertical);
                auto sizes = parent_splitter->sizes();
                parent_splitter->insertWidget(pos_index, vsplitter);
                vsplitter->addWidget(view);
                vsplitter->addWidget(poster);
                parent_splitter->setSizes(sizes);
                break;
        }
    }
}

void MainFrame::acceptSplitBottomRequest(ViewSelector *poster)
{
    auto basic = centralWidget();
    if(basic == poster){
        takeCentralWidget();
        NEW_VIEWSELECTOR(view);

        NEW_VIEWSPLITTER(splitter, Qt::Vertical);
        setCentralWidget(splitter);
        splitter->addWidget(basic);
        splitter->addWidget(view);
    }
    else{
        auto parent_splitter = this->find_parent_splitter(static_cast<ViewSplitter*>(basic), poster);
        if(!parent_splitter) throw new WsException("传入了不受管理的节点");

        NEW_VIEWSELECTOR(view);

        auto pos_index = parent_splitter->indexOf(poster);
        auto orientation = parent_splitter->orientation();
        switch (orientation) {
            case Qt::Vertical:{
                    parent_splitter->insertWidget(pos_index+1, view);
                }break;
            case Qt::Horizontal:
                NEW_VIEWSPLITTER(vsplitter, Qt::Vertical);
                auto sizes = parent_splitter->sizes();
                parent_splitter->insertWidget(pos_index+1, vsplitter);
                vsplitter->addWidget(poster);
                vsplitter->addWidget(view);
                parent_splitter->setSizes(sizes);
                break;
        }
    }

}

void MainFrame::acceptSplitCancelRequest(ViewSelector *poster)
{
    if(centralWidget() != poster){
        auto parent_splitter = find_parent_splitter(static_cast<ViewSplitter*>(centralWidget()), poster);
        if(parent_splitter->count() == 2){
            QWidget *elwidget = nullptr;
            switch (parent_splitter->indexOf(poster)) {
                case 0:
                    elwidget = parent_splitter->widget(1);
                    break;
                case 1:
                    elwidget = parent_splitter->widget(0);
                    break;
            }
            if(parent_splitter == centralWidget()){
                takeCentralWidget();
                setCentralWidget(elwidget);
            }
            else if(dynamic_cast<ViewFrame*>(elwidget)->viewType() == ViewFrame::FrameType::VIEW_SELECTOR){
                auto pparent_splitter = find_parent_splitter(static_cast<ViewSplitter*>(centralWidget()), parent_splitter);
                auto sizes = pparent_splitter->sizes();
                auto parent_index = pparent_splitter->indexOf(parent_splitter);
                pparent_splitter->insertWidget(parent_index, elwidget);
                pparent_splitter->setSizes(sizes);
            }
            else if(dynamic_cast<ViewFrame*>(elwidget)->viewType() == ViewFrame::FrameType::VIEW_SPLITTER){
                auto pparent_splitter = find_parent_splitter(static_cast<ViewSplitter*>(centralWidget()), parent_splitter);
                auto parent_index = pparent_splitter->indexOf(parent_splitter);
                auto ppsizes = pparent_splitter->sizes();
                auto elsizes = static_cast<ViewSplitter*>(elwidget)->sizes();
                ppsizes.removeAt(parent_index);
                for (auto index = elsizes.size()-1; index >= 0; --index) {
                    ppsizes.insert(parent_index, elsizes.at(index));
                    pparent_splitter->insertWidget(parent_index, static_cast<ViewSplitter*>(elwidget)->widget(index));
                }
                pparent_splitter->setSizes(ppsizes);
            }
            parent_splitter->deleteLater();
        }
    }

    poster->deleteLater();
}

void MainFrame::acceptFocusLockRequest(ViewSelector *poster, bool)
{
    if(centralWidget() == poster) return;

    QList<ViewFrame*> stack;
    stack.append(static_cast<ViewSplitter*>(centralWidget()));

    while (stack.size()) {
        auto item = stack.takeAt(0);
        if(item->viewType() == ViewFrame::FrameType::VIEW_SELECTOR && item != poster)
            static_cast<ViewSelector*>(item)->setFocusLock(false);

        if(item->viewType() == ViewFrame::FrameType::VIEW_SPLITTER){
            auto split_item = static_cast<ViewSplitter*>(item);

            for (auto index=0; index<split_item->count(); ++index) {
                stack.append(dynamic_cast<ViewFrame*>(split_item->widget(index)));
            }
        }
    }
}

void MainFrame::acceptViewCloseRequest(ViewSelector *poster)
{
    qDebug() << "acceptViewCloseRequest";
}

void MainFrame::acceptViewmItemChanged(ViewSelector *poster, const QString &name)
{
    qDebug() << "acceptViewmItemChanged:"<< name;
}

void MainFrame::connect_all_frame_before_present(ViewFrame *root) const
{
    if(root->viewType() == ViewFrame::FrameType::VIEW_SPLITTER){
        auto splitter = static_cast<ViewSplitter*>(root);
        connect(splitter,   &ViewSplitter::splitterPositionMoved,   this,   &MainFrame::acceptSplitterPostionChanged);

        for (int index=0; index<splitter->count(); ++index) {
            connect_all_frame_before_present(dynamic_cast<ViewFrame*>(splitter->widget(index)));
        }
    }
    else if (root->viewType() == ViewFrame::FrameType::VIEW_SELECTOR) {
        auto selector = static_cast<ViewSelector*>(root);
        connect(selector,   &ViewSelector::closeViewRequest,        this,   &MainFrame::acceptViewCloseRequest);
        connect(selector,   &ViewSelector::currentViewItemChanged,  this,   &MainFrame::acceptViewmItemChanged);
    }
}

void MainFrame::disconnect_all_frame_before_hidden(ViewFrame *root) const
{
    if(root->viewType() == ViewFrame::FrameType::VIEW_SPLITTER){
        auto splitter = static_cast<ViewSplitter*>(root);
        disconnect(splitter,   &ViewSplitter::splitterPositionMoved,   this,   &MainFrame::acceptSplitterPostionChanged);

        for (int index=0; index<splitter->count(); ++index) {
            disconnect_all_frame_before_hidden(dynamic_cast<ViewFrame*>(splitter->widget(index)));
        }
    }
    else if (root->viewType() == ViewFrame::FrameType::VIEW_SELECTOR) {
        auto selector = static_cast<ViewSelector*>(root);
        disconnect(selector,   &ViewSelector::closeViewRequest,        this,   &MainFrame::acceptViewCloseRequest);
        disconnect(selector,   &ViewSelector::currentViewItemChanged,  this,   &MainFrame::acceptViewmItemChanged);
    }
}

ViewSplitter *MainFrame::find_parent_splitter(ViewSplitter*parent, ViewFrame *view) const
{
    for (auto index=0; index<parent->count(); ++index) {
        auto widget = dynamic_cast<ViewFrame*>(parent->widget(index));
        if(widget == view) return parent;
    }

    for (auto index=0; index<parent->count(); ++index) {
        auto widget = dynamic_cast<ViewFrame*>(parent->widget(index));
        if(widget->viewType() == ViewFrame::FrameType::VIEW_SPLITTER){
            auto result = find_parent_splitter(static_cast<ViewSplitter*>(widget), view);
            if(result) return result;
        }
    }

    return nullptr;
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
    auto view = static_cast<QTreeView*>(get_view_according_name(DESPLINES_SUM_UNDER_VOLUME));
    view->resizeColumnToContents(0);
    view->resizeColumnToContents(1);
    view->resizeColumnToContents(4);
    view->resizeColumnToContents(5);
    view->resizeColumnToContents(6);

    view = static_cast<QTreeView*>(get_view_according_name(DESPLINES_SUM_UNTIL_VOLUME));
    view->resizeColumnToContents(0);
    view->resizeColumnToContents(1);
    view->resizeColumnToContents(4);
    view->resizeColumnToContents(5);
    view->resizeColumnToContents(6);

    view = static_cast<QTreeView*>(get_view_according_name(DESPLINES_SUM_UNTIL_CHAPTER));
    view->resizeColumnToContents(0);
    view->resizeColumnToContents(1);
    view->resizeColumnToContents(4);
    view->resizeColumnToContents(5);
    view->resizeColumnToContents(6);
}

void MainFrame::chapters_navigate_jump(const QModelIndex &index0)
{
    static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))
            ->resizeColumnToContents(0);

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
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->indexAt(point);

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
                xmenu.addAction("删除", this, &MainFrame::remove_selected_chapters);
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
                            act->setData(attach_point.second);
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
                            act->setData(attach_point.second);
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
    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加卷宗",title, desp) == QDialog::Rejected)
        return;

    try {
        novel_core->insertVolume(title, desp);
    } catch (WsException *e) {
        QMessageBox::critical(this, "增加卷宗", e->reason());
    }
}

void MainFrame::insert_volume()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("插入卷宗",title, desp)==QDialog::Rejected)
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
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加章节",title, desp)==QDialog::Rejected)
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
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("插入章节",title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertChapter(index.parent(), title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入章节", e->reason());
    }
}

void MainFrame::append_despline_from_chapters()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
    if(!index.isValid())
        return;
    if(index.column())
        index = index.sibling(index.row(), 0);

    QString name="键入支线名称", desp0="键入支线总体描述";
    if(getDescription("输入支线信息", name, desp0) == QDialog::Rejected)
        return;

    if(novel_core->indexDepth(index) == 1)
        novel_core->appendDesplineUnder(index, name, desp0);
    else
        novel_core->appendDesplineUnder(index.parent(), name, desp0);

    refresh_desplineview();
}

void MainFrame::pointattach_from_chapter(QAction *item)
{
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
    if(!index.isValid())
        return;

    novel_core->chapterAttachSet(index, item->data().toInt());

    refresh_desplineview();
}

void MainFrame::pointclear_from_chapter(QAction *item)
{
    novel_core->chapterAttachClear(item->data().toInt());

    refresh_desplineview();
}

void MainFrame::remove_selected_chapters()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
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
    auto index = static_cast<QTreeView*>(get_view_according_name(CHAPTERS_NAV_VIEW))->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    QClipboard *x = QApplication::clipboard();
    auto content = novel_core->chapterActiveText(index);
    x->setText(content);
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
                    static_cast<QTextEdit*>(get_view_according_name(VOLUME_OUTLINES_EDIT_VIEW))->setTextCursor(cursor);

                    auto at_value = static_cast<QTextEdit*>(get_view_according_name(VOLUME_OUTLINES_EDIT_VIEW))
                                    ->verticalScrollBar()->value();
                    static_cast<QTextEdit*>(get_view_according_name(VOLUME_OUTLINES_EDIT_VIEW))
                            ->verticalScrollBar()->setValue(at_value + static_cast<QTextEdit*>(get_view_according_name(VOLUME_OUTLINES_EDIT_VIEW))
                                                            ->cursorRect().y());
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
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->indexAt(point);

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
                            action->setData(suspend_point.second);
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
                            action->setData(attached_point.second);
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
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("插入卷宗",title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertVolume(title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入分卷", e->reason());
    }
}

void MainFrame::append_storyblock()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加新剧情",title, desp)==QDialog::Rejected)
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
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("插入新剧情",title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertStoryblock(index.parent(), title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入剧情", e->reason());
    }
}

void MainFrame::append_keypoint()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加分解点",title, desp)==QDialog::Rejected)
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
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("插入分解点",title, desp)==QDialog::Rejected)
        return;

    try {
        novel_core->insertKeypoint(index.parent(), title, desp, index.row());
    } catch (WsException *e) {
        QMessageBox::critical(this, "插入分解点", e->reason());
    }
}

void MainFrame::append_despline_from_outlines()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    if(index.column())
        index = index.sibling(index.row(), 0);

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加新支线",title, desp)==QDialog::Rejected)
        return;

    while (novel_core->indexDepth(index)!=1) {
        index = index.parent();
    }

    novel_core->appendDesplineUnder(index, title, desp);

    refresh_desplineview();
}

void MainFrame::pointattach_from_storyblock(QAction *item)
{
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
    if(!index.isValid())
        return;

    novel_core->storyblockAttachSet(index, item->data().toInt());

    refresh_desplineview();
}

void MainFrame::pointclear_from_storyblock(QAction *item)
{
    novel_core->storyblockAttachClear(item->data().toInt());

    refresh_desplineview();
}

void MainFrame::remove_selected_outlines()
{
    auto index = static_cast<QTreeView*>(get_view_according_name(STORYBLK_NAV_VIEM))->currentIndex();
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
    //chapter_textedit_present->setDocument(empty_document);
}

void MainFrame::documentPresent(QTextDocument *doc, const QString &title)
{
    auto title_novel = novel_core->novelTitle();
    setWindowTitle(title_novel+":"+title);

    for(auto tuple : views_group){
        if(std::get<0>(tuple) == title){
            std::get<2>(tuple)->activePresentView();
            active_text_edit_view = static_cast<CQTextEdit*>(std::get<1>(tuple));
            return;
        }
    }

    active_text_edit_view = group_textedit_panel(doc);
    views_group.append(std::make_tuple(title, active_text_edit_view, nullptr));
    for (auto tuple : views_group) {
        auto stack = std::get<2>(tuple);
        if(stack && stack->isFocusLocked()){
            stack->acceptItemInsert(title);
            stack->setCurrentView(title);
        }
    }
}

void MainFrame::currentChaptersAboutPresent()
{
    get_view_according_name(CHPS_OUTLINES_EDIT_VIEW)->setEnabled(true);

    get_view_according_name(DESPLINES_SUM_UNDER_VOLUME)->setEnabled(true);
    get_view_according_name(DESPLINES_SUM_UNTIL_VOLUME)->setEnabled(true);
    get_view_according_name(DESPLINES_SUM_UNTIL_CHAPTER)->setEnabled(true);
}

void MainFrame::currentVolumeOutlinesPresent()
{
    get_view_according_name(VOLUME_OUTLINES_EDIT_VIEW)->setEnabled(true);

    get_view_according_name(DESPLINES_SUM_UNDER_VOLUME)->setEnabled(true);
    get_view_according_name(DESPLINES_SUM_UNTIL_VOLUME)->setEnabled(true);
}

void MainFrame::convert20_21()
{
    auto source_path = QFileDialog::getOpenFileName(nullptr, "选择2.0描述文件", QDir::homePath(), "NovelStruct(*.nml)",
                                                    nullptr, QFileDialog::DontResolveSymlinks);
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

void MainFrame::append_despline_from_desplineview()
{
    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加新支线",title, desp)==QDialog::Rejected)
        return;

    novel_core->appendDesplineUnderCurrentVolume(title, desp);

    refresh_desplineview();
}

void MainFrame::remove_despline_from_desplineview(QTreeView *view)
{
    auto disp_index = view->currentIndex();
    if(!disp_index.isValid())
        return;

    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    auto id_index = disp_index.sibling(disp_index.row(), 1);

    try {
        novel_core->removeDespline(id_index.data(Qt::UserRole+1).toInt());
        view->model()->removeRow(disp_index.row(), disp_index.parent());
    } catch (WsException *e) {
        QMessageBox::critical(this, "移除支线", e->reason());
    }
}

void MainFrame::append_attachpoint_from_desplineview(QTreeView *view)
{
    auto disp_index = view->currentIndex();
    if(!disp_index.isValid())
        return;

    auto id_index = disp_index.sibling(disp_index.row(), 1);
    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("增加新驻点",title, desp)==QDialog::Rejected)
        return;

    auto type = disp_index.data(Qt::UserRole+1);
    if(type == 1){
        disp_index = view->model()->index(view->model()->rowCount(disp_index)-1, 0);
        novel_core->insertAttachpoint(id_index.data(Qt::UserRole+1).toInt(), title, desp);
    }
    else {
        auto despline_id = disp_index.parent().sibling(disp_index.parent().row(), 1);
        novel_core->insertAttachpoint(despline_id.data(Qt::UserRole+1).toInt(), title, desp);
    }

    refresh_desplineview(view);
}

void MainFrame::insert_attachpoint_from_desplineview(QTreeView *widget)
{
    auto disp_index = widget->currentIndex();
    if(!disp_index.isValid())
        return;

    auto id_index = disp_index.sibling(disp_index.row(), 1);
    if(disp_index.column())
        disp_index = disp_index.sibling(disp_index.row(), 0);

    QString title = "键入名称", desp = "键入描述";
    if(getDescription("插入新驻点",title, desp)==QDialog::Rejected)
        return;

    auto despline_id = disp_index.parent().sibling(disp_index.parent().row(), 1);
    novel_core->insertAttachpoint(despline_id.data(Qt::UserRole+1).toInt(), title, desp, id_index.data().toInt());

    refresh_desplineview(widget);
}

void MainFrame::remove_attachpoint_from_desplineview(QTreeView *widget)
{
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

void MainFrame::attachpoint_moveup(QTreeView *widget)
{
    auto disp_index = widget->currentIndex();
    novel_core->attachPointMoveup(disp_index);
}

void MainFrame::attachpoint_movedown(QTreeView *widget)
{
    auto disp_index = widget->currentIndex();
    novel_core->attachPointMovedown(disp_index);
}

void MainFrame::refresh_desplineview(QTreeView *view)
{
    if(!view || !view->currentIndex().isValid()){
        novel_core->refreshDesplinesSummary();
    }
    else {
        auto disp_index = view->currentIndex();
        auto poslist = extractPositionData(disp_index);
        novel_core->refreshDesplinesSummary();
        scrollToSamePosition(view, poslist);
    }

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

void MainFrame::_structure_output(ViewFrame *base, int deepth) const
{
    QString string = "";
    for (int index=0; index<deepth; ++index)
        string += "  ";

    if(base == nullptr)
        base = dynamic_cast<ViewFrame*>(centralWidget());

    switch (base->viewType()) {
        case ViewFrame::FrameType::VIEW_SELECTOR:
            string += "VIEW_SELECTOR";
            break;
        case ViewFrame::FrameType::VIEW_SPLITTER:
            string += "VIEW_SPLITTER";
            break;
    }

    qDebug() << string;

    if(base->viewType() == ViewFrame::FrameType::VIEW_SPLITTER){
        for (auto index=0; index<static_cast<ViewSplitter*>(base)->count(); ++index) {
            _structure_output(dynamic_cast<ViewFrame*>(static_cast<ViewSplitter*>(base)->widget(index)), deepth+1);
        }
    }
}

int MainFrame::getDescription(const QString &title, QString &nameOut, QString &descriptionOut)
{
    auto dlg = new QDialog(this);
    dlg->setWindowTitle(title);
    auto layout = new QGridLayout(dlg);

    auto nameEnter = new QLineEdit(dlg);
    nameEnter->setPlaceholderText("键入名称");
    layout->addWidget(nameEnter, 0, 0, 1, 2);

    auto descriptionEnter = new QTextEdit(dlg);
    descriptionEnter->setPlaceholderText("键入描述内容");
    layout->addWidget(descriptionEnter, 1, 0, 2, 2);

    auto accept = new QPushButton("确定", dlg);
    layout->addWidget(accept, 3, 0);
    connect(accept, &QPushButton::clicked, [dlg]{
        dlg->accept();
    });

    auto cancel = new QPushButton("取消", dlg);
    layout->addWidget(cancel, 3, 1);
    connect(cancel, &QPushButton::clicked,  [dlg]{
        dlg->reject();
    });

    auto retcode = dlg->exec();
    nameOut = nameEnter->text();
    descriptionOut = descriptionEnter->toPlainText();

    delete dlg;
    return retcode;
}

// ==================================================================================================================================
// ==================================================================================================================================
// ==================================================================================================================================
// ==================================================================================================================================

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

FieldsAdjustDialog::FieldsAdjustDialog(const QList<QPair<int, std::tuple<QString,
                                       QString, DBAccess::KeywordField::ValueType> > > &base, const NovelHost *host)
    :host(host), base(base), view(new QTableView(this)), model(new QStandardItemModel(this)),
      appendItem(new QPushButton("新建条目", this)), removeItem(new QPushButton("移除条目", this)),
      itemMoveUp(new QPushButton("条目上行", this)), itemMoveDown(new QPushButton("条目下行", this)),
      accept_action(new QPushButton("确定更改", this)), reject_action(new QPushButton("取消更改", this))
{
    auto layout = new QGridLayout(this);
    view->setModel(model);

    QList<QPair<QString, QString>> tables;
    host->getAllKeywordsTables(tables);

    model->setHorizontalHeaderLabels(QStringList() << "数据类型"<<"原字段名称"<<"补充值"<<"新字段名称");
    for (auto one : base) {
        QList<QStandardItem*> row;

        switch (std::get<2>(one.second)) {
            case NovelBase::DBAccess::KeywordField::ValueType::NUMBER:{
                    row << new QStandardItem("NUMBER");
                    row << new QStandardItem(std::get<0>(one.second));
                    row << new QStandardItem();
                    row << new QStandardItem(std::get<0>(one.second));
                }break;
            case NovelBase::DBAccess::KeywordField::ValueType::STRING:{
                    row << new QStandardItem("STRING");
                    row << new QStandardItem(std::get<0>(one.second));
                    row << new QStandardItem();
                    row << new QStandardItem(std::get<0>(one.second));
                }break;
            case NovelBase::DBAccess::KeywordField::ValueType::ENUM:{
                    row << new QStandardItem("ENUM");
                    row << new QStandardItem(std::get<0>(one.second));
                    row << new QStandardItem(std::get<1>(one.second));
                    row << new QStandardItem(std::get<0>(one.second));
                }break;
            case NovelBase::DBAccess::KeywordField::ValueType::TABLEREF:{
                    row << new QStandardItem("TABLEREF");
                    row << new QStandardItem(std::get<0>(one.second));

                    auto kwtable_ref = std::get<1>(one.second);
                    for (auto pair : tables) {
                        if(pair.second == kwtable_ref){
                            row << new QStandardItem(pair.first);
                            row.last()->setData(kwtable_ref);
                            break;
                        }
                    }

                    row << new QStandardItem(std::get<0>(one.second));
                }
        }

        row[0]->setEditable(false);
        row[1]->setEditable(false);
        row[2]->setEditable(false);
        row.first()->setData(one.first, Qt::UserRole+1);
        row.first()->setData(static_cast<int>(std::get<2>(one.second)), Qt::UserRole+2);

        model->appendRow(row);
    }

    view->setItemDelegate(new ValueTypeDelegate(host, this));

    layout->addWidget(view, 0,0,6,4);
    layout->addWidget(appendItem, 0, 4);
    layout->addWidget(removeItem, 1, 4);
    layout->addWidget(itemMoveUp, 2, 4);
    layout->addWidget(itemMoveDown, 3, 4);
    layout->addWidget(accept_action, 6, 4);
    layout->addWidget(reject_action, 6, 3);
    layout->setRowStretch(4, 1);
    layout->setRowStretch(5, 1);

    connect(appendItem,     &QPushButton::clicked,  this,   &FieldsAdjustDialog::append_field);
    connect(removeItem,     &QPushButton::clicked,  this,   &FieldsAdjustDialog::remove_field);
    connect(itemMoveUp,     &QPushButton::clicked,  this,   &FieldsAdjustDialog::item_moveup);
    connect(itemMoveDown,   &QPushButton::clicked,  this,   &FieldsAdjustDialog::item_movedown);
    connect(accept_action,  &QPushButton::clicked,  [this]{this->accept();});
    connect(reject_action,  &QPushButton::clicked,  [this]{this->reject();});
}

void FieldsAdjustDialog::extractFieldsDefine(QList<QPair<int, std::tuple<QString, QString,
                                             DBAccess::KeywordField::ValueType>>> &result) const
{
    result.clear();

    for (auto index=0; index<model->rowCount(); ++index) {
        auto mindex0 = model->item(index, 0)->index();
        auto mindex2 = model->item(index, 2)->index();
        auto mindex3 = model->item(index, 3)->index();

        auto id = mindex0.data(Qt::UserRole+1).toInt();
        auto type = static_cast<DBAccess::KeywordField::ValueType>(mindex0.data(Qt::UserRole+2).toInt());

        QString supply_string = mindex2.data().toString();
        switch (type) {
            case DBAccess::KeywordField::ValueType::NUMBER:
            case DBAccess::KeywordField::ValueType::STRING:
                supply_string = "";
                break;
            case DBAccess::KeywordField::ValueType::ENUM:
                break;
            case DBAccess::KeywordField::ValueType::TABLEREF:{
                    supply_string = mindex2.data(Qt::UserRole+1).toString();

                    QList<QPair<QString,QString>> tables;
                    host->getAllKeywordsTables(tables);
                    bool findit = false;
                    for (auto pair : tables) {
                        if(pair.second == supply_string){
                            findit = true;
                        }
                    }

                    if(!findit) throw new WsException("表格名称非法");
                }
                break;
        }

        result << qMakePair(id, std::make_tuple(mindex3.data().toString(), supply_string, type));
    }
}

void FieldsAdjustDialog::append_field()
{
    QList<QStandardItem*> row;
    row << new QStandardItem("STRING");
    row.last()->setData(INT_MAX, Qt::UserRole+1);
    row.last()->setData(static_cast<int>(DBAccess::KeywordField::ValueType::STRING), Qt::UserRole+2);

    row << new QStandardItem("新增条目");
    row.last()->setEditable(false);

    row << new QStandardItem("");
    row << new QStandardItem("新字段");

    model->appendRow(row);
}

void FieldsAdjustDialog::remove_field()
{
    auto index = view->currentIndex();
    if(!index.isValid()) return;

    model->removeRow(index.row());
}

void FieldsAdjustDialog::item_moveup()
{
    auto index = view->currentIndex();
    if(!index.isValid() || !index.row()) return;

    auto row = model->takeRow(index.row());
    model->insertRow(index.row()-1, row);
}

void FieldsAdjustDialog::item_movedown()
{
    auto index = view->currentIndex();
    if(!index.isValid() || index.row()==model->rowCount()-1) return;

    auto row = model->takeRow(index.row());
    model->insertRow(index.row()+1, row);
}


ValueTypeDelegate::ValueTypeDelegate(const NovelHost *host, QObject *object):QStyledItemDelegate (object), host(host){}

QWidget *ValueTypeDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    switch (index.column()) {
        case 0:
            return new QComboBox(parent);
        case 2:{
                auto index0 = index.sibling(index.row(), 0);

                if(index0.data(Qt::UserRole+2).toInt() == static_cast<int>(DBAccess::KeywordField::ValueType::TABLEREF))
                    return new QComboBox(parent);
                return new QLineEdit(parent);
            }
        default:
            return new QLineEdit(parent);
    }
}

void ValueTypeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    switch (index.column()) {
        case 0:{
                auto ed2 = static_cast<QComboBox*>(editor);

                ed2->addItem("NUMBER", static_cast<int>(DBAccess::KeywordField::ValueType::NUMBER));
                ed2->addItem("STRING", static_cast<int>(DBAccess::KeywordField::ValueType::STRING));
                ed2->addItem("ENUM", static_cast<int>(DBAccess::KeywordField::ValueType::ENUM));
                ed2->addItem("TABLEREF", static_cast<int>(DBAccess::KeywordField::ValueType::TABLEREF));
            }break;
        case 2:{
                auto index0 = index.sibling(index.row(), 0);

                if(index0.data(Qt::UserRole+2).toInt() == static_cast<int>(DBAccess::KeywordField::ValueType::TABLEREF)){
                    auto ed2 = static_cast<QComboBox*>(editor);

                    QList<QPair<QString, QString>> tables;
                    host->getAllKeywordsTables(tables);
                    for (auto pair : tables) {
                        ed2->addItem(pair.first, pair.second);
                    }
                    ed2->setCurrentText(index.data().toString());
                }
                else {
                    auto ed2 = static_cast<QLineEdit*>(editor);
                    ed2->setText(index.data().toString());
                }
            }break;
        default:
            auto ed2 = static_cast<QLineEdit*>(editor);
            ed2->setText(index.data().toString());
            break;
    }
}

void ValueTypeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    switch (index.column()) {
        case 0:{
                auto ed2 = static_cast<QComboBox*>(editor);
                model->setData(index, ed2->currentText());
                model->setData(index, ed2->currentData(), Qt::UserRole+2);
            }break;
        case 2:{
                auto index0 = index.sibling(index.row(), 0);

                if(index0.data(Qt::UserRole+2).toInt() == static_cast<int>(DBAccess::KeywordField::ValueType::TABLEREF)){
                    auto ed2 = static_cast<QComboBox*>(editor);
                    model->setData(index, ed2->currentText());
                    model->setData(index, ed2->currentData(), Qt::UserRole+1);
                }
                else if (index0.data(Qt::UserRole+2).toInt() == static_cast<int>(DBAccess::KeywordField::ValueType::ENUM)) {
                    auto value = static_cast<QLineEdit*>(editor)->text();
                    auto list = value.split(";");

                    QString enum_list = "";
                    for (auto index=0; index<list.size(); ++index) {
                        if(list[index].isEmpty()){
                            list.removeAt(index);
                            index--;
                        }
                        else {
                            enum_list += list[index]+";";
                        }
                    }
                    model->setData(index, enum_list.mid(0, enum_list.length()-1));
                }
            }break;
        default:{
                auto ed2 = static_cast<QLineEdit*>(editor);
                model->setData(index, ed2->text());
            }break;
    }
}

void ValueTypeDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}


ValueAssignDelegate::ValueAssignDelegate(const NovelHost *host, QObject *object)
    :QStyledItemDelegate(object), host(host){}

QWidget *ValueAssignDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    switch (index.column()) {
        case 0:
            return new QLineEdit(parent);
        default:{
                auto value_type = index.data(Qt::UserRole+2).toInt();
                switch (static_cast<DBAccess::KeywordField::ValueType>(value_type)) {
                    case DBAccess::KeywordField::ValueType::STRING:
                        return new QLineEdit(parent);
                    case DBAccess::KeywordField::ValueType::NUMBER:
                        return new QDoubleSpinBox(parent);
                    case DBAccess::KeywordField::ValueType::ENUM:
                    case DBAccess::KeywordField::ValueType::TABLEREF:
                        return new QComboBox(parent);
                }
            }break;
    }
}

void ValueAssignDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    switch (index.column()) {
        case 0:{
                auto ed2 = static_cast<QLineEdit*>(editor);
                ed2->setText(index.data().toString());
            }break;
        default:{
                auto value_type = index.data(Qt::UserRole+2).toInt();
                switch (static_cast<DBAccess::KeywordField::ValueType>(value_type)) {
                    case DBAccess::KeywordField::ValueType::STRING:{
                            auto ed2 = static_cast<QLineEdit*>(editor);
                            ed2->setText(index.data(Qt::UserRole+1).toString());
                        }break;
                    case DBAccess::KeywordField::ValueType::NUMBER:{
                            auto ed2 = static_cast<QDoubleSpinBox*>(editor);
                            ed2->setMinimum(-DBL_MAX);
                            ed2->setMaximum(DBL_MAX);
                            ed2->setValue(index.data(Qt::UserRole+1).toDouble());
                        }break;
                    case DBAccess::KeywordField::ValueType::ENUM:{
                            auto ed2 = static_cast<QComboBox*>(editor);
                            auto enums = host->avaliableEnumsForIndex(index);
                            for (auto pair : enums) {
                                ed2->addItem(pair.second, pair.first);
                            }
                            ed2->setCurrentText(index.data().toString());
                        }break;
                    case DBAccess::KeywordField::ValueType::TABLEREF:{
                            auto ed2 = static_cast<QComboBox*>(editor);
                            auto items = host->avaliableItemsForIndex(index);
                            for (auto pair : items) {
                                ed2->addItem(pair.second, pair.first);
                            }
                            ed2->setCurrentText(index.data().toString());
                        }break;
                }
            }break;
    }
}

void ValueAssignDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    switch (index.column()) {
        case 0:{
                auto ed2 = static_cast<QLineEdit*>(editor);
                model->setData(index, ed2->text());
            }break;
        default:{
                auto value_type = index.data(Qt::UserRole+2).toInt();
                switch (static_cast<DBAccess::KeywordField::ValueType>(value_type)) {
                    case DBAccess::KeywordField::ValueType::STRING:{
                            auto ed2 = static_cast<QLineEdit*>(editor);
                            model->setData(index, ed2->text(), Qt::UserRole+1);
                        }break;
                    case DBAccess::KeywordField::ValueType::NUMBER:{
                            auto ed2 = static_cast<QDoubleSpinBox*>(editor);
                            model->setData(index, ed2->value(), Qt::UserRole+1);
                        }break;
                    case DBAccess::KeywordField::ValueType::ENUM:
                    case DBAccess::KeywordField::ValueType::TABLEREF:{
                            auto ed2 = static_cast<QComboBox*>(editor);
                            auto num = ed2->currentData().toInt();
                            model->setData(index, num, Qt::UserRole+1);
                        }break;
                }
            }break;
    }
}

void ValueAssignDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}




StoryblockRedirect::StoryblockRedirect(NovelHost *const host)
    :host(host){}

QWidget *StoryblockRedirect::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    return new QComboBox(parent);
}

void StoryblockRedirect::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto cedit = static_cast<QComboBox*>(editor);
    QList<QPair<QString,int>> key_stories;
    host->allStoryblocksWithIDUnderCurrentVolume(key_stories);
    for (auto xpair : key_stories) {
        cedit->addItem(xpair.first, xpair.second);
    }
    cedit->insertItem(0, "未吸附", QVariant());
    cedit->setCurrentText(index.data().toString());
}

void StoryblockRedirect::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto cedit = static_cast<QComboBox*>(editor);
    model->setData(index, cedit->currentData(), Qt::UserRole+1);
}

void StoryblockRedirect::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}











ViewSelector::ViewSelector(MainFrame *parent)
    : QWidget(parent),
      view_controller(parent),
      view_select(new QComboBox(this)),
      lock_focus(new QPushButton("聚焦")),
      split_view(new QPushButton("分+", this)),
      close_action(new QPushButton("关+", this)),
      place_holder(new QWidget(this))
{
    auto header = new QWidget(this);
    header->setStyleSheet("QPushButton { border:none; background: #ededed;"
                  "margin:0; padding:0; font-size: 12px; min-height:20px;"
                  "min-width:36px; border: 1px solid gray; border-top-color:transparent;"
                  "border-right-color:transparent; }"
                  "QPushButton::menu-indicator{ width:10px; height:10px; "
                  "subcontrol-position: right center; image: url(:/icon/arrow/darrow.png); }"
                  "QPushButton:hover { background : #fafafa; }"
                  "QPushButton:pressed { background:#e0e0e0; }"
                  "QPushButton:checked { background:red; }"

                  "QComboBox { border:none; margin:0; padding:0; font-size: 12px; min-height:20px; "
                  "background : #ededed; border:1px solid transparent; border-bottom-color:gray; }"
                  "QComboBox::drop-down{ border: 1px solid transparent; border-left-color:gray; width : 20px; }"
                  "QComboBox::down-arrow{ width:16px; height:16px; }"
                  "QComboBox::down-arrow:!open{ image: url(:/icon/arrow/darrow.png); }"
                  "QComboBox::down-arrow:open{ image: url(:/icon/arrow/uarrow.png); }"
                  "QComboBox QAbstractItemView {  border: 2px solid darkgray;  background : #ededed;  show-decoration-selected:1; }"
                  "QComboBox QAbstractItemView::item:selected { background:blue; }"
                  "");

    auto hplace = new QHBoxLayout(header);
    hplace->setMargin(0);
    hplace->setSpacing(0);
    hplace->addWidget(view_select, 1);
    hplace->addWidget(lock_focus);
    hplace->addWidget(split_view);
    hplace->addWidget(close_action);

    auto base_layout = new QVBoxLayout(this);
    base_layout->setMargin(0);
    base_layout->setSpacing(0);
    base_layout->addWidget(header);
    base_layout->addWidget(place_holder, 1);

    lock_focus->setCheckable(true);
    place_holder->setStyleSheet("QWidget {background:white;}");
    view_select->setItemDelegate(new QStyledItemDelegate(this));
    view_select->addItem("空视图", QVariant());

    auto place_area = new QStackedLayout(place_holder);
    auto view_select = this->view_select;
    auto view_controllor = this->view_controller;
    connect(view_select,    QOverload<int>::of(&QComboBox::currentIndexChanged), [view_select, view_controllor, this, place_area]{
        auto data = view_select->currentData();
        auto view = view_controllor->locateAndViewlockChangeViaTitlename(this, data.toString());
        if(!view) return;

        if(place_area->count())
            place_area->removeWidget(place_area->currentWidget());

        place_area->addWidget(view);
        view->setFocus();

        emit this->currentViewItemChanged(this, view_select->currentData().toString());
    });
    connect(close_action,  &QPushButton::clicked,  this, &ViewSelector::view_close_operate);
    connect(split_view,  &QPushButton::clicked,  this,   &ViewSelector::view_split_operate);
    connect(lock_focus, &QPushButton::clicked,  [this](bool state){ emit this->focusLockReport(this, state); });

}

ViewSelector::~ViewSelector()
{

}

void ViewSelector::setCurrentView(const QString &name)
{
    for (auto index=0; index<view_select->count(); ++index) {
        if(view_select->itemData(index).isValid() &&
           view_select->itemData(index).toString() == name){

            view_select->setCurrentIndex(index);
            return;
        }
    }
}

QString ViewSelector::currentViewName() const
{
    return view_select->currentText();
}

bool ViewSelector::isFocusLocked() const
{
    auto layout = static_cast<QStackedLayout*>(place_holder->layout());
    return (layout->count() > 0 && lock_focus->isChecked());
}

ViewFrame::FrameType ViewSelector::viewType() const{return FrameType::VIEW_SELECTOR;}

void ViewSelector::acceptItemInsert(const QString &name)
{
    for (auto index=0; index<view_select->count(); ++index) {
        if(view_select->itemData(index).toString() == name)
            return;
    }
    view_select->addItem(name, name);
}

void ViewSelector::acceptItemRemove(const QString &name)
{
    for (auto index=0; index<view_select->count(); ++index) {
        if(view_select->itemData(index).toString() == name){
            view_select->removeItem(index);
            break;
        }
    }
}

void ViewSelector::setFocusLock(bool state)
{
    lock_focus->setChecked(state);
}

void ViewSelector::activePresentView()
{
    auto layout = static_cast<QStackedLayout*>(place_holder->layout());
    if(layout->count())
        layout->widget(layout->count()-1)->setFocus(Qt::FocusReason::TabFocusReason);
}

void ViewSelector::view_split_operate()
{
    QMenu opshow(this);
    opshow.addAction("分割左部空间", [this]{ emit this->splitLeft(this); });
    opshow.addAction("分割右部空间", [this]{ emit this->splitRight(this); });
    opshow.addAction("分割顶部空间", [this]{ emit this->splitTop(this); });
    opshow.addAction("分割底部空间", [this]{ emit this->splitBottom(this); });

    opshow.exec(split_view->mapToGlobal(QPoint(0, split_view->height())));
}

void ViewSelector::view_close_operate()
{
    QMenu opshow(this);
    opshow.addAction("关闭视图", [this]{emit this->closeViewRequest(this);});
    opshow.addAction("取消分割", [this]{emit this->splitCancel(this);});

    opshow.exec(close_action->mapToGlobal(QPoint(0,close_action->height())));
}











ViewSplitter::ViewSplitter(Qt::Orientation ori, QWidget *parent)
    :QSplitter(ori, parent){
    setStyleSheet("QSplitter::handle{background-color:lightgray; }"
                  "QSplitter::handle:horizontal { width: 1px; }"
                  "QSplitter::handle:vertical { height: 1px; }");
    connect(this,   &QSplitter::splitterMoved,  [this](int pos, int index){ emit this->splitterPositionMoved(this, index, pos);});
}

ViewFrame::FrameType ViewSplitter::viewType() const
{return FrameType::VIEW_SPLITTER;}
