/*****************************************************************//**
 * \file   aiknowledgebase.h
 * \brief  AI 本地 Markdown 知识库
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef AIKNOWLEDGEBASE_H
#define AIKNOWLEDGEBASE_H

#include <QString>
#include <QStringList>
#include <QVector>

class AIKnowledgeBase {
public:
    struct Document {
        QString title;
        QString path;
        QString content;
    };

    void loadDirectory(const QString& dirPath);
    void clear();
    bool isEmpty() const;
    QString buildContextForQuestion(const QString& question, int maxChars = 4000) const;
    const QVector<Document>& documents() const;

private:
    static QStringList tokenize(const QString& text);
    static int scoreDocument(const Document& doc, const QStringList& terms);
    static QString excerpt(const QString& content, const QStringList& terms, int maxChars);

private:
    QVector<Document> m_documents;
};

#endif // AIKNOWLEDGEBASE_H
