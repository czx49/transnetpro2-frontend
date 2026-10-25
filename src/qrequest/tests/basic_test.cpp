/**
 * @file basic_test.cpp
 * @brief QRequest2 basic functionality tests - based on local book management HTTP server
 * @details Tests various HTTP methods support of QRequest2 library for book management API
 */

#include <QCoreApplication>
#include <QTest>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include "qrequest2.h"

// Test helper: default headers for requests
static QRequest2::KnownHeaderMap defaultHeaders()
{
    QRequest2::KnownHeaderMap headers;
    headers[QNetworkRequest::ContentTypeHeader] = QVariant(QString("application/json"));
    return headers;
}

class BasicTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Basic connection tests
    void testHealthCheck();
    void testSingleton();

    // GET request tests
    void testGetAllBooks();
    void testGetBookById();
    void testGetBookByIdNotFound();
    void testSearchBooks();

    // POST request tests
    void testCreateBook();
    void testCreateBookInvalidData();

    // PUT request tests
    void testUpdateBook();
    void testUpdateBookNotFound();

    // DELETE request tests
    void testDeleteBook();
    void testDeleteBookNotFound();

    // Synchronous method tests
    void testSyncMethods();

    // Thread safety tests
    void testThreadSafety();

private:
    QString m_serverUrl;
    QString m_testBookId; // Store test book ID for subsequent tests

    // Helper methods
    bool waitForResponse(std::function<void()> requestFunc, int timeoutMs = 5000);
    QJsonObject createTestBook(const QString &title = "Test Book");
};

void BasicTest::initTestCase()
{
    m_serverUrl = "http://127.0.0.1:8888";
    qDebug() << "=== QRequest2 Basic Functionality Tests Started ===";
    qDebug() << "Test server URL:" << m_serverUrl;

    // Wait a moment for the server to fully start
    QThread::msleep(1000);
}

void BasicTest::cleanupTestCase()
{
    qDebug() << "=== QRequest2 Basic Functionality Tests Completed ===";
}

void BasicTest::testHealthCheck()
{
    qDebug() << "\n--- Testing Health Check API ---";

    QEventLoop loop;
    bool completed = false;
    bool success = false;
    QByteArray responseData;

    QRequest2::instance().get(
        m_serverUrl + "/health",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            success = s;
            responseData = data;
            completed = true;
            loop.quit();
            qDebug() << "Health check response:" << (s ? "Success" : "Failed");
            if (s)
            {
                QJsonDocument doc = QJsonDocument::fromJson(data);
                qDebug() << "Response data:" << doc.toJson(QJsonDocument::Compact);
            }
            else
            {
                qDebug() << "Error message:" << message;
            }
        });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    // 验证响应内容
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("data"));

    QJsonObject healthData = obj["data"].toObject();
    QCOMPARE(healthData["status"].toString(), QString("healthy"));
}

void BasicTest::testSingleton()
{
    qDebug() << "\n--- Testing Singleton Pattern ---";

    QRequest2 &instance1 = QRequest2::instance();
    QRequest2 &instance2 = QRequest2::instance();

    QVERIFY(&instance1 == &instance2);
    qDebug() << "Singleton test: Passed";
}

void BasicTest::testGetAllBooks()
{
    qDebug() << "\n--- Testing Get All Books ---";

    QEventLoop loop;
    bool completed = false;
    bool success = false;
    QByteArray responseData;

    QRequest2::instance().get(
        m_serverUrl + "/books",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            success = s;
            responseData = data;
            completed = true;
            loop.quit();
            qDebug() << "Get all books response:" << (s ? "Success" : "Failed");
            if (!s)
            {
                qDebug() << "Error message:" << message;
            }
        });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    // 验证响应数据格式
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("data"));
    QVERIFY(obj["data"].isArray());

    QJsonArray books = obj["data"].toArray();
    qDebug() << "Retrieved" << books.size() << "books";
    QVERIFY(books.size() > 0); // Should have sample data
}

void BasicTest::testGetBookById()
{
    qDebug() << "\n--- Testing Get Book by ID ---";

    // First get all books to obtain a valid ID
    auto response = QRequest2::instance().getSync(m_serverUrl + "/books");
    QVERIFY(response.suc());

    QJsonDocument doc = QJsonDocument::fromJson(response.data());
    QJsonArray books = doc.object()["data"].toArray();
    QVERIFY(books.size() > 0);

    QString bookId = books[0].toObject()["id"].toString();
    qDebug() << "Test book ID:" << bookId;

    QEventLoop loop;
    bool completed = false;
    bool success = false;
    QByteArray responseData;

    QRequest2::instance().get(
        m_serverUrl + "/books/" + bookId,
        [&](bool s, const QString &message, const QByteArray &data)
        {
            success = s;
            responseData = data;
            completed = true;
            loop.quit();
            qDebug() << "Get book by ID response:" << (s ? "Success" : "Failed");
            if (!s)
            {
                qDebug() << "Error message:" << message;
            }
        });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    // Verify the returned book is correct
    doc = QJsonDocument::fromJson(responseData);
    QJsonObject book = doc.object()["data"].toObject();
    QCOMPARE(book["id"].toString(), bookId);
}

