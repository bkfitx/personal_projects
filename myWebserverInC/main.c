#include <stdio.h>  // console input/output, perror
#include <stdlib.h> // exit
#include <string.h> // string manipulation
#include <netdb.h>  // getnameinfo
#include <libgen.h> // dirname
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <dirent.h>
#include <sys/stat.h>

#include <sys/socket.h> // socket APIs
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // open, close

#include <signal.h> // signal handling
#include <time.h>   // time
#include <limits.h> // PATH_MAX

#define PORT 4220
#define BACKLOG 5
#define SIZE 1024
#define MAX_NAME_LEN 256
#define MAX_FILES 1024

typedef struct FileNode {
    char name[MAX_NAME_LEN];
    int is_directory;
    struct FileNode *children[MAX_FILES];
    int children_count;
} FileNode;

struct {
    int done;
    int serverSocket, clientSocket, headerSize;
    char *resBuffer, *resHeader;
} state;

FileNode* create_node(const char *name, int is_directory) {
    FileNode *node = (FileNode *)malloc(sizeof(FileNode));
    strncpy(node->name, name, MAX_NAME_LEN);
    node->is_directory = is_directory;
    node->children_count = 0;
    return node;
}

void free_node(FileNode *node) {
    for (int i = 0; i < node->children_count; i++) {
        free_node(node->children[i]);
    }
    free(node);
}

void read_directory(const char *path, FileNode *parent) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_NAME_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) == -1) {
            perror("stat");
            continue;
        }

        FileNode *node = create_node(entry->d_name, S_ISDIR(statbuf.st_mode));
        parent->children[parent->children_count++] = node;

        if (S_ISDIR(statbuf.st_mode)) {
            read_directory(full_path, node);
        }
    }

    closedir(dir);
}

void print_structure(FileNode *node, int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    printf("%s%s\n", node->name, node->is_directory ? "/" : "");

    for (int i = 0; i < node->children_count; i++) {
        print_structure(node->children[i], depth + 1);
    }
}

void getTimeString(char *buffer) {
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);

    strftime(buffer, 100, "%a, %d %b %Y %H:%M:%S GMT", tm_info);
}

void getMimeType(char *file, char *mime) {
    const char *dot = strrchr(file, '.');

    if (dot == NULL)
        strcpy(mime, "text/html");
    else if (strcmp(dot, ".html") == 0)
        strcpy(mime, "text/html");
    else if (strcmp(dot, ".css") == 0)
        strcpy(mime, "text/css");
    else if (strcmp(dot, ".js") == 0)
        strcpy(mime, "application/js");
    else if (strcmp(dot, ".jpg") == 0)
        strcpy(mime, "image/jpeg");
    else if (strcmp(dot, ".png") == 0)
        strcpy(mime, "image/png");
    else if (strcmp(dot, ".gif") == 0)
        strcpy(mime, "image/gif");
    else
        strcpy(mime, "text/html");
}

