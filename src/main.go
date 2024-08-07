package main

import (
	"fmt"
	"net/http"

	"golang.org/x/net/html"
)

func extractLinks(n *html.Node) []string {
	var links []string
	if n.Type == html.ElementNode && n.Data == "a" {
		for _, attr := range n.Attr {
			if attr.Key == "href" {
				links = append(links, attr.Val)
			}
		}
	}
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		links = append(links, extractLinks(c)...)
	}
	return links
}

func fetch(url string) (*html.Node, error) {
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	doc, err := html.Parse(resp.Body)
	if err != nil {
		return nil, err
	}
	return doc, nil
}

func main() {
	url := "https://en.wikipedia.org/"

	doc, err := fetch(url)
	if err != nil {
		fmt.Println("Error fetching URL:", err)
		return
	}

	// extract all links found in url
	links := extractLinks(doc)

	for index, link := range links {
		fmt.Println(index, " : ", link)
	}
}
