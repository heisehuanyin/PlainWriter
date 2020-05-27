#include "novelhost.h"

#include <QTextCursor>
#include <QTextFrame>
#include <QtDebug>

NovelHost::NovelHost(ConfigHost &config)
    :host(config)
{
    content_presentation = new QTextDocument();
    node_navigate_model = new QStandardItemModel;
    result_enter_model = new QStandardItemModel;

    QTextBlockFormat fb;
    QTextCharFormat fw;
    QTextFrameFormat ff;
    config.novelTitleFormat(fb, fw);
    config.basicFrameFormat(ff);
    content_presentation->rootFrame()->setFrameFormat(ff);
    QTextCursor cur(content_presentation);
    cur.movePosition(QTextCursor::Start);
    ff.setBorderBrush(Qt::blue);
    ff.setBorder(2);
    auto x = cur.insertFrame(ff);
    cur = QTextCursor(x);
    cur.setBlockFormat(fb);
    cur.insertText("小说标题", fw);












    auto nframe = appendVolume(content_presentation, "分卷标题", config);
    auto currr = appendChapter(nframe, "章节标题", config);
    currr.insertText("内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容\n"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容");
    auto currr2 = appendChapter(nframe, "章节标题", config);
    currr2.insertText("内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容\n"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容");
    auto currr3 = appendChapter(nframe, "章节标题", config);
    currr3.insertText("内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容\n"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容"
                     "内容内容内容内容内容内容内容内容内容内容内容内容内容内容内容");

    content_presentation->clearUndoRedoStacks();

    QTextFrame::Iterator it;
    for (it = content_presentation->rootFrame()->begin(); it != content_presentation->rootFrame()->end(); ++it) {
        auto f = it.currentFrame();
        auto b = it.currentBlock();

        if(f){
            qDebug() << "frame";
        }
        else if(b.isValid()){
            qDebug() << "block:" << b.text();
        }
    }
}

QTextDocument *NovelHost::presentModel() const
{
    return content_presentation;
}

QStandardItemModel *NovelHost::navigateModel() const
{
    return node_navigate_model;
}

QStandardItemModel *NovelHost::searchModel() const
{
    return result_enter_model;
}

QTextFrame *NovelHost::appendVolume(QTextDocument *doc, const QString &title, ConfigHost &host)
{
    QTextCursor cur(doc);
    cur.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    QTextFrameFormat ff;
    host.volumeFrameFormat(ff);
    auto volume_group =  cur.insertFrame(ff);

    QTextCursor cur2(volume_group);
    QTextCharFormat fw;
    QTextBlockFormat fb;
    host.volumeTitleFormat(fb, fw);
    cur2.setBlockFormat(fb);
    cur2.insertText(title, fw);

    return volume_group;
}

QTextCursor NovelHost::appendChapter(QTextFrame *volume, const QString &title, ConfigHost &host)
{
    QTextCursor cur = volume->lastCursorPosition();

    QTextFrameFormat ff;
    host.chapterFrameFormat(ff);
    auto chapter_group = cur.insertFrame(ff);

    QTextBlockFormat fb;
    QTextCharFormat fw;
    host.chapterTitleFormat(fb, fw);
    QTextCursor title_pos(chapter_group);
    title_pos.setBlockFormat(fb);
    title_pos.setCharFormat(fw);
    title_pos.insertText(title);

    QTextBlockFormat bf;
    QTextCharFormat cf;
    host.chapterTextFormat(bf, cf);
    auto textpos = chapter_group->lastCursorPosition();
    auto textarea = textpos.insertFrame(QTextFrameFormat());
    QTextCursor textcur(textarea);
    textcur.setCharFormat(cf);
    textcur.setBlockFormat(bf);

    return textcur;
}
