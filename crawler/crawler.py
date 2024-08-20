import urllib.robotparser
import requests
from urllib.parse import urlparse, urljoin
from bs4 import BeautifulSoup
import os
import time

class WebCrawler:
    def __init__(self, user_agent='MyCrawler', delay=0.25, dump_dir='../raw_dump'):
        self.user_agent = user_agent
        self.delay = delay
        self.visited_urls = set()
        self.dump_dir = dump_dir
        os.makedirs(self.dump_dir, exist_ok=True)

    def is_allowed_to_crawl(self, url):
        parsed_url = urlparse(url)
        robots_url = urljoin(f"{parsed_url.scheme}://{parsed_url.netloc}", '/robots.txt')
        
        rp = urllib.robotparser.RobotFileParser()
        rp.set_url(robots_url)
        rp.read()
        
        return rp.can_fetch(self.user_agent, url)

    def fetch_page(self, url):
        if self.is_allowed_to_crawl(url):
            try:
                response = requests.get(url, headers={'User-Agent': self.user_agent})
                if response.status_code == 200:
                    return response.text
                else:
                    print(f"Failed to fetch {url}: {response.status_code}")
            except requests.RequestException as e:
                print(f"Request failed: {e}")
        else:
            print(f"Crawling disallowed by robots.txt: {url}")
        return None

    def extract_links(self, page_content, base_url):
        soup = BeautifulSoup(page_content, 'html.parser')
        links = set()
        for link in soup.find_all('a', href=True):
            full_url = urljoin(base_url, link['href'])
            links.add(full_url)
        return links

    def save_page(self, url, content):
        # Create a safe filename from the URL
        safe_filename = url.replace('http://', '').replace('https://', '').replace('/', '_')
        file_path = os.path.join(self.dump_dir, f"{safe_filename}.html")
        
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(f"{url}\n---URL---\n{content}")
        print(f"Saved: {file_path}")

    def crawl(self, seed_url, max_pages=100):
        to_crawl = [seed_url]
        
        while to_crawl and len(self.visited_urls) < max_pages:
            url = to_crawl.pop(0)
            if url not in self.visited_urls:
                print(f"Crawling: {url}")
                page_content = self.fetch_page(url)
                if page_content:
                    self.visited_urls.add(url)
                    self.save_page(url, page_content)
                    new_links = self.extract_links(page_content, url)
                    to_crawl.extend(new_links - self.visited_urls)
                time.sleep(self.delay)

        return self.visited_urls

if __name__ == '__main__':
    seed_url = 'https://en.wikipedia.org/wiki/Main_Page'
    crawler = WebCrawler(user_agent='MyCrawler', delay=1)
    crawled_urls = crawler.crawl(seed_url, max_pages=1000000)
    print("Crawled URLs:")
    for url in crawled_urls:
        print(url)