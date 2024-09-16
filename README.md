# IndexStream

**IndexStream** is a full-text web search engine designed for fast and efficient web scraping, indexing, and search functionality. The project is written in a combination of **C++** and **Python**, leveraging the strengths of each language to handle different components of the search engine. The primary goal of IndexStream is to crawl web pages, store and index the content, and provide fast, relevant search results using a custom-built indexing and search algorithm.

## Table of Contents
- [Features](#features)
- [Tech Stack](#tech-stack)
- [Architecture](#architecture)
- [Contributing](#contributing)
- [License](#license)

## Features
- **Web Scraping**: Starts from Wikipedia and scrapes outward, respecting `robots.txt`.
- **Indexing**: Builds a term-document matrix using TF-IDF for efficient document retrieval.
- **Multithreaded Search**: Handles concurrent search queries with a custom thread pool implementation.
- **Web Crawler**: Depth-controlled web crawler to prevent looping or excessive scraping of certain domains.
- **Full-Text Search**: Implements TF-IDF for relevance ranking.
- **Persistent Storage**: Indexed data is stored in an SQL database for fast retrieval.

## Tech Stack
- **C++**: Core engine for processing, parsing, indexing web content, and handling the web server.
- **Python**: Web scraper, handling scraping logic and adhering to the `robots.txt` protocol.
- **SQLite**: Lightweight database for storing and indexing web content.

## Architecture

1. **Web Scraper (Python)**:
    - Starts at a base URL (e.g., Wikipedia).
    - Scrapes web pages while respecting `robots.txt`.
    - Stores raw web content for further indexing.

2. **Indexer (C++)**:
    - Processes the raw web data.
    - Extracts URLs, parses documents, and updates the term-document frequency matrix.
    - Calculates **TF-IDF** scores for efficient document retrieval.
    - Stores processed data in an SQLite database.

3. **Web Server (C++)**:
    - Exposes a search interface to users.
    - Handles HTTP requests and passes search queries to the indexer.
    - Retrieves relevant search results from the indexed data and displays them.

4. **Thread Pool**:
    - Manages multiple tasks such as database updates, query handling, and web scraping.
    - Allows efficient concurrent processing without overloading the system.

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.

## License

This project is licensed under the MIT License.
