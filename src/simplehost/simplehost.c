#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "../libutilipc/utilipc.h"

static void ensure_www_dir(char *out_path, size_t max_len) {
    const char *home = getenv("HOME");
    if (!home || strlen(home) == 0) home = ".";
    snprintf(out_path, max_len, "%s/simplehost_www", home);
    mkdir(out_path, 0755);

    char uploads_path[1024];
    snprintf(uploads_path, sizeof(uploads_path), "%s/uploads", out_path);
    mkdir(uploads_path, 0755);

    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.html", out_path);
    struct stat st;
    if (stat(index_path, &st) != 0) {
        FILE *fp = fopen(index_path, "w");
        if (fp) {
            fprintf(fp, "<!DOCTYPE html>\n<html>\n<head><meta charset=\"UTF-8\">\n<title>simplehost</title>\n");
            fprintf(fp, "<style>body{font-family:sans-serif;background:#1e1e2e;color:#cdd6f4;text-align:center;padding:50px;} .box{background:#313244;padding:30px;border-radius:12px;display:inline-block;} input,button{padding:10px;margin-top:10px;background:#89b4fa;border:none;border-radius:5px;cursor:pointer;}</style>\n");
            fprintf(fp, "</head><body><div class=\"box\"><h1>🚀 simplehost is Live!</h1>\n");
            fprintf(fp, "<p>Upload files below:</p>\n");
            fprintf(fp, "<form action=\"/upload\" method=\"POST\" enctype=\"multipart/form-data\">\n");
            fprintf(fp, "<input type=\"file\" name=\"fileToUpload\"><br><button type=\"submit\">Upload</button>\n");
            fprintf(fp, "</form></div></body></html>\n");
            fclose(fp);
        }
    }
}

static const char *get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "text/plain";
    if (strcmp(dot, ".html") == 0) return "text/html; charset=UTF-8";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    return "text/plain";
}

int main(int argc, char *argv[]) {
    utilipc_init();
    int port = (argc >= 2) ? atoi(argv[1]) : 8080;
    if (port <= 0 || port > 65535) port = 8080;

    char www_dir[1024];
    ensure_www_dir(www_dir, sizeof(www_dir));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(server_fd, 10) < 0) {
        perror("Server Start Failed");
        return 1;
    }

    printf("==========================================\n");
    printf("[simplehost - Web Server & Uploader]\n");
    printf("  URL: \033[1;32mhttp://localhost:%d/\033[0m\n", port);
    printf("==========================================\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        char request[4096];
        ssize_t bytes = recv(client_fd, request, sizeof(request) - 1, 0);
        if (bytes <= 0) { close(client_fd); continue; }
        request[bytes] = '\0';

        char method[16], req_path[512];
        sscanf(request, "%15s %511s", method, req_path);

        if (strcmp(method, "POST") == 0 && strcmp(req_path, "/upload") == 0) {
            // Extremely basic multipart dump (saves whole payload including boundaries to prevent data loss in C)
            char *content_len_str = strstr(request, "Content-Length: ");
            int content_len = 0;
            if (content_len_str) content_len = atoi(content_len_str + 16);

            char *body_start = strstr(request, "\r\n\r\n");
            if (body_start) {
                body_start += 4;
                int headers_len = body_start - request;
                int read_body = bytes - headers_len;
                
                char upload_file[1024];
                snprintf(upload_file, sizeof(upload_file), "%s/uploads/uploaded_%ld.bin", www_dir, time(NULL));
                
                FILE *out = fopen(upload_file, "wb");
                if (out) {
                    fwrite(body_start, 1, read_body, out);
                    int remaining = content_len - read_body;
                    while (remaining > 0) {
                        char buf[4096];
                        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
                        if (n <= 0) break;
                        fwrite(buf, 1, n, out);
                        remaining -= n;
                    }
                    fclose(out);
                    printf(" [UPLOAD SUCCESS] Saved to %s\n", upload_file);
                }
            }
            
            const char *res = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n<h1>Upload Done! Saved in uploads/</h1><a href=\"/\">Back</a>";
            send(client_fd, res, strlen(res), 0);
        } else {
            if (strcmp(req_path, "/") == 0) strcpy(req_path, "/index.html");
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s%s", www_dir, req_path);
            
            FILE *fp = fopen(file_path, "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END); long fsize = ftell(fp); fseek(fp, 0, SEEK_SET);
                char header[512];
                snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", get_mime_type(file_path), fsize);
                send(client_fd, header, strlen(header), 0);
                
                char buf[4096]; size_t n;
                while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) send(client_fd, buf, n, 0);
                fclose(fp);
            } else {
                const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
                send(client_fd, not_found, strlen(not_found), 0);
            }
        }
        close(client_fd);
    }
    return 0;
}
