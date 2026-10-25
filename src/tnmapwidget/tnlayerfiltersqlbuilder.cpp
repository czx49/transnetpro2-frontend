/*****************************************************************//**
 * \file   tnlayerfiltersqlbuilder.cpp
 * \brief  图层按属性过滤 — SQL 构建、JSON 与 subset 解析
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "tnlayerfiltersqlbuilder.h"

#include "tnlayerfilterapply.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <functional>

#include <qgsexpression.h>
#include <qgsexpressioncontextutils.h>
#include <qgsfield.h>
#include <qgsvectorlayer.h>

namespace TNLayerFilter {

namespace {

QString quoteColumn(const QString& fieldName)
{
    return QgsExpression::quotedColumnRef(fieldName);
}

QString escapeStringLiteral(const QString& value)
{
    QString v = value;
    v.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(v);
}

QString formatLiteral(const QgsField& field, const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QStringLiteral("NULL");
    }

    const FieldKind kind = fieldKindForField(field);
    if (kind == FieldKind::Numeric || kind == FieldKind::Boolean) {
        if (kind == FieldKind::Boolean) {
            return value.toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
        }
        if (field.type() == QMetaType::Double || field.type() == QMetaType::Float) {
            return QString::number(value.toDouble(), 'g', 15);
        }
        return QString::number(value.toLongLong());
    }

    if (kind == FieldKind::DateTime) {
        if (value.type() == QMetaType::QDateTime) {
            return escapeStringLiteral(value.toDateTime().toString(Qt::ISODate));
        }
        if (value.type() == QMetaType::QDate) {
            return escapeStringLiteral(value.toDate().toString(Qt::ISODate));
        }
        return escapeStringLiteral(value.toString());
    }

    return escapeStringLiteral(value.toString());
}

QString formatLiteralExpression(const QgsField& field, const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QStringLiteral("NULL");
    }

    const FieldKind kind = fieldKindForField(field);
    if (kind == FieldKind::Boolean) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (kind == FieldKind::Numeric) {
        if (field.type() == QMetaType::Double || field.type() == QMetaType::Float) {
            return QString::number(value.toDouble(), 'g', 15);
        }
        return QString::number(value.toLongLong());
    }
    if (kind == FieldKind::DateTime) {
        if (value.type() == QMetaType::QDateTime) {
            return escapeStringLiteral(value.toDateTime().toString(Qt::ISODate));
        }
        if (value.type() == QMetaType::QDate) {
            return escapeStringLiteral(value.toDate().toString(Qt::ISODate));
        }
        return escapeStringLiteral(value.toString());
    }

    return escapeStringLiteral(value.toString());
}

QVector<Clause> collectValidClauses(const QgsVectorLayer* layer, const QVector<Clause>& clauses)
{
    QVector<Clause> valid;
    if (!layer) {
        return valid;
    }

    for (const Clause& c : clauses) {
        if (!c.isValid()) {
            continue;
        }
        if (!SqlBuilder::operatorNeedsValue(c.op)) {
            valid.append(c);
            continue;
        }
        if (SqlBuilder::operatorNeedsTwoValues(c.op)) {
            if (c.value1.isValid() && c.value2.isValid()) {
                valid.append(c);
            }
        } else if (c.value1.isValid()) {
            const FieldKind kind = fieldKindForLayerField(layer, c.fieldName);
            if (kind == FieldKind::Numeric || kind == FieldKind::Boolean || kind == FieldKind::DateTime) {
                valid.append(c);
            } else if (!c.value1.toString().isEmpty()) {
                valid.append(c);
            }
        }
    }
    return valid;
}

QString buildSingleClauseSql(const QgsVectorLayer* layer, const Clause& clause, QString* errorOut)
{
    if (!layer || clause.fieldName.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"子句缺少字段。");
        }
        return {};
    }

    const int fieldIdx = layer->fields().indexFromName(clause.fieldName);
    if (fieldIdx < 0) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"字段不存在：%1").arg(clause.fieldName);
        }
        return {};
    }

    const QgsField field = layer->fields().at(fieldIdx);
    const QString col = quoteColumn(clause.fieldName);

    switch (clause.op) {
    case Operator::IsNull:
        return col + QStringLiteral(" IS NULL");
    case Operator::IsNotNull:
        return col + QStringLiteral(" IS NOT NULL");
    case Operator::Between: {
        if (!clause.value1.isValid() || !clause.value2.isValid()) {
            if (errorOut) {
                *errorOut = QString::fromUtf8(u8"「介于」需要两个值。");
            }
            return {};
        }
        return col + QStringLiteral(" BETWEEN ") + formatLiteral(field, clause.value1) + QStringLiteral(" AND ")
               + formatLiteral(field, clause.value2);
    }
    case Operator::LikeContains:
        return col + QStringLiteral(" LIKE ") + escapeStringLiteral(QStringLiteral("%") + clause.value1.toString() + QStringLiteral("%"));
    case Operator::LikeStarts:
        return col + QStringLiteral(" LIKE ") + escapeStringLiteral(clause.value1.toString() + QStringLiteral("%"));
    case Operator::LikeEnds:
        return col + QStringLiteral(" LIKE ") + escapeStringLiteral(QStringLiteral("%") + clause.value1.toString());
    case Operator::Eq:
        return col + QStringLiteral(" = ") + formatLiteral(field, clause.value1);
    case Operator::Ne:
        return col + QStringLiteral(" <> ") + formatLiteral(field, clause.value1);
    case Operator::Gt:
        return col + QStringLiteral(" > ") + formatLiteral(field, clause.value1);
    case Operator::Ge:
        return col + QStringLiteral(" >= ") + formatLiteral(field, clause.value1);
    case Operator::Lt:
        return col + QStringLiteral(" < ") + formatLiteral(field, clause.value1);
    case Operator::Le:
        return col + QStringLiteral(" <= ") + formatLiteral(field, clause.value1);
    }

    if (errorOut) {
        *errorOut = QString::fromUtf8(u8"不支持的比较方式。");
    }
    return {};
}

QString buildSingleClauseQgsExpression(const QgsVectorLayer* layer, const Clause& clause, QString* errorOut)
{
    if (!layer || clause.fieldName.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"子句缺少字段。");
        }
        return {};
    }

    const int fieldIdx = layer->fields().indexFromName(clause.fieldName);
    if (fieldIdx < 0) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"字段不存在：%1").arg(clause.fieldName);
        }
        return {};
    }

    const QgsField field = layer->fields().at(fieldIdx);
    const QString col = quoteColumn(clause.fieldName);

    switch (clause.op) {
    case Operator::IsNull:
        return col + QStringLiteral(" IS NULL");
    case Operator::IsNotNull:
        return col + QStringLiteral(" IS NOT NULL");
    case Operator::Between: {
        if (!clause.value1.isValid() || !clause.value2.isValid()) {
            if (errorOut) {
                *errorOut = QString::fromUtf8(u8"「介于」需要两个值。");
            }
            return {};
        }
        return col + QStringLiteral(" BETWEEN ") + formatLiteralExpression(field, clause.value1) + QStringLiteral(" AND ")
               + formatLiteralExpression(field, clause.value2);
    }
    case Operator::LikeContains:
        return col + QStringLiteral(" LIKE ") + escapeStringLiteral(QStringLiteral("%") + clause.value1.toString() + QStringLiteral("%"));
    case Operator::LikeStarts:
        return col + QStringLiteral(" LIKE ") + escapeStringLiteral(clause.value1.toString() + QStringLiteral("%"));
    case Operator::LikeEnds:
        return col + QStringLiteral(" LIKE ") + escapeStringLiteral(QStringLiteral("%") + clause.value1.toString());
    case Operator::Eq:
        return col + QStringLiteral(" = ") + formatLiteralExpression(field, clause.value1);
    case Operator::Ne:
        return col + QStringLiteral(" != ") + formatLiteralExpression(field, clause.value1);
    case Operator::Gt:
        return col + QStringLiteral(" > ") + formatLiteralExpression(field, clause.value1);
    case Operator::Ge:
        return col + QStringLiteral(" >= ") + formatLiteralExpression(field, clause.value1);
    case Operator::Lt:
        return col + QStringLiteral(" < ") + formatLiteralExpression(field, clause.value1);
    case Operator::Le:
        return col + QStringLiteral(" <= ") + formatLiteralExpression(field, clause.value1);
    }

    if (errorOut) {
        *errorOut = QString::fromUtf8(u8"不支持的比较方式。");
    }
    return {};
}

QString buildSubsetString(const QgsVectorLayer* layer, const QVector<Clause>& clauses,
                          const std::function<QString(const QgsVectorLayer*, const Clause&, QString*)>& buildClause,
                          QString* errorOut)
{
    if (!layer) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return {};
    }

    const QVector<Clause> valid = collectValidClauses(layer, clauses);
    if (valid.isEmpty()) {
        return {};
    }

    QStringList parts;
    for (int i = 0; i < valid.size(); ++i) {
        const Clause& c = valid.at(i);
        const QString part = buildClause(layer, c, errorOut);
        if (part.isEmpty()) {
            return {};
        }
        if (i > 0) {
            const JoinType j = c.join == JoinType::None ? JoinType::And : c.join;
            parts.append(j == JoinType::Or ? QStringLiteral("OR") : QStringLiteral("AND"));
        }
        parts.append(part);
    }

    return parts.join(QLatin1Char(' '));
}

QVariant variantFromJsonValue(const QJsonValue& v)
{
    if (v.isBool()) {
        return v.toBool();
    }
    if (v.isDouble()) {
        return v.toDouble();
    }
    if (v.isString()) {
        return v.toString();
    }
    return QVariant();
}

QString stripOuterParens(QString s)
{
    s = s.trimmed();
    while (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        int depth = 0;
        bool matched = true;
        for (int i = 0; i < s.size(); ++i) {
            if (s.at(i) == QLatin1Char('(')) {
                ++depth;
            } else if (s.at(i) == QLatin1Char(')')) {
                --depth;
                if (depth == 0 && i != s.size() - 1) {
                    matched = false;
                    break;
                }
            }
        }
        if (!matched) {
            break;
        }
        s = s.mid(1, s.size() - 2).trimmed();
    }
    return s;
}

struct SplitPart
{
    QString text;
    JoinType join = JoinType::None;
};

QVector<SplitPart> splitTopLevel(const QString& subset)
{
    QVector<SplitPart> parts;
    QString current;
  JoinType nextJoin = JoinType::None;
    int depth = 0;
    bool inSingle = false;

    auto flush = [&]() {
        const QString t = current.trimmed();
        if (!t.isEmpty()) {
            SplitPart p;
            p.text = t;
            p.join = nextJoin;
            parts.append(p);
        }
        current.clear();
    };

    const QString upper = subset;
    for (int i = 0; i < subset.size(); ++i) {
        const QChar ch = subset.at(i);
        if (ch == QLatin1Char('\'') && (i == 0 || subset.at(i - 1) != QLatin1Char('\\'))) {
            inSingle = !inSingle;
            current.append(ch);
            continue;
        }
        if (!inSingle) {
            if (ch == QLatin1Char('(')) {
                ++depth;
            } else if (ch == QLatin1Char(')')) {
                --depth;
            } else if (depth == 0) {
                if (i + 4 <= subset.size() && upper.mid(i, 4).compare(QLatin1String(" AND"), Qt::CaseInsensitive) == 0
                    && (i + 4 == subset.size() || upper.at(i + 4).isSpace())) {
                    flush();
                    nextJoin = JoinType::And;
                    i += 3;
                    continue;
                }
                if (i + 3 <= subset.size() && upper.mid(i, 3).compare(QLatin1String(" OR"), Qt::CaseInsensitive) == 0
                    && (i + 3 == subset.size() || upper.at(i + 3).isSpace())) {
                    flush();
                    nextJoin = JoinType::Or;
                    i += 2;
                    continue;
                }
            }
        }
        current.append(ch);
    }
    flush();
    return parts;
}

bool parseOneExpression(const QgsVectorLayer* layer, const QString& expr, Clause* clauseOut, QString* errorOut)
{
    QString e = stripOuterParens(expr);
    if (e.isEmpty()) {
        return false;
    }

    static const QRegularExpression reIsNull(
        QStringLiteral("^\\s*(\"[^\"]+\"|[\\w.]+)\\s+IS\\s+NULL\\s*$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reIsNotNull(
        QStringLiteral("^\\s*(\"[^\"]+\"|[\\w.]+)\\s+IS\\s+NOT\\s+NULL\\s*$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reBetween(
        QStringLiteral("^\\s*(\"[^\"]+\"|[\\w.]+)\\s+BETWEEN\\s+(.+)\\s+AND\\s+(.+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reLike(
        QStringLiteral("^\\s*(\"[^\"]+\"|[\\w.]+)\\s+LIKE\\s+'((?:''|[^'])*)'\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reCmp(
        QStringLiteral("^\\s*(\"[^\"]+\"|[\\w.]+)\\s*(=|<>|!=|>=|<=|>|<)\\s*(.+?)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    auto fieldFromToken = [](const QString& token) -> QString {
        QString f = token.trimmed();
        if (f.startsWith(QLatin1Char('"')) && f.endsWith(QLatin1Char('"'))) {
            f = f.mid(1, f.size() - 2);
        }
        return f;
    };

    auto unquoteLiteral = [](QString lit) -> QVariant {
        lit = lit.trimmed();
        if (lit.compare(QLatin1String("NULL"), Qt::CaseInsensitive) == 0) {
            return QVariant();
        }
        if (lit.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0
            || lit.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (lit.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0
            || lit.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
            return false;
        }
        if (lit.startsWith(QLatin1Char('\'')) && lit.endsWith(QLatin1Char('\''))) {
            lit = lit.mid(1, lit.size() - 2);
            lit.replace(QStringLiteral("''"), QStringLiteral("'"));
            return lit;
        }
        bool ok = false;
        const qlonglong i = lit.toLongLong(&ok);
        if (ok) {
            return i;
        }
        const double d = lit.toDouble(&ok);
        if (ok) {
            return d;
        }
        return lit;
    };

    QRegularExpressionMatch m;

    m = reIsNotNull.match(e);
    if (m.hasMatch()) {
        Clause c;
        c.fieldName = fieldFromToken(m.captured(1));
        c.op = Operator::IsNotNull;
        *clauseOut = c;
        return layer->fields().indexFromName(c.fieldName) >= 0;
    }

    m = reIsNull.match(e);
    if (m.hasMatch()) {
        Clause c;
        c.fieldName = fieldFromToken(m.captured(1));
        c.op = Operator::IsNull;
        *clauseOut = c;
        return layer->fields().indexFromName(c.fieldName) >= 0;
    }

    m = reBetween.match(e);
    if (m.hasMatch()) {
        Clause c;
        c.fieldName = fieldFromToken(m.captured(1));
        c.op = Operator::Between;
        c.value1 = unquoteLiteral(m.captured(2));
        c.value2 = unquoteLiteral(m.captured(3));
        *clauseOut = c;
        return layer->fields().indexFromName(c.fieldName) >= 0;
    }

    m = reLike.match(e);
    if (m.hasMatch()) {
        Clause c;
        c.fieldName = fieldFromToken(m.captured(1));
        QString pattern = m.captured(2);
        pattern.replace(QStringLiteral("''"), QStringLiteral("'"));
        if (pattern.startsWith(QLatin1Char('%')) && pattern.endsWith(QLatin1Char('%')) && pattern.size() >= 2) {
            c.op = Operator::LikeContains;
            c.value1 = pattern.mid(1, pattern.size() - 2);
        } else if (pattern.endsWith(QLatin1Char('%'))) {
            c.op = Operator::LikeStarts;
            c.value1 = pattern.left(pattern.size() - 1);
        } else if (pattern.startsWith(QLatin1Char('%'))) {
            c.op = Operator::LikeEnds;
            c.value1 = pattern.mid(1);
        } else {
            c.op = Operator::Eq;
            c.value1 = pattern;
        }
        *clauseOut = c;
        return layer->fields().indexFromName(c.fieldName) >= 0;
    }

    m = reCmp.match(e);
    if (m.hasMatch()) {
        Clause c;
        c.fieldName = fieldFromToken(m.captured(1));
        const QString opTok = m.captured(2);
        c.value1 = unquoteLiteral(m.captured(3));
        if (opTok == QLatin1String("=")) {
            c.op = Operator::Eq;
        } else if (opTok == QLatin1String("<>") || opTok == QLatin1String("!=")) {
            c.op = Operator::Ne;
        } else if (opTok == QLatin1String(">")) {
            c.op = Operator::Gt;
        } else if (opTok == QLatin1String(">=")) {
            c.op = Operator::Ge;
        } else if (opTok == QLatin1String("<")) {
            c.op = Operator::Lt;
        } else if (opTok == QLatin1String("<=")) {
            c.op = Operator::Le;
        }
        *clauseOut = c;
        return layer->fields().indexFromName(c.fieldName) >= 0;
    }

    if (errorOut) {
        *errorOut = QString::fromUtf8(u8"无法解析表达式：%1").arg(e);
    }
    return false;
}

} // namespace

bool SqlBuilder::operatorNeedsValue(Operator op)
{
    return op != Operator::IsNull && op != Operator::IsNotNull;
}

bool SqlBuilder::operatorNeedsTwoValues(Operator op)
{
    return op == Operator::Between;
}

QString SqlBuilder::buildSql(const QgsVectorLayer* layer, const QVector<Clause>& clauses, QString* errorOut)
{
    return buildSubsetString(layer, clauses, buildSingleClauseSql, errorOut);
}

QString SqlBuilder::buildQgsExpression(const QgsVectorLayer* layer, const QVector<Clause>& clauses, QString* errorOut)
{
    return buildSubsetString(layer, clauses, buildSingleClauseQgsExpression, errorOut);
}

bool SqlBuilder::layerUsesQgsExpressionSubset(const QgsVectorLayer* layer)
{
    if (!layer) {
        return false;
    }
    return layer->providerType().compare(QLatin1String("WFS"), Qt::CaseInsensitive) == 0;
}

QString SqlBuilder::buildForLayer(const QgsVectorLayer* layer, const QVector<Clause>& clauses, QString* errorOut)
{
    if (layerUsesQgsExpressionSubset(layer)) {
        return buildQgsExpression(layer, clauses, errorOut);
    }
    return buildSql(layer, clauses, errorOut);
}

bool SqlBuilder::validateSubsetForLayer(const QgsVectorLayer* layer, const QString& subset, QString* errorOut)
{
    if (!layer) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"图层无效。");
        }
        return false;
    }

    const QString trimmed = subset.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }

    if (!layerUsesQgsExpressionSubset(layer)) {
        return true;
    }

    QgsExpression expression(trimmed);
    if (expression.hasParserError()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"表达式语法错误：%1").arg(expression.parserErrorString());
        }
        return false;
    }

    QgsExpressionContext context;
    context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(layer));
    if (!expression.prepare(&context)) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"表达式无法在当前图层上解析。");
        }
        return false;
    }

    return true;
}

QString SqlBuilder::toJson(const QVector<Clause>& clauses, const QString& subset)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("subset"), subset);

    QJsonArray arr;
    for (const Clause& c : clauses) {
        if (!c.isValid()) {
            continue;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("join"), joinTypeToString(c.join));
        obj.insert(QStringLiteral("field"), c.fieldName);
        obj.insert(QStringLiteral("op"), operatorToCode(c.op));
        QJsonArray vals;
        if (c.value1.isValid()) {
            vals.append(QJsonValue::fromVariant(c.value1));
        }
        if (c.value2.isValid()) {
            vals.append(QJsonValue::fromVariant(c.value2));
        }
        obj.insert(QStringLiteral("values"), vals);
        arr.append(obj);
    }
    root.insert(QStringLiteral("clauses"), arr);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool SqlBuilder::fromJson(const QString& json, PersistedState* stateOut, QString* errorOut)
{
    if (!stateOut) {
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"过滤状态 JSON 无效。");
        }
        return false;
    }

    const QJsonObject root = doc.object();
    PersistedState state;
    state.version = root.value(QStringLiteral("version")).toInt(1);
    state.subset = root.value(QStringLiteral("subset")).toString();

    const QJsonArray arr = root.value(QStringLiteral("clauses")).toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject obj = v.toObject();
        Clause c;
        c.join = joinTypeFromString(obj.value(QStringLiteral("join")).toString());
        c.fieldName = obj.value(QStringLiteral("field")).toString();
        c.op = operatorFromCode(obj.value(QStringLiteral("op")).toString());
        const QJsonArray vals = obj.value(QStringLiteral("values")).toArray();
        if (vals.size() > 0) {
            c.value1 = variantFromJsonValue(vals.at(0));
        }
        if (vals.size() > 1) {
            c.value2 = variantFromJsonValue(vals.at(1));
        }
        if (c.isValid()) {
            state.clauses.append(c);
        }
    }

    *stateOut = state;
    return true;
}

PersistedState SqlBuilder::loadPersistedState(const QgsVectorLayer* layer)
{
    PersistedState state;
    if (!layer) {
        return state;
    }
    const QString json = layer->customProperty(kCustomPropertyKey).toString();
    if (!json.isEmpty()) {
        fromJson(json, &state, nullptr);
    }
    return state;
}

void SqlBuilder::savePersistedState(QgsVectorLayer* layer, const QVector<Clause>& clauses, const QString& subset)
{
    if (!layer) {
        return;
    }
    layer->setCustomProperty(kCustomPropertyKey, toJson(clauses, subset));
}

void SqlBuilder::clearPersistedState(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }
    layer->removeCustomProperty(kCustomPropertyKey);
}

QVector<Clause> SqlBuilder::tryParseSubsetString(const QgsVectorLayer* layer, const QString& subset, QString* errorOut)
{
    QVector<Clause> result;
    if (!layer || subset.trimmed().isEmpty()) {
        return result;
    }

    const QVector<SplitPart> parts = splitTopLevel(subset.trimmed());
    for (const SplitPart& part : parts) {
        Clause c;
        if (!parseOneExpression(layer, part.text, &c, errorOut)) {
            result.clear();
            return result;
        }
        c.join = part.join;
        result.append(c);
    }
    if (!result.isEmpty() && result.first().join != JoinType::None) {
        result[0].join = JoinType::None;
    }
    return result;
}

QVector<Clause> SqlBuilder::clausesForDialogOpen(const QgsVectorLayer* layer, QString* warningOut)
{
    QVector<Clause> empty;
    if (!layer) {
        return empty;
    }

    const QString currentSubset = TNLayerFilter::displayFilterExpression(layer);
    const PersistedState persisted = loadPersistedState(layer);

    if (!persisted.clauses.isEmpty()) {
        const bool subsetMatch = persisted.subset.trimmed() == currentSubset
                                 || (currentSubset.isEmpty() && persisted.subset.trimmed().isEmpty());
        if (subsetMatch) {
            QVector<Clause> valid;
            for (Clause c : persisted.clauses) {
                if (layer->fields().indexFromName(c.fieldName) >= 0) {
                    valid.append(c);
                }
            }
            if (!valid.isEmpty()) {
                return valid;
            }
        } else if (warningOut) {
            *warningOut = QString::fromUtf8(u8"已保存的子句与当前图层过滤不一致，正在尝试从当前 SQL 还原。");
        }
    }

    if (!currentSubset.isEmpty()) {
        const QVector<Clause> parsed = tryParseSubsetString(layer, currentSubset, nullptr);
        if (!parsed.isEmpty()) {
            return parsed;
        }
        if (warningOut) {
            *warningOut = QString::fromUtf8(u8"无法自动还原子句，请重新编辑或清除后重建。当前 SQL 已显示在预览区。");
        }
    }

    return empty;
}

} // namespace TNLayerFilter
