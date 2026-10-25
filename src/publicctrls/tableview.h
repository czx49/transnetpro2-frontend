#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QDebug>
#include <QTableView>
#include <functional>

class TableView : public QTableView {
    Q_OBJECT
public:
public:
    enum DataReadState {
        Ready,
        Reading,
    };

public:
    explicit TableView(QWidget* parent = nullptr);

public:
    DataReadState dataState() const;

    // 原始虚函数：子类实现具体的数据读取逻辑
    void readData(const QVariantMap& params)
    {
        QString desc;
        paramsValidator(params);
        readDataWithParam(params);
    }

    virtual void paramsValidator(const QVariantMap& params) const = 0;

public slots:
    virtual void onCurrentPageNumberChanged(int pagenumber) { }

protected:
    virtual void readDataWithParam(const QVariantMap& params) = 0;

protected:
    void setDataState(DataReadState state);

signals:
    void dataStateChanged();
    void pageTotalCountChanged(int pageCount);

private:
    DataReadState m_dataState = Ready;
};

#endif // TABLEVIEW_H
