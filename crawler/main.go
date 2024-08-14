package main

import (
	"fmt"
	"log"
	"os"
	"path/filepath"

	"github.com/gocolly/colly"
)

func main() {
	idx := 0
	c := colly.NewCollector(
		colly.AllowedDomains("hackernoon.com", "www.hackernoon.com"),
		colly.MaxDepth(2),
	)

	visitedLinks := make(map[string]bool)

	dumpDir := "../raw_dump"
	if err := os.MkdirAll(dumpDir, os.ModePerm); err != nil {
		log.Fatalf("Could not create dump directory: %v", err)
	}

	c.OnHTML("a[href]", func(e *colly.HTMLElement) {
		link := e.Request.AbsoluteURL(e.Attr("href"))
		if link == "" {
			link = "https://hackernoon.com/"
		}
		if !visitedLinks[link] {
			visitedLinks[link] = true
			idx++
			fmt.Printf("%v Visiting: %v\n", idx, link)

			c.Visit(link)

			fileName := fmt.Sprintf("'%v'.html", idx)
			filePath := filepath.Join(dumpDir, fileName)
			delimiter := "\n---URL---\n"
			content := link + delimiter + string(e.Response.Body)

			err := os.WriteFile(filePath, []byte(content), 0644)
			if err != nil {
				log.Printf("Error writing file %s: %v", filePath, err)
			} else {
				fmt.Printf("Saved %s\n", filePath)
			}

		}
	})

	err := c.Visit("https://hackernoon.com/c")
	if err != nil {
		log.Fatal(err)
	}
}