void getFileURL(char *route, char *fileURL, int *clientSocket) {
    char *question = strrchr(route, '?');
    if (question)
        *question = '\0';

    // Default to index.html for the root route
    if (strcmp(route, "/") == 0) {
        strcpy(route, "/index.html");
    } else {
        // Check if the route corresponds to a directory
        char tempPath[MAX_NAME_LEN];
        snprintf(tempPath, sizeof(tempPath), "htdocs%s", route);
        struct stat statbuf;
        if (stat(tempPath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            // Append a trailing slash if not present
            if (route[strlen(route) - 1] != '/') {
                strcat(route, "/");
            }
            strcat(route, "display.html");
        } else if (route[strlen(route) - 1] == '/') {
            strcat(route, "index.html");
        }
    }

    strcpy(fileURL, "htdocs");
    strcat(fileURL, route);

    const char *dot = strrchr(fileURL, '.');
    if (!dot || dot == fileURL) {
        strcat(fileURL, ".html");
    }

    printf("Attempting to access file: %s\n", fileURL); // Debug statement

    if (access(fileURL, F_OK) != -1) {
        char mimeType[32];
        getMimeType(fileURL, mimeType);

        char timeBuf[100];
        getTimeString(timeBuf);

        state.resHeader = (char *)malloc(SIZE * sizeof(char));
        if (state.resHeader == NULL) {
            perror("Failed to allocate memory for response header");
            exit(EXIT_FAILURE);
        }

        sprintf(state.resHeader, "HTTP/1.1 200 OK\r\nDate: %s\r\nContent-Type: %s\r\n\n", timeBuf, mimeType);
        state.headerSize = (int)strlen(state.resHeader);

        printf("MIME type: %s\n", mimeType); // Debug statement
    } else {
        perror("Failed to open file");
        const char response[] = "HTTP/1.1 404 Not Found\r\n\n";
        send(state.clientSocket, response, sizeof(response) - 1, 0);
    }
}

void generate_routes(FileNode *node, char *path, char routes[MAX_FILES][MAX_NAME_LEN], int *route_count) {
    if (node->is_directory) {
        for (int i = 0; i < node->children_count; i++) {
            char new_path[MAX_NAME_LEN];
            snprintf(new_path, sizeof(new_path), "%s/%s", path, node->children[i]->name);
            generate_routes(node->children[i], new_path, routes, route_count);
        }
    } else {
        snprintf(routes[*route_count], MAX_NAME_LEN, "%s", path);
        (*route_count)++;
    }
}

void handle_file_upload(const char *boundary, int clientSocket) {
    printf("Handling file upload...\n");
    printf("Boundary: %s\n", boundary);

    char buffer[SIZE];
    char filename[MAX_NAME_LEN] = {0};
    FILE *file = NULL;
    int in_file_data = 0;

    while (1) {
        ssize_t bytes_read = read(clientSocket, buffer, SIZE);
        if (bytes_read <= 0) {
            break;
        }

        if (!in_file_data) {
            // Look for the Content-Disposition header
            const char *content_disposition = "Content-Disposition: form-data; name=\"movieFile\"; filename=\"";
            const char *start = strstr(buffer, content_disposition);
            if (start) {
                start += strlen(content_disposition);
                const char *end = strchr(start, '"');
                if (end) {
                    strncpy(filename, start, end - start);
                    filename[end - start] = '\0';

                    char filepath[MAX_NAME_LEN];
                    snprintf(filepath, sizeof(filepath), "movies/%s", filename);

                    file = fopen(filepath, "wb");
                    if (!file) {
                        perror("Failed to open file for writing");
                        return;
                    }

                    // Move the pointer to the start of the file data
                    start = strstr(end, "\r\n\r\n");
                    if (start) {
                        start += 4; // Skip past "\r\n\r\n"
                        in_file_data = 1;
                        fwrite(start, 1, bytes_read - (start - buffer), file);
                    }
                }
            }
        } else {
            // Look for the boundary to find the end of the file data
            const char *boundary_start = strstr(buffer, boundary);
            if (boundary_start) {
                fwrite(buffer, 1, boundary_start - buffer - 2, file); // -2 to remove trailing "\r\n"
                break;
            } else {
                fwrite(buffer, 1, bytes_read, file);
            }
        }
    }

    if (file) {
        fclose(file);
        printf("File uploaded successfully: %s\n", filename);
    }
}

int main(int argc, const char *argv[]) {
    // Ensure the "movies" directory exists
    struct stat st = {0};
    if (stat("movies", &st) == -1) {
        mkdir("movies", 0700);
    }

    // Change the current working directory to the current one
    char exePath[PATH_MAX];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) != 0) {
        fprintf(stderr, "buffer too small; need size %u\n", size);
        return 1;
    }

    char *dirPath = dirname(exePath);

    if (chdir(dirPath) != 0) {
        perror("chdir() error");
        return 1;
    }

    const char *root_path = "./htdocs"; // Change this to the directory you want to read
    FileNode *root = create_node(root_path, 1);

    read_directory(root_path, root);
    print_structure(root, 0);

    // Generate routes
    char routes[MAX_FILES][MAX_NAME_LEN];
    int route_count = 0;
    generate_routes(root, "", routes, &route_count);

    struct sockaddr_in serverAddress;
    state.done = 0;

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    state.serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(state.serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(state.serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Error: The server is not bound to the address");
        return 1;
    }

    if (listen(state.serverSocket, BACKLOG) < 0) {
        perror("Error: The server is not listening");
        return 1;
    }

    char hostBuffer[NI_MAXHOST], serviceBuffer[NI_MAXSERV];
    int error = getnameinfo((struct sockaddr *)&serverAddress, sizeof(serverAddress), hostBuffer,
                            sizeof(hostBuffer), serviceBuffer, sizeof(serviceBuffer), NI_NUMERICHOST | NI_NUMERICSERV);

    if (error != 0) {
        printf("Error: %s\n", gai_strerror(error));
        return 1;
    }

    printf("\nServer is listening on http://%s:%s/\n\n", hostBuffer, serviceBuffer);

    while (!state.done) {
        char *request = (char *)malloc(SIZE * sizeof(char));
        if (request == NULL) {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }
        char method[10], route[100];

        state.clientSocket = accept(state.serverSocket, NULL, NULL);
        ssize_t request_len = read(state.clientSocket, request, SIZE);

        sscanf(request, "%s %s", method, route);
        printf("%s %s\n", method, route); // Debug statement

        if (strcmp(method, "GET") == 0) {
            printf("Received a GET request for %s\n", route);
            char fileURL[100];

            getFileURL(route, fileURL, &state.clientSocket);

            FILE *file = fopen(fileURL, "r");
            if (file == NULL) {
                perror("Failed to open file");
                free(request);
                close(state.clientSocket);
                continue;
            }

            fseek(file, 0, SEEK_END);
            long fsize = ftell(file);
            fseek(file, 0, SEEK_SET);

            state.resBuffer = (char *)malloc(fsize + state.headerSize);
            if (state.resBuffer == NULL) {
                perror("Failed to allocate memory for response buffer");
                fclose(file);
                free(request);
                close(state.clientSocket);
                continue;
            }

            strcpy(state.resBuffer, state.resHeader);
            char *fileBuffer = state.resBuffer + state.headerSize;
            fread(fileBuffer, fsize, 1, file);

            send(state.clientSocket, state.resBuffer, fsize + state.headerSize, 0);
            free(state.resBuffer);
            fclose(file);
        } else if (strcmp(method, "POST") == 0) {
            printf("Received a POST request for %s\n", route);

            const char *boundary = strstr(request, "boundary=");
            if (boundary) {
                boundary += 9; // Skip past "boundary="
                char boundary_str[100];
                snprintf(boundary_str, sizeof(boundary_str), "--%s", boundary);

                handle_file_upload(boundary_str, state.clientSocket);

                const char response[] = "HTTP/1.1 200 OK\r\n\nFile uploaded successfully.";
                send(state.clientSocket, response, sizeof(response) - 1, 0);
            } else {
                const char response[] = "HTTP/1.1 400 Bad Request\r\n\nInvalid multipart/form-data.";
                send(state.clientSocket, response, sizeof(response) - 1, 0);
            }
        } else {
            printf("Received other request\n");
        }

        free(request);
        close(state.clientSocket);
    }

    close(state.serverSocket);
    free_node(root);
    return 0;
}
