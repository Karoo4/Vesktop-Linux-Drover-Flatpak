#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <stdbool.h>

// Configuration structure
typedef struct {
    char *proxy;            // Proxy URL (e.g., http://127.0.0.1:1080 or socks5://127.0.0.1:1080)
    bool use_nekobox_proxy; // Use NekoBox proxy if available
    char *nekobox_proxy;    // NekoBox proxy URL
    bool direct_mode;       // Direct mode for UDP bypass without proxy
} DroverOptions;

// Proxy value structure
typedef struct {
    bool is_specified;
    char *protocol;  // http or socks5
    char *host;
    int port;
    char *login;     // Optional for authenticated proxies
    char *password;
    bool is_http;
    bool is_socks5;
    bool is_auth;
} ProxyValue;

// Global configuration
static DroverOptions config = {0};
static ProxyValue proxy = {0};

// Function pointers for original system calls
static int (*original_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen) = NULL;
static ssize_t (*original_sendto)(int sockfd, const void *buf, size_t len, int flags,
                                 const struct sockaddr *dest_addr, socklen_t addrlen) = NULL;

// Simple INI file parser
static void parse_ini(const char *filename, DroverOptions *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Trim whitespace and newline
        char *end = line + strlen(line) - 1;
        while (end > line && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
        char *start = line;
        while (*start == ' ') start++;

        // Skip empty lines or comments
        if (start[0] == '\0' || start[0] == '#' || start[0] == ';') continue;

        // Check for [drover] section
        if (strncmp(start, "[drover]", 8) == 0) continue;

        // Parse key=value
        char *equals = strchr(start, '=');
        if (!equals) continue;
        *equals = '\0';
        char *key = start;
        char *value = equals + 1;
        while (*value == ' ') value++;

        // Assign values
        if (strcmp(key, "proxy") == 0) {
            config->proxy = strdup(value);
        } else if (strcmp(key, "use-nekobox-proxy") == 0) {
            config->use_nekobox_proxy = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        } else if (strcmp(key, "nekobox-proxy") == 0) {
            config->nekobox_proxy = strdup(value);
        } else if (strcmp(key, "direct-mode") == 0) {
            config->direct_mode = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
    }
    fclose(file);
}

// Parse proxy URL (e.g., http://user:pass@127.0.0.1:1080)
static void parse_proxy(const char *url, ProxyValue *proxy) {
    regex_t regex;
    regmatch_t matches[6];
    const char *pattern = "^(?:([a-z0-9]+)://)?(?:(.+):(.+)@)?(.+):([0-9]+)$";

    proxy->is_specified = false;
    proxy->protocol = NULL;
    proxy->host = NULL;
    proxy->port = 0;
    proxy->login = NULL;
    proxy->password = NULL;
    proxy->is_http = false;
    proxy->is_socks5 = false;
    proxy->is_auth = false;

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE) != 0) {
        fprintf(stderr, "Failed to compile regex\n");
        return;
    }

    if (regexec(&regex, url, 6, matches, 0) == 0) {
        proxy->is_specified = true;

        // Protocol
        if (matches[1].rm_so != -1) {
            int len = matches[1].rm_eo - matches[1].rm_so;
            proxy->protocol = strndup(url + matches[1].rm_so, len);
            if (strcmp(proxy->protocol, "https") == 0) {
                free(proxy->protocol);
                proxy->protocol = strdup("http");
            }
        } else {
            proxy->protocol = strdup("http");
        }
        proxy->is_http = strcmp(proxy->protocol, "http") == 0;
        proxy->is_socks5 = strcmp(proxy->protocol, "socks5") == 0;

        // Login and password
        if (matches[2].rm_so != -1) {
            int len = matches[2].rm_eo - matches[2].rm_so;
            proxy->login = strndup(url + matches[2].rm_so, len);
            len = matches[3].rm_eo - matches[3].rm_so;
            proxy->password = strndup(url + matches[3].rm_so, len);
            proxy->is_auth = (proxy->login[0] != '\0') && (proxy->password[0] != '\0');
        }

        // Host
        if (matches[4].rm_so != -1) {
            int len = matches[4].rm_eo - matches[4].rm_so;
            proxy->host = strndup(url + matches[4].rm_so, len);
        }

        // Port
        if (matches[5].rm_so != -1) {
            char port_str[16] = {0};
            int len = matches[5].rm_eo - matches[5].rm_so;
            strncpy(port_str, url + matches[5].rm_so, len);
            proxy->port = atoi(port_str);
        }
    }

    regfree(&regex);
}

// Load configuration from drover.ini
static void load_config(const char *filename) {
    config.proxy = strdup("");
    config.use_nekobox_proxy = false;
    config.nekobox_proxy = strdup("http://127.0.0.1:2080");
    config.direct_mode = false;

    parse_ini(filename, &config);

    // Parse proxy settings
    if (config.proxy && strlen(config.proxy) > 0) {
        parse_proxy(config.proxy, &proxy);
    } else if (config.use_nekobox_proxy && config.nekobox_proxy) {
        parse_proxy(config.nekobox_proxy, &proxy);
    }
}