void BasicTest::testGetBookByIdNotFound()
{
    qDebug() << "\n--- Testing Get Non-Existent Book ---";

    QEventLoop loop;
    bool completed = false;
    bool success = false;

    QRequest2::instance().get(
        m_serverUrl + "/books/nonexistent-id",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            Q_UNUSED(data)
            success = s;
            completed = true;
            loop.quit();
            qDebug() << "Get non-existent book response:" << (s ? "Success" : "Failed");
            qDebug() << "Message:" << message;
        });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    // Should fail because the book doesn't exist
    QVERIFY(!success);
}

void BasicTest::testSearchBooks()
{
    qDebug() << "\n--- Testing Search Books ---";

    QEventLoop loop;
    bool completed = false;
    bool success = false;
    QByteArray responseData;

    QRequest2::instance().get(
        m_serverUrl + "/books?search=Python",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            success = s;
            responseData = data;
            completed = true;
            loop.quit();
            qDebug() << "Search books response:" << (s ? "Success" : "Failed");
            if (!s)
            {
                qDebug() << "Error message:" << message;
            }
        });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QJsonArray books = doc.object()["data"].toArray();
    qDebug() << "Found" << books.size() << "books";

    // Verify search results contain Python-related content
    if (books.size() > 0)
    {
        QJsonObject book = books[0].toObject();
        QString title = book["title"].toString().toLower();
        QVERIFY(title.contains("python"));
    }
}

void BasicTest::testCreateBook()
{
    qDebug() << "\n--- Testing Create New Book ---";

    QJsonObject newBook = createTestBook("QRequest2 Test Book");

    QEventLoop loop;
    bool completed = false;
    bool success = false;
    QByteArray responseData;

    QRequest2::instance().post(
        m_serverUrl + "/books",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            success = s;
            responseData = data;
            completed = true;
            loop.quit();
            qDebug() << "Create book response:" << (s ? "Success" : "Failed");
            if (!s)
            {
                qDebug() << "Error message:" << message;
            }
        },
        newBook.toVariantMap(), defaultHeaders());

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    // Save the created book ID for subsequent tests
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QJsonObject createdBook = doc.object()["data"].toObject();
    m_testBookId = createdBook["id"].toString();
    qDebug() << "Created book ID:" << m_testBookId;

    QVERIFY(!m_testBookId.isEmpty());
    QCOMPARE(createdBook["title"].toString(), QString("QRequest2 Test Book"));
}

void BasicTest::testCreateBookInvalidData()
{
    qDebug() << "\n--- Testing Create Book (Invalid Data) ---";

    QJsonObject invalidBook;
    invalidBook["title"] = ""; // Missing required fields

    QEventLoop loop;
    bool completed = false;
    bool success = false;

    QRequest2::instance().post(
        m_serverUrl + "/books",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            Q_UNUSED(data)
            success = s;
            completed = true;
            loop.quit();
            qDebug() << "Create invalid book response:" << (s ? "Success" : "Failed");
            qDebug() << "Message:" << message;
        },
        QVariant::fromValue(invalidBook));

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(!success); // Should fail
}

void BasicTest::testUpdateBook()
{
    qDebug() << "\n--- Testing Update Book ---";

    // Ensure there is a book to update
    if (m_testBookId.isEmpty())
    {
        testCreateBook();
    }

    QJsonObject updateData;
    updateData["title"] = "QRequest2 Test Book (Updated)";
    updateData["author"] = "QRequest2 Author (Updated)";

    QEventLoop loop;
    bool completed = false;
    bool success = false;
    QByteArray responseData;

    QRequest2::instance().put(
        m_serverUrl + "/books/" + m_testBookId,
        [&](bool s, const QString &message, const QByteArray &data)
        {
            success = s;
            responseData = data;
            completed = true;
            loop.quit();
            qDebug() << "Update book response:" << (s ? "Success" : "Failed");
            if (!s)
            {
                qDebug() << "Error message:" << message;
            }
        },
        updateData.toVariantMap(), defaultHeaders());

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    // Verify update result
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QJsonObject updatedBook = doc.object()["data"].toObject();
    QCOMPARE(updatedBook["title"].toString(), QString("QRequest2 Test Book (Updated)"));
}

void BasicTest::testUpdateBookNotFound()
{
    qDebug() << "\n--- Testing Update Non-Existent Book ---";

    QJsonObject updateData;
    updateData["title"] = "Non-Existent Book";

    QEventLoop loop;
    bool completed = false;
    bool success = false;

    QRequest2::instance().put(
        m_serverUrl + "/books/nonexistent-id",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            Q_UNUSED(data)
            success = s;
            completed = true;
            loop.quit();
            qDebug() << "Update non-existent book response:" << (s ? "Success" : "Failed");
            qDebug() << "Message:" << message;
        },
        QVariant::fromValue(updateData));

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(!success); // Should fail
}

