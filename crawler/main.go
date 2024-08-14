package main

import (
	"fmt"
	"log"
	"net/url"
	"os"
	"path/filepath"
	"time"

	"github.com/gocolly/colly"
)

func main() {
	c := colly.NewCollector(
		colly.MaxDepth(3),
		colly.Async(true),
	)

	c.Limit(&colly.LimitRule{
		DomainGlob:  "*",
		Parallelism: 2,
		Delay:       2 * time.Second,
	})

	visitedLinks := make(map[string]bool)
	dumpDir := "../raw_dump"

	if err := os.MkdirAll(dumpDir, os.ModePerm); err != nil {
		log.Fatalf("Could not create dump directory: %v", err)
	}

	c.OnHTML("a[href]", func(e *colly.HTMLElement) {
		link := e.Request.AbsoluteURL(e.Attr("href"))

		if link == "" || !isValidURL(link) {
			return
		}

		if !visitedLinks[link] {
			visitedLinks[link] = true
			fmt.Printf("Visiting: %v\n", link)

			err := c.Visit(link)
			if err != nil {
				log.Printf("Error visiting link %s: %v", link, err)
				return
			}

			fileName := fmt.Sprintf("%d.html", time.Now().UnixNano())
			filePath := filepath.Join(dumpDir, fileName)
			delimiter := "\n---URL---\n"
			content := link + delimiter + string(e.Response.Body)

			err = os.WriteFile(filePath, []byte(content), 0644)
			if err != nil {
				log.Printf("Error writing file %s: %v", filePath, err)
			} else {
				fmt.Printf("Saved %s\n", filePath)
			}
		}
	})

	err := c.Visit("https://en.wikipedia.org/wiki/Main_Page")
	if err != nil {
		log.Fatal(err)
	}

	c.Wait()
}

func isValidURL(link string) bool {
	parsedURL, err := url.Parse(link)
	return err == nil && parsedURL.Scheme != "" && parsedURL.Host != ""
}
