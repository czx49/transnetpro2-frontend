#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Flask-restx based Book Management HTTP Test Server
Provides RESTful API with OpenAPI/Swagger documentation:
  GET    /books
  GET    /books?search=x
  GET    /books/<id>
  POST   /books
  PUT    /bo    @books_ns.doc('create_book')
    @books_ns.expect(book_input_model)
    @books_ns.marshal_with(api_response_model)
    def post(self):<id>
  DELETE /books/<id>
  GET    /health

Swagger UI available at: /doc/
OpenAPI JSON available at: /swagger.json
"""

from datetime import datetime
import time
import threading
import uuid
from flask import Flask
from flask_restx import Api, Resource, fields, reqparse


class Book:
    def __init__(self, title, author, isbn, publish_date=None, book_id=None):
        self.id = book_id or str(uuid.uuid4())
        self.title = title
        self.author = author
        self.isbn = isbn
        self.publish_date = publish_date or datetime.now().strftime("%Y-%m-%d")
        self.created_at = datetime.now().isoformat()
        self.updated_at = datetime.now().isoformat()

    def to_dict(self):
        return {
            "id": self.id,
            "title": self.title,
            "author": self.author,
            "isbn": self.isbn,
            "publish_date": self.publish_date,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }

    def update(self, **kwargs):
        for key, value in kwargs.items():
            if hasattr(self, key) and key != "id":
                setattr(self, key, value)
        self.updated_at = datetime.now().isoformat()


class BookManager:
    def __init__(self):
        self.books = {}
        self.lock = threading.Lock()
        self._init_sample_data()

    def _init_sample_data(self):
        sample_books = [
            Book(
                "Python Programming: From Introduction to Practice",
                "Eric Matthes",
                "978-7-115-42802-8",
                "2016-07-01",
            ),
            Book(
                "Computer Systems: A Programmer's Perspective",
                "Randal E. Bryant",
                "978-7-111-44454-4",
                "2016-01-01",
            ),
            Book(
                "Introduction to Algorithms",
                "Thomas H. Cormen",
                "978-7-111-40701-0",
                "2012-12-01",
            ),
            Book("Design Patterns", "Erich Gamma", "978-7-111-09707-9", "2000-09-01"),
            Book(
                "Refactoring: Improving the Design of Existing Code",
                "Martin Fowler",
                "978-7-115-21708-0",
                "2010-05-01",
            ),
        ]

        for book in sample_books:
            self.books[book.id] = book

    def get_all_books(self):
        with self.lock:
            return [book.to_dict() for book in self.books.values()]

    def get_book(self, book_id):
        with self.lock:
            book = self.books.get(book_id)
            return book.to_dict() if book else None

    def add_book(self, book_data):
        try:
            book = Book(
                title=book_data.get("title"),
                author=book_data.get("author"),
                isbn=book_data.get("isbn"),
                publish_date=book_data.get("publish_date"),
            )
            with self.lock:
                self.books[book.id] = book
            return book.to_dict()
        except Exception as e:
            raise ValueError(f"Failed to create book: {str(e)}")

    def update_book(self, book_id, book_data):
        with self.lock:
            book = self.books.get(book_id)
            if not book:
                return None
            book.update(**book_data)
            return book.to_dict()

    def delete_book(self, book_id):
        with self.lock:
            if book_id in self.books:
                del self.books[book_id]
                return True
            return False

    def search_books(self, query):
        with self.lock:
            results = []
            query_lower = query.lower()
            for book in self.books.values():
                if (
                    query_lower in book.title.lower()
                    or query_lower in book.author.lower()
                    or query_lower in book.isbn.lower()
                ):
                    results.append(book.to_dict())
            return results


# Flask and API setup
app = Flask(__name__)
app.url_map.strict_slashes = False  # 取消严格的斜杠匹配
api = Api(
    app,
    version="1.0",
    title="Book Management API",
    description="A RESTful API for managing books with QRequest2 testing support",
    doc="/doc/",  # Swagger UI endpoint
    contact="QRequest Team",
    contact_email="qrequest@example.com",
)

# Global book manager instance
book_manager = BookManager()
start_time = time.time()

# API Models for documentation
book_model = api.model(
    "Book",
    {
        "id": fields.String(readonly=True, description="Unique book identifier"),
        "title": fields.String(required=True, description="Book title"),
        "author": fields.String(required=True, description="Book author"),
        "isbn": fields.String(required=True, description="ISBN number"),
        "publish_date": fields.String(description="Publication date (YYYY-MM-DD)"),
        "created_at": fields.String(readonly=True, description="Creation timestamp"),
        "updated_at": fields.String(readonly=True, description="Last update timestamp"),
    },
)

book_input_model = api.model(
    "BookInput",
    {
        "title": fields.String(required=True, description="Book title"),
        "author": fields.String(required=True, description="Book author"),
        "isbn": fields.String(required=True, description="ISBN number"),
        "publish_date": fields.String(description="Publication date (YYYY-MM-DD)"),
    },
)

api_response_model = api.model(
    "ApiResponse",
    {
        "status": fields.Integer(description="HTTP status code"),
        "message": fields.String(description="Response message"),
        "timestamp": fields.String(description="Response timestamp"),
        "data": fields.Raw(description="Response data (optional)"),
    },
)

health_data_model = api.model(
    "HealthData",
    {
        "status": fields.String(description="Server status"),
        "server": fields.String(description="Server name"),
        "version": fields.String(description="Server version"),
        "uptime": fields.Float(description="Server uptime in seconds"),
    },
)

# Namespaces
books_ns = api.namespace("books", description="Books operations")
health_ns = api.namespace("health", description="Health check operations")

# Request parsers
books_parser = reqparse.RequestParser()
books_parser.add_argument(
    "search", type=str, help="Search query for books", location="args"
)


@health_ns.route("/")
class Health(Resource):
    @health_ns.doc("health_check")
    @health_ns.marshal_with(api_response_model)
    def get(self):
        """Get server health status"""
        health_data = {
            "status": "healthy",
            "server": "Book Management Server",
            "version": "1.0.0",
            "uptime": time.time() - start_time,
        }
        return {
            "status": 200,
            "message": "Server running normally",
            "timestamp": datetime.now().isoformat(),
            "data": health_data,
        }


@books_ns.route("/")
class BookList(Resource):
    @books_ns.doc("list_books")
    @books_ns.expect(books_parser)
    @books_ns.marshal_with(api_response_model)
    def get(self):
        """Get all books or search books"""
        args = books_parser.parse_args()
        search_query = args.get("search")

        if search_query:
            books = book_manager.search_books(search_query)
            message = f"Found {len(books)} books"
        else:
            books = book_manager.get_all_books()
            message = f"Retrieved {len(books)} books"

        return {
            "status": 200,
            "message": message,
            "timestamp": datetime.now().isoformat(),
            "data": books,
        }

    @books_ns.doc("create_book")
    @books_ns.expect(book_input_model)
    @books_ns.marshal_with(api_response_model)
    def post(self):
        """Create a new book"""
        book_data = api.payload

        # Validate required fields
        required_fields = ["title", "author", "isbn"]
        for field in required_fields:
            if not book_data.get(field):
                api.abort(400, f"Missing required field: {field}")

        try:
            new_book = book_manager.add_book(book_data)
            return {
                "status": 201,
                "message": "Book created successfully",
                "timestamp": datetime.now().isoformat(),
                "data": new_book,
            }, 201
        except ValueError as e:
            api.abort(400, str(e))


@books_ns.route("/<string:book_id>")
@books_ns.param("book_id", "Book identifier")
class BookItem(Resource):
    @books_ns.doc("get_book")
    @books_ns.marshal_with(api_response_model)
    def get(self, book_id):
        """Get a book by ID"""
        book = book_manager.get_book(book_id)
        if book:
            return {
                "status": 200,
                "message": "Book retrieved successfully",
                "timestamp": datetime.now().isoformat(),
                "data": book,
            }
        api.abort(404, "Book not found")

    @books_ns.doc("update_book")
    @books_ns.expect(book_input_model)
    @books_ns.marshal_with(api_response_model)
    def put(self, book_id):
        """Update a book"""
        book_data = api.payload

        updated_book = book_manager.update_book(book_id, book_data)
        if updated_book:
            return {
                "status": 200,
                "message": "Book updated successfully",
                "timestamp": datetime.now().isoformat(),
                "data": updated_book,
            }
        api.abort(404, "Book not found")

    @books_ns.doc("delete_book")
    @books_ns.marshal_with(api_response_model)
    def delete(self, book_id):
        """Delete a book"""
        deleted = book_manager.delete_book(book_id)
        if deleted:
            return {
                "status": 200,
                "message": "Book deleted successfully",
                "timestamp": datetime.now().isoformat(),
            }
        api.abort(404, "Book not found")


def main():
    HOST = "127.0.0.1"
    PORT = 8888

    print(
        f"""
╔══════════════════════════════════════════════════════════════╗
║            Book Management API Server (flask-restx)         ║
║                        Book Server v1.0                     ║
╠══════════════════════════════════════════════════════════════╣
║  Server URL: http://{HOST}:{PORT}                          ║
║  Swagger UI: http://{HOST}:{PORT}/doc/                     ║
║  OpenAPI JSON: http://{HOST}:{PORT}/swagger.json           ║
║                                                              ║
║  API Endpoints:                                             ║
║    GET    /books           - Get all books                  ║
║    GET    /books?search=x  - Search books                   ║
║    GET    /books/{{id}}      - Get specific book              ║
║    POST   /books           - Create new book                ║
║    PUT    /books/{{id}}      - Update book                    ║
║    DELETE /books/{{id}}      - Delete book                    ║
║    GET    /health          - Health check                   ║
║                                                              ║
║  Press Ctrl+C to stop server                                ║
╚══════════════════════════════════════════════════════════════╝
    """
    )

    app.run(host=HOST, port=PORT, debug=True)


if __name__ == "__main__":
    main()