void BasicTest::testDeleteBook()
{
    qDebug() << "\n--- Testing Delete Book ---";

    // Ensure there is a book to delete
    if (m_testBookId.isEmpty())
    {
        qDebug() << "No test book ID available, creating a new book first...";
        testCreateBook();
    }

    qDebug() << "Deleting book with ID:" << m_testBookId;

    // Verify we have a valid book ID
    QVERIFY(!m_testBookId.isEmpty());

    QEventLoop loop;
    bool completed = false;
    bool success = false;

    QRequest2::instance().remove(
        m_serverUrl + "/books/" + m_testBookId,
        [&](bool s, const QString &message, const QByteArray &data)
        {
            Q_UNUSED(data)
            success = s;
            completed = true;
            loop.quit();
            qDebug() << "Delete book response:" << (s ? "Success" : "Failed");
            if (!s)
            {
                qDebug() << "Error message:" << message;
            }
        },
        QVariant(),
        defaultHeaders());

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(success);

    // Verify the book has been deleted - trying to get it again should fail
    auto response = QRequest2::instance().getSync(m_serverUrl + "/books/" + m_testBookId);
    QVERIFY(!response.suc());
}

void BasicTest::testDeleteBookNotFound()
{
    qDebug() << "\n--- Testing Delete Non-Existent Book ---";

    QEventLoop loop;
    bool completed = false;
    bool success = false;

    QRequest2::instance().remove(
        m_serverUrl + "/books/nonexistent-id",
        [&](bool s, const QString &message, const QByteArray &data)
        {
            Q_UNUSED(data)
            success = s;
            completed = true;
            loop.quit();
            qDebug() << "Delete non-existent book response:" << (s ? "Success" : "Failed");
            qDebug() << "Message:" << message;
        },
        QVariant(),
        defaultHeaders());

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(!success); // Should fail
}

void BasicTest::testSyncMethods()
{
    qDebug() << "\n--- Testing Synchronous Methods ---";

    // Test synchronous GET
    auto getResponse = QRequest2::instance().getSync(m_serverUrl + "/health");
    QVERIFY(getResponse.suc());
    qDebug() << "Synchronous GET test: Passed";

    // Test synchronous POST
    QJsonObject newBook = createTestBook("Sync Test Book");
    auto postResponse = QRequest2::instance().postSync(m_serverUrl + "/books", newBook.toVariantMap(), defaultHeaders());
    QVERIFY(postResponse.suc());
    qDebug() << "Synchronous POST test: Passed";

    // Get the created book ID
    QJsonDocument doc = QJsonDocument::fromJson(postResponse.data());
    QString bookId = doc.object()["data"].toObject()["id"].toString();

    // Test synchronous PUT
    QJsonObject updateData;
    updateData["title"] = "Sync Test Book (Updated)";
    auto putResponse = QRequest2::instance().putSync(m_serverUrl + "/books/" + bookId, updateData.toVariantMap(), defaultHeaders());
    QVERIFY(putResponse.suc());
    qDebug() << "Synchronous PUT test: Passed";

    // Test synchronous DELETE
    auto deleteResponse = QRequest2::instance().removeSync(m_serverUrl + "/books/" + bookId);
    QVERIFY(deleteResponse.suc());
    qDebug() << "Synchronous DELETE test: Passed";
}

void BasicTest::testThreadSafety()
{
    qDebug() << "\n--- Testing Thread Safety ---";

    // Verify different threads use different network managers
    QNetworkAccessManager *mainThreadManager = QRequest2::network();
    QVERIFY(mainThreadManager != nullptr);

    QNetworkAccessManager *otherThreadManager = nullptr;
    QThread *testThread = QThread::create([&]()
                                          { otherThreadManager = QRequest2::network(); });

    testThread->start();
    testThread->wait();
    testThread->deleteLater();

    QVERIFY(otherThreadManager != nullptr);
    QVERIFY(mainThreadManager != otherThreadManager);
    qDebug() << "Thread safety test: Passed - Different threads use different network managers";
}

QJsonObject BasicTest::createTestBook(const QString &title)
{
    QJsonObject book;
    book["title"] = title;
    book["author"] = "QRequest2 Test Author";
    book["isbn"] = "978-0-000-00000-0";
    book["publish_date"] = "2025-09-16";
    return book;
}

bool BasicTest::waitForResponse(std::function<void()> requestFunc, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeoutMs);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start();
    requestFunc();
    loop.exec();

    return !timer.isActive();
}

QTEST_MAIN(BasicTest)
#include "basic_test.moc"
