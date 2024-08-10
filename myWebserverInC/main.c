//
//  main.c
//  http
//
//  Created by Trey Bertram on 2024-08-09.
//

#include <stdio.h>  // console input/output, perror
#include <stdlib.h> // exit
#include <string.h> // string manipulation
#include <netdb.h>  // getnameinfo
#include <libgen.h> // dirname
#include <mach-o/dyld.h> // _NSGetExecutablePath

#include <sys/socket.h> // socket APIs
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // open, close

#include <signal.h> // signal handling
#include <time.h>   // time
#include <limits.h> // PATH_MAX

#define PORT 4220
#define BACKLOG 5
#define SIZE 1024

struct {
    int done;
    int serverSocket, clientSocket, headerSize;
    char *resBuffer, *resHeader;
} state;

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

    if (route[strlen(route) - 1] == '/') {
        strcat(route, "index.html");
    }

    strcpy(fileURL, "htdocs");
    strcat(fileURL, route);

    const char *dot = strrchr(fileURL, '.');
    if (!dot || dot == fileURL) {
        strcat(fileURL, ".html");
    }

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

        printf(" %s", mimeType);
    } else {
        perror("Failed to open file1");
        const char response[] = "HTTP/1.1 404 Not Found\r\n\n";
        send(state.clientSocket, response, sizeof(response) - 1, 0);
    }
}

int main(int argc, const char *argv[]) {
    //change the current owrking directory to the current one
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
    
    struct sockaddr_in serverAddress;
    state.done = 0;
    /*
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    } else {
        perror("getcwd() error");
        return 1;
    }
     */

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
        read(state.clientSocket, request, SIZE);

        sscanf(request, "%s %s", method, route);
        printf("%s %s", method, route);

        if (strcmp(method, "GET") == 0) {
            printf("Received a GET request for %s\n", route);
            char fileURL[100];

            getFileURL(route, fileURL, &state.clientSocket);

            FILE *file = fopen(fileURL, "r");
            if (file == NULL) {
                perror("Failed to open file2");
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
        } else {
            printf("Received other request\n");
        }

        free(request);
        close(state.clientSocket);
    }

    close(state.serverSocket);
    return 0;
}
