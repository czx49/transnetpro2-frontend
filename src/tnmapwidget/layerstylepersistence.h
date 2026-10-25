/*****************************************************************//**
 * \file   layerstylepersistence.h
 * \brief  图层字段符号化规则本地持久化
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#ifndef LAYERSTYLEPERSISTENCE_H
#define LAYERSTYLEPERSISTENCE_H

#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>

class QSqlDatabase;
class QgsVectorLayer;

enum class FieldStyleCategory {
    Invalid = -1,
    Number = 0,
    Bool = 1,
    String = 2
};

class FieldColorRuleItem {
public:
    QString color;
    double min = 0.0;
    double max = 0.0;
    bool boolValue = false;
    QString stringValue;

    QString label(FieldStyleCategory category) const;
    QVariant value(FieldStyleCategory category) const;
};

class FieldColorRule {
public:
    FieldStyleCategory category = FieldStyleCategory::Invalid;
    double opacity = 100.0;
    double lineWidth = 0.8;
    int classLevel = 5;
    QString colorRampName = QStringLiteral("RdYlGn");
    bool rampInverted = false;
    QList<FieldColorRuleItem> colors;

    bool isValid() const;
    void reset();
    QString toJsonString() const;
    static FieldColorRule fromJsonString(const QString& jsonString, QString* errorMessage = nullptr);
};

class LayerStylePersistence : public QObject {
    Q_OBJECT

public:
    static LayerStylePersistence& instance();
    static QString layerStorageKey(QgsVectorLayer* layer);

    bool initDatabase(const QString& dbPath = QString(), QString* errorMessage = nullptr);

    bool saveFieldColorRule(int projectId,
                            int caseId,
                            const QString& layerKey,
                            const QString& fieldName,
                            const FieldColorRule& rule,
                            QString* errorMessage = nullptr);
    bool loadFieldColorRule(int projectId,
                            int caseId,
                            const QString& layerKey,
                            const QString& fieldName,
                            FieldColorRule* rule,
                            QString* errorMessage = nullptr);
    bool loadLatestFieldColorRule(int projectId,
                                  int caseId,
                                  const QString& layerKey,
                                  QString* fieldName,
                                  FieldColorRule* rule,
                                  QString* errorMessage = nullptr);
    bool deleteFieldColorRule(int projectId,
                              int caseId,
                              const QString& layerKey,
                              const QString& fieldName,
                              QString* errorMessage = nullptr);
    bool deleteLayerFieldColorRules(int projectId,
                                    int caseId,
                                    const QString& layerKey,
                                    QString* errorMessage = nullptr);

private:
    explicit LayerStylePersistence(QObject* parent = nullptr);
    ~LayerStylePersistence() override;
    Q_DISABLE_COPY(LayerStylePersistence)

    bool ensureDatabase(QString* errorMessage = nullptr);
    bool executeSchema(QString* errorMessage = nullptr);
    void setError(const QString& error, QString* errorMessage) const;

    QSqlDatabase* m_database = nullptr;
    QString m_dbPath;
    bool m_initialized = false;
};

#endif // LAYERSTYLEPERSISTENCE_H
