/*****************************************************************//**
 * \file   aiknowledgebase.cpp
 * \brief  AI 本地 Markdown 知识库实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "aiknowledgebase.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

void AIKnowledgeBase::loadDirectory(const QString& dirPath)
{
    clear();
    if (dirPath.isEmpty()) {
        return;
    }

    QDirIterator it(dirPath, QStringList() << QStringLiteral("*.md"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        Document doc;
        doc.path = path;
        doc.title = QFileInfo(path).completeBaseName();
        doc.content = QString::fromUtf8(file.readAll()).trimmed();
        if (!doc.content.isEmpty()) {
            m_documents.append(doc);
        }
    }
}

void AIKnowledgeBase::clear()
{
    m_documents.clear();
}

bool AIKnowledgeBase::isEmpty() const
{
    return m_documents.isEmpty();
}

QString AIKnowledgeBase::buildContextForQuestion(const QString& question, int maxChars) const
{
    if (m_documents.isEmpty() || question.trimmed().isEmpty() || maxChars <= 0) {
        return QString();
    }

    const QStringList terms = tokenize(question);
    QVector<QPair<int, const Document*>> scored;
    for (const Document& doc : m_documents) {
        const int score = scoreDocument(doc, terms);
        if (score > 0) {
            scored.append(qMakePair(score, &doc));
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
    });

    QStringList snippets;
    int used = 0;
    for (const auto& item : scored) {
        if (used >= maxChars) {
            break;
        }
        const Document* doc = item.second;
        const int remain = maxChars - used;
        const QString part = excerpt(doc->content, terms, qMin(1200, remain));
        if (part.isEmpty()) {
            continue;
        }
        const QString snippet = QStringLiteral("【%1】\n%2").arg(doc->title, part);
        snippets.append(snippet);
        used += snippet.size();
    }

    return snippets.join(QStringLiteral("\n\n"));
}

const QVector<AIKnowledgeBase::Document>& AIKnowledgeBase::documents() const
{
    return m_documents;
}

QStringList AIKnowledgeBase::tokenize(const QString& text)
{
    QString normalized = text.toLower();
    normalized.replace(QRegularExpression(QStringLiteral("[\\p{P}\\p{S}\\s]+")), QStringLiteral(" "));
    QStringList terms = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    terms.removeDuplicates();
    return terms;
}

int AIKnowledgeBase::scoreDocument(const Document& doc, const QStringList& terms)
{
    if (terms.isEmpty()) {
        return 0;
    }

    const QString haystack = (doc.title + QLatin1Char('\n') + doc.content).toLower();
    int score = 0;
    for (const QString& term : terms) {
        if (term.size() < 2) {
            continue;
        }
        if (doc.title.toLower().contains(term)) {
            score += 8;
        }
        if (haystack.contains(term)) {
            score += 2;
        }
    }
    return score;
}

QString AIKnowledgeBase::excerpt(const QString& content, const QStringList& terms, int maxChars)
{
    if (content.isEmpty() || maxChars <= 0) {
        return QString();
    }
    if (content.size() <= maxChars) {
        return content;
    }

    int start = 0;
    const QString lower = content.toLower();
    for (const QString& term : terms) {
        const int pos = lower.indexOf(term.toLower());
        if (pos >= 0) {
            start = qMax(0, pos - maxChars / 3);
            break;
        }
    }

    QString result = content.mid(start, maxChars).trimmed();
    if (start > 0) {
        result.prepend(QStringLiteral("..."));
    }
    if (start + maxChars < content.size()) {
        result.append(QStringLiteral("..."));
    }
    return result;
}
