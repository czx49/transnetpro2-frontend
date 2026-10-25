#include "qrequest2.h"
#include <QCoreApplication>
#include <QDebug>

QRestfulRequest::KnownHeaderMap defaultHeaders()
{
    QRestfulRequest::KnownHeaderMap headers;
    headers[QNetworkRequest::ContentTypeHeader] = "application/json";
    return headers;
}
QJsonObject createTestBook(const QString& title)
{
    QJsonObject book;
    book["title"] = title;
    book["author"] = "QRequest2 Test Author";
    book["isbn"] = "978-0-000-00000-0";
    book["publish_date"] = "2025-09-16";
    return book;
}

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);

    const QJsonObject newBook = createTestBook("QRequest2 Test Book");

    auto callback = [=](bool suc, const QString& msg, const QByteArray& data) {

    };

    // QRequest2::instance().post("http://127.0.0.1:8888/books", callback, newBook.toVariantMap(), defaultHeaders());
    QRequest2::instance().get("http://127.0.0.1:8888/health", callback);

    return a.exec();
}