// Connect to proxy server (HTTP CONNECT or SOCKS5)
static int connect_to_proxy(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!proxy.is_specified || config.direct_mode) {
        return original_connect(sockfd, addr, addrlen);
    }

    struct sockaddr_in proxy_addr = {0};
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(proxy.port);
    if (inet_pton(AF_INET, proxy.host, &proxy_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid proxy address\n");
        return -1;
    }

    // Connect to proxy
    if (original_connect(sockfd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        fprintf(stderr, "Failed to connect to proxy: %s\n", strerror(errno));
        return -1;
    }

    if (proxy.is_http) {
        // HTTP CONNECT method
        char connect_req[256];
        struct sockaddr_in *dest = (struct sockaddr_in *)addr;
        char dest_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &dest->sin_addr, dest_ip, INET_ADDRSTRLEN);
        snprintf(connect_req, sizeof(connect_req),
                 "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
                 dest_ip, ntohs(dest->sin_port), dest_ip, ntohs(dest->sin_port));

        if (send(sockfd, connect_req, strlen(connect_req), 0) < 0) {
            fprintf(stderr, "Failed to send HTTP CONNECT: %s\n", strerror(errno));
            return -1;
        }

        char response[1024];
        ssize_t n = recv(sockfd, response, sizeof(response) - 1, 0);
        if (n <= 0 || strstr(response, "200 Connection established") == NULL) {
            fprintf(stderr, "HTTP proxy connection failed\n");
            return -1;
        }
    } else if (proxy.is_socks5) {
        // SOCKS5 handshake (no auth for simplicity)
        char init_req[] = {0x05, 0x01, 0x00}; // Version 5, 1 method, no auth
        if (send(sockfd, init_req, sizeof(init_req), 0) < 0) {
            fprintf(stderr, "Failed to send SOCKS5 init: %s\n", strerror(errno));
            return -1;
        }

        char init_resp[2];
        if (recv(sockfd, init_resp, sizeof(init_resp), 0) < 2 || init_resp[1] != 0x00) {
            fprintf(stderr, "SOCKS5 init failed\n");
            return -1;
        }

        // SOCKS5 connect request
        char connect_req[10] = {0x05, 0x01, 0x00, 0x01}; // Version 5, connect, IPv4
        struct sockaddr_in *dest = (struct sockaddr_in *)addr;
        memcpy(connect_req + 4, &dest->sin_addr, 4);
        memcpy(connect_req + 8, &dest->sin_port, 2);
        if (send(sockfd, connect_req, sizeof(connect_req), 0) < 0) {
            fprintf(stderr, "Failed to send SOCKS5 connect: %s\n", strerror(errno));
            return -1;
        }

        char connect_resp[10];
        if (recv(sockfd, connect_resp, sizeof(connect_resp), 0) < 10 || connect_resp[1] != 0x00) {
            fprintf(stderr, "SOCKS5 connect failed\n");
            return -1;
        }
    }

    return 0;
}

// Hooked connect function
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!original_connect) {
        original_connect = dlsym(RTLD_NEXT, "connect");
        if (!original_connect) {
            fprintf(stderr, "Failed to get original connect: %s\n", dlerror());
            return -1;
        }
    }

    // Check if socket is TCP
    int sock_type;
    socklen_t optlen = sizeof(sock_type);
    if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &sock_type, &optlen) == 0) {
        if (sock_type == SOCK_STREAM) {
            return connect_to_proxy(sockfd, addr, addrlen);
        }
    }

    return original_connect(sockfd, addr, addrlen);
}

// Hooked sendto function for UAE WebRTC bypass
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (!original_sendto) {
        original_sendto = dlsym(RTLD_NEXT, "sendto");
        if (!original_sendto) {
            fprintf(stderr, "Failed to get original sendto: %s\n", dlerror());
            return -1;
        }
    }

    // Check if socket is UDP
    int sock_type;
    socklen_t optlen = sizeof(sock_type);
    if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &sock_type, &optlen) == 0) {
        if (sock_type == SOCK_DGRAM && len >= 20) {
            // Check for STUN packet (WebRTC uses STUN for screen sharing/voice)
            unsigned char *packet = (unsigned char *)buf;
            // STUN packet: first two bytes are 0x00 0x01 (Binding Request) or 0x00 0x00 (Binding Response)
            if ((packet[0] == 0x00 && (packet[1] == 0x01 || packet[1] == 0x00)) &&
                ntohs(*(uint16_t *)(packet + 2)) == len - 20) { // Message length field
                // Modify STUN header to evade UAE DPI
                char modified_buf[1500];
                memcpy(modified_buf, buf, len);
                // Alter STUN message type to non-standard value (e.g., 0x0003) to disguise as non-WebRTC
                modified_buf[0] = 0x00;
                modified_buf[1] = 0x03;
                // Adjust transaction ID (bytes 8-19) to avoid DPI pattern matching
                for (int i = 8; i < 20; i++) {
                    modified_buf[i] ^= 0xFF; // Simple XOR to obfuscate
                }
                return original_sendto(sockfd, modified_buf, len, flags, dest_addr, addrlen);
            }
        }
    }

    return original_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

// Constructor to load configuration
__attribute__((constructor))
static void init(void) {
    // Load configuration from drover.ini in Vesktop's config directory
    char *home = getenv("HOME");
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s/.var/app/dev.vencord.Vesktop/config/drover.ini", home);
    load_config(config_path);
}
