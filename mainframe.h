#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "novelhost.h"

#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTextEdit>
#include <QTreeView>

class MainFrame : public QMainWindow
{
    Q_OBJECT

public:
    MainFrame(NovelHost *core, ConfigHost &host, QWidget *parent = nullptr);
    ~MainFrame();

private:
    QTimer *const timer_autosave;
    NovelHost *const novel_core;
    QSplitter *const split_panel;
    QTreeView *const node_navigate_view;
    QTableView *const search_result_view;
    QLineEdit *const search_text_enter;
    QPushButton *const search, *const clear;
    QTextEdit *const text_edit_block;
    QTextDocument *const empty_document;

    QMenu *const file;
    QMenu *const func;

    void rename_novel_title();

    void navigate_jump(const QModelIndex &index);
    void show_manipulation(const QPoint &point);
    void append_volume();
    void append_chapter();
    void remove_selected();
    void content_output();

    void search_text();
    void clear_search_result();
    void search_jump(const QModelIndex &index);

    void saveOp();
    void autosave_timespan_reset();

    void documentOpened(QTextDocument *doc, const QString &title);
    void documentClosed(QTextDocument *);
    void documentActived(QTextDocument *doc, const QString &title);
};

class CQTextEdit : public QTextEdit
{
public:
    CQTextEdit(ConfigHost &config, QWidget *parent=nullptr);

    // QTextEdit interface
protected:
    virtual void insertFromMimeData(const QMimeData *source) override;

private:
    ConfigHost &host;
};

#endif // MAINFRAME_H
