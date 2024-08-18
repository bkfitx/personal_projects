package main

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
)

const (
	PORT         = ":4220"
	MAX_NAME_LEN = 256
	MAX_FILES    = 1024
)

type FileNode struct {
	Name        string
	IsDirectory bool
	Children    []*FileNode
}

func createNode(name string, isDirectory bool) *FileNode {
	return &FileNode{
		Name:        name,
		IsDirectory: isDirectory,
		Children:    []*FileNode{},
	}
}

func readDirectory(path string, parent *FileNode) {
	entries, err := os.ReadDir(path)
	if err != nil {
		fmt.Println("opendir:", err)
		return
	}

	for _, entry := range entries {
		if entry.Name() == "." || entry.Name() == ".." {
			continue
		}

		fullPath := filepath.Join(path, entry.Name())
		info, err := entry.Info()
		if err != nil {
			fmt.Println("stat:", err)
			continue
		}

		node := createNode(entry.Name(), info.IsDir())
		parent.Children = append(parent.Children, node)

		if info.IsDir() {
			readDirectory(fullPath, node)
		}
	}
}

func printStructure(node *FileNode, depth int) {
	fmt.Printf("%s%s\n", strings.Repeat("  ", depth), node.Name)
	if node.IsDirectory {
		fmt.Println("/")
	}

	for _, child := range node.Children {
		printStructure(child, depth+1)
	}
}

func getMimeType(file string) string {
	ext := filepath.Ext(file)
	switch ext {
	case ".html":
		return "text/html"
	case ".css":
		return "text/css"
	case ".js":
		return "application/js"
	case ".jpg":
		return "image/jpeg"
	case ".png":
		return "image/png"
	case ".gif":
		return "image/gif"
	default:
		return "text/html"
	}
}

func getFileURL(route string) string {
	if route == "/" {
		route = "/index.html"
	} else {
		tempPath := filepath.Join("static", route)
		info, err := os.Stat(tempPath)
		if err == nil && info.IsDir() {
			if !strings.HasSuffix(route, "/") {
				route += "/"
			}
			route += "display.html"
		} else if strings.HasSuffix(route, "/") {
			route += "index.html"
		}
	}

	fileURL := filepath.Join("static", route)
	if filepath.Ext(fileURL) == "" {
		fileURL += ".html"
	}

	fmt.Println("Attempting to access file:", fileURL) // Debug statement
	return fileURL
}

func handleFileUpload(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Handling file upload...")

	err := r.ParseMultipartForm(10 << 20) // 10 MB
	if err != nil {
		http.Error(w, "Unable to parse form", http.StatusBadRequest)
		return
	}

	file, handler, err := r.FormFile("movieFile")
	if err != nil {
		http.Error(w, "Unable to retrieve file", http.StatusBadRequest)
		return
	}
	defer file.Close()

	filePath := filepath.Join("movies", handler.Filename)
	out, err := os.Create(filePath)
	if err != nil {
		http.Error(w, "Unable to create file", http.StatusInternalServerError)
		return
	}
	defer out.Close()

	_, err = io.Copy(out, file)
	if err != nil {
		http.Error(w, "Unable to save file", http.StatusInternalServerError)
		return
	}

	fmt.Println("File uploaded successfully:", handler.Filename)
	w.Write([]byte("File uploaded successfully."))
}

func main() {
	// Ensure the "movies" directory exists
	if _, err := os.Stat("movies"); os.IsNotExist(err) {
		os.Mkdir("movies", 0700)
	}

	// Set the working directory explicitly
	projectDir := "/Users/treybertram/Desktop/go" // Replace with the actual path to your project directory
	if err := os.Chdir(projectDir); err != nil {
		fmt.Println("chdir() error:", err)
		return
	}

	cwd, err := os.Getwd()
	if err != nil {
		fmt.Println("Error getting current working directory:", err)
		return
	}
	fmt.Println("Current working directory:", cwd)

	rootPath := "./static"
	root := createNode(rootPath, true)
	readDirectory(rootPath, root)
	printStructure(root, 0)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case "GET":
			fmt.Println("Received a GET request for", r.URL.Path)
			fileURL := getFileURL(r.URL.Path)

			file, err := os.Open(fileURL)
			if err != nil {
				http.Error(w, "File not found", http.StatusNotFound)
				return
			}
			defer file.Close()

			mimeType := getMimeType(fileURL)
			w.Header().Set("Content-Type", mimeType)
			http.ServeFile(w, r, fileURL)
		case "POST":
			fmt.Println("Received a POST request for", r.URL.Path)
			handleFileUpload(w, r)
		default:
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		}
	})

	fmt.Println("Server is listening on http://localhost" + PORT)
	if err := http.ListenAndServe(PORT, nil); err != nil {
		fmt.Println("Error starting server:", err)
	}
}
