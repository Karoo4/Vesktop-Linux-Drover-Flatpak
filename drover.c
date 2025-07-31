#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Configuration structure
typedef struct {
  char *proxy;            // Proxy URL (e.g., http://127.0.0.1:1080 or
                          // socks5://127.0.0.1:1080)
  bool use_nekobox_proxy; // Use NekoBox proxy if available
  char *nekobox_proxy;    // NekoBox proxy URL
  bool direct_mode;       // Direct mode for UDP bypass without proxy
  char *log_file;         // Log file path
} DroverOptions;

// Proxy value structure
typedef struct {
  bool is_specified;
  char *protocol; // http or socks5
  char *host;
  int port;
  char *login; // Optional for authenticated proxies
  char *password;
  bool is_http;
  bool is_socks5;
  bool is_auth;
} ProxyValue;

// Global configuration
static DroverOptions config = {0};
static ProxyValue proxy = {0};
static FILE *log_fp = NULL;

// Function pointers for original system calls
static int (*original_connect)(int sockfd, const struct sockaddr *addr,
                               socklen_t addrlen) = NULL;
static ssize_t (*original_sendto)(int sockfd, const void *buf, size_t len,
                                  int flags, const struct sockaddr *dest_addr,
                                  socklen_t addrlen) = NULL;

// Logging function
static void log_message(const char *level, const char *message, ...) {
  if (!log_fp)
    return;
  va_list args;
  va_start(args, message);
  time_t now = time(NULL);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
  fprintf(log_fp, "[%s] %s: ", timestamp, level);
  vfprintf(log_fp, message, args);
  fprintf(log_fp, "\n");
  fflush(log_fp);
  va_end(args);
}

// Custom function to parse IPv4 address (replaces inet_pton)
static int parse_ipv4(const char *ip_str, struct in_addr *addr) {
  unsigned int a, b, c, d;
  if (sscanf(ip_str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 ||
      b > 255 || c > 255 || d > 255) {
    return -1; // Invalid IP
  }
  addr->s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
  return 0;
}

// Custom function to format IPv4 address (replaces inet_ntop)
static void format_ipv4(const struct in_addr *addr, char *buf, size_t buf_len) {
  unsigned int ip = ntohl(addr->s_addr);
  snprintf(buf, buf_len, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF);
}

// Improved INI file parser
static bool parse_ini(const char *filename, DroverOptions *config) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    log_message("ERROR", "Failed to open %s: %s", filename, strerror(errno));
    return false;
  }

  char line[512];
  bool in_drover_section = false;
  while (fgets(line, sizeof(line), file)) {
    // Trim whitespace and newline
    char *end = line + strlen(line) - 1;
    while (end > line && (*end == '\n' || *end == '\r' || *end == ' '))
      *end-- = '\0';
    char *start = line;
    while (*start == ' ')
      start++;

    // Skip empty lines or comments
    if (start[0] == '\0' || start[0] == '#' || start[0] == ';')
      continue;

    // Check for [drover] section
    if (strncmp(start, "[drover]", 8) == 0) {
      in_drover_section = true;
      continue;
    }

    if (!in_drover_section)
      continue;

    // Parse key=value
    char *equals = strchr(start, '=');
    if (!equals)
      continue;
    *equals = '\0';
    char *key = start;
    char *value = equals + 1;
    while (*value == ' ')
      value++;

    // Assign values
    if (strcmp(key, "proxy") == 0) {
      free(config->proxy);
      config->proxy = strdup(value);
    } else if (strcmp(key, "use-nekobox-proxy") == 0) {
      config->use_nekobox_proxy =
          (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    } else if (strcmp(key, "nekobox-proxy") == 0) {
      free(config->nekobox_proxy);
      config->nekobox_proxy = strdup(value);
    } else if (strcmp(key, "direct-mode") == 0) {
      config->direct_mode =
          (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    } else if (strcmp(key, "log-file") == 0) {
      free(config->log_file);
      config->log_file = strdup(value);
    }
  }
  fclose(file);
  return true;
}

// Manual proxy URL parser (replaces regex)
static bool parse_proxy(const char *url, ProxyValue *proxy) {
  proxy->is_specified = false;
  free(proxy->protocol);
  free(proxy->host);
  free(proxy->login);
  free(proxy->password);
  proxy->protocol = NULL;
  proxy->host = NULL;
  proxy->login = NULL;
  proxy->password = NULL;
  proxy->port = 0;
  proxy->is_http = false;
  proxy->is_socks5 = false;
  proxy->is_auth = false;

  if (!url || !*url) {
    log_message("ERROR", "Empty proxy URL");
    return false;
  }

  char *work = strdup(url);
  if (!work) {
    log_message("ERROR", "Memory allocation failed for proxy URL parsing");
    return false;
  }

  char *protocol_end = strstr(work, "://");
  if (protocol_end) {
    *protocol_end = '\0';
    proxy->protocol = strdup(work);
    if (strcmp(proxy->protocol, "https") == 0) {
      free(proxy->protocol);
      proxy->protocol = strdup("http");
    }
    work = protocol_end + 3;
  } else {
    proxy->protocol = strdup("http");
  }
  proxy->is_http = strcmp(proxy->protocol, "http") == 0;
  proxy->is_socks5 = strcmp(proxy->protocol, "socks5") == 0;

  char *auth_end = strchr(work, '@');
  if (auth_end) {
    *auth_end = '\0';
    char *colon = strchr(work, ':');
    if (colon) {
      *colon = '\0';
      proxy->login = strdup(work);
      proxy->password = strdup(colon + 1);
      proxy->is_auth = (proxy->login[0] != '\0' && proxy->password[0] != '\0');
    }
    work = auth_end + 1;
  }

  char *port_start = strrchr(work, ':');
  if (!port_start) {
    free(work);
    log_message("ERROR", "No port specified in proxy URL: %s", url);
    return false;
  }
  *port_start = '\0';
  proxy->host = strdup(work);
  proxy->port = atoi(port_start + 1);

  if (!proxy->host || proxy->port <= 0 || proxy->port > 65535) {
    free(work);
    log_message("ERROR", "Invalid host or port in proxy URL: %s", url);
    return false;
  }

  proxy->is_specified = true;
  free(work);
  return true;
}

// Load configuration with logging
static bool load_config(const char *filename) {
  config.proxy = strdup("");
  config.use_nekobox_proxy = false;
  config.nekobox_proxy = strdup("http://127.0.0.1:2080");
  config.direct_mode = true; // Default to direct mode as per user preference
  config.log_file = strdup("/tmp/drover.log");

  if (!parse_ini(filename, &config)) {
    log_message("ERROR", "Failed to parse configuration file %s", filename);
    return false;
  }

  // Initialize logging
  if (config.log_file && strlen(config.log_file) > 0) {
    log_fp = fopen(config.log_file, "a");
    if (!log_fp) {
      fprintf(stderr, "Failed to open log file %s: %s\n", config.log_file,
              strerror(errno));
    }
  }

  // Parse proxy settings
  if (config.proxy && strlen(config.proxy) > 0) {
    if (!parse_proxy(config.proxy, &proxy)) {
      log_message("ERROR", "Failed to parse proxy: %s", config.proxy);
      return false;
    }
  } else if (config.use_nekobox_proxy && config.nekobox_proxy) {
    if (!parse_proxy(config.nekobox_proxy, &proxy)) {
      log_message("ERROR", "Failed to parse NekoBox proxy: %s",
                  config.nekobox_proxy);
      return false;
    }
  }

  log_message("INFO",
              "Configuration loaded: proxy=%s, use_nekobox=%d, direct_mode=%d",
              config.proxy, config.use_nekobox_proxy, config.direct_mode);
  return true;
}

// Improved proxy connection with timeout
static int connect_to_proxy(int sockfd, const struct sockaddr *addr,
                            socklen_t addrlen) {
  if (!proxy.is_specified || config.direct_mode) {
    log_message("INFO",
                "Using direct connection (direct_mode=%d, proxy_specified=%d)",
                config.direct_mode, proxy.is_specified);
    return original_connect(sockfd, addr, addrlen);
  }

  struct sockaddr_in proxy_addr = {0};
  proxy_addr.sin_family = AF_INET;
  proxy_addr.sin_port = htons(proxy.port);
  if (parse_ipv4(proxy.host, &proxy_addr.sin_addr) < 0) {
    log_message("ERROR", "Invalid proxy address: %s", proxy.host);
    return -1;
  }

  // Set socket timeout
  struct timeval connect_timeout = {.tv_sec = 5, .tv_usec = 0};
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &connect_timeout,
             sizeof(connect_timeout));
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &connect_timeout,
             sizeof(connect_timeout));

  // Connect to proxy
  if (original_connect(sockfd, (struct sockaddr *)&proxy_addr,
                       sizeof(proxy_addr)) < 0) {
    log_message("ERROR", "Failed to connect to proxy %s:%d: %s", proxy.host,
                proxy.port, strerror(errno));
    return -1;
  }

  if (proxy.is_http) {
    // HTTP CONNECT method with authentication
    char connect_req[512];
    struct sockaddr_in *dest = (struct sockaddr_in *)addr;
    char dest_ip[16]; // Enough for "255.255.255.255"
    format_ipv4(&dest->sin_addr, dest_ip, sizeof(dest_ip));
    if (proxy.is_auth) {
      char auth[256];
      snprintf(auth, sizeof(auth), "%s:%s", proxy.login, proxy.password);
      char *b64_auth = NULL; // Implement base64 encoding if needed
      snprintf(connect_req, sizeof(connect_req),
               "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\nProxy-Authorization: "
               "Basic %s\r\n\r\n",
               dest_ip, ntohs(dest->sin_port), dest_ip, ntohs(dest->sin_port),
               auth);
    } else {
      snprintf(connect_req, sizeof(connect_req),
               "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n", dest_ip,
               ntohs(dest->sin_port), dest_ip, ntohs(dest->sin_port));
    }

    if (send(sockfd, connect_req, strlen(connect_req), 0) < 0) {
      log_message("ERROR", "Failed to send HTTP CONNECT: %s", strerror(errno));
      return -1;
    }

    char response[1024];
    ssize_t n = recv(sockfd, response, sizeof(response) - 1, 0);
    response[n] = '\0';
    if (n <= 0 || strstr(response, "200 Connection established") == NULL) {
      log_message("ERROR", "HTTP proxy connection failed: %s", response);
      return -1;
    }
  } else if (proxy.is_socks5) {
    // SOCKS5 handshake with authentication
    char init_req[4] = {0x05, 0x02, 0x00,
                        0x02}; // Version 5, 2 methods: no auth, user/pass
    if (send(sockfd, init_req, sizeof(init_req), 0) < 0) {
      log_message("ERROR", "Failed to send SOCKS5 init: %s", strerror(errno));
      return -1;
    }

    char init_resp[2];
    if (recv(sockfd, init_resp, sizeof(init_resp), 0) < 2) {
      log_message("ERROR", "SOCKS5 init response failed");
      return -1;
    }
    if (init_resp[1] == 0x02 && proxy.is_auth) {
      // Username/password authentication
      char auth_req[512];
      int len = snprintf(auth_req, sizeof(auth_req), "%c%c%s%c%s", 0x01,
                         (char)strlen(proxy.login), proxy.login,
                         (char)strlen(proxy.password), proxy.password);
      if (send(sockfd, auth_req, len, 0) < 0) {
        log_message("ERROR", "Failed to send SOCKS5 auth: %s", strerror(errno));
        return -1;
      }
      char auth_resp[2];
      if (recv(sockfd, auth_resp, sizeof(auth_resp), 0) < 2 ||
          auth_resp[1] != 0x00) {
        log_message("ERROR", "SOCKS5 auth failed");
        return -1;
      }
    } else if (init_resp[1] != 0x00) {
      log_message("ERROR", "SOCKS5 method selection failed: %d", init_resp[1]);
      return -1;
    }

    // SOCKS5 connect request
    char connect_req[10] = {0x05, 0x01, 0x00, 0x01}; // Version 5, connect, IPv4
    struct sockaddr_in *dest = (struct sockaddr_in *)addr;
    memcpy(connect_req + 4, &dest->sin_addr, 4);
    memcpy(connect_req + 8, &dest->sin_port, 2);
    if (send(sockfd, connect_req, sizeof(connect_req), 0) < 0) {
      log_message("ERROR", "Failed to send SOCKS5 connect: %s",
                  strerror(errno));
      return -1;
    }

    char connect_resp[10];
    if (recv(sockfd, connect_resp, sizeof(connect_resp), 0) < 10 ||
        connect_resp[1] != 0x00) {
      log_message("ERROR", "SOCKS5 connect failed: %d", connect_resp[1]);
      return -1;
    }
  }

  // Reset timeouts after successful connection
  struct timeval reset_timeout = {.tv_sec = 0, .tv_usec = 0};
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &reset_timeout,
             sizeof(reset_timeout));
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &reset_timeout,
             sizeof(reset_timeout));

  log_message("INFO", "Successfully connected to proxy %s:%d", proxy.host,
              proxy.port);
  return 0;
}

// Hooked connect function
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  if (!original_connect) {
    original_connect = dlsym(RTLD_NEXT, "connect");
    if (!original_connect) {
      log_message("ERROR", "Failed to get original connect: %s", dlerror());
      return -1;
    }
  }

  // Check if socket is TCP
  int sock_type;
  socklen_t optlen = sizeof(sock_type);
  if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &sock_type, &optlen) == 0) {
    if (sock_type == SOCK_STREAM) {
      char ip_str[16];
      format_ipv4(&((struct sockaddr_in *)addr)->sin_addr, ip_str,
                  sizeof(ip_str));
      log_message("DEBUG", "TCP connect to %s:%d", ip_str,
                  ntohs(((struct sockaddr_in *)addr)->sin_port));
      return connect_to_proxy(sockfd, addr, addrlen);
    }
  }

  return original_connect(sockfd, addr, addrlen);
}

// Enhanced UDP packet obfuscation
static void obfuscate_stun_packet(unsigned char *buf, size_t len) {
  // Randomize message type to non-standard values (avoid DPI detection)
  uint16_t msg_type =
      (rand() % 0xFFFE) + 0x0003; // Non-standard STUN message type
  buf[0] = (msg_type >> 8) & 0xFF;
  buf[1] = msg_type & 0xFF;

  // Randomize transaction ID (bytes 8-19)
  for (int i = 8; i < 20; i++) {
    buf[i] = (unsigned char)rand();
  }

  // Add fake attribute to confuse DPI (e.g., non-standard attribute)
  if (len + 8 <= 1500) {
    uint16_t attr_type = 0xFFFF; // Non-standard attribute
    uint16_t attr_len = 4;
    uint32_t fake_value = rand();
    memcpy(buf + len, &attr_type, 2);
    memcpy(buf + len + 2, &attr_len, 2);
    memcpy(buf + len + 4, &fake_value, 4);
    // Update STUN length field (bytes 2-3)
    uint16_t new_len = ntohs(*(uint16_t *)(buf + 2)) + 8;
    *(uint16_t *)(buf + 2) = htons(new_len);
  }
}

// Hooked sendto function for WebRTC bypass
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
  if (!original_sendto) {
    original_sendto = dlsym(RTLD_NEXT, "sendto");
    if (!original_sendto) {
      log_message("ERROR", "Failed to get original sendto: %s", dlerror());
      return -1;
    }
  }

  // Check if socket is UDP
  int sock_type;
  socklen_t optlen = sizeof(sock_type);
  if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &sock_type, &optlen) == 0) {
    if (sock_type == SOCK_DGRAM && len >= 20) {
      unsigned char *packet = (unsigned char *)buf;
      // Detect STUN or TURN packets (used in WebRTC)
      uint16_t msg_type = (packet[0] << 8) | packet[1];
      uint16_t msg_len = ntohs(*(uint16_t *)(packet + 2));
      if (((msg_type == 0x0001 ||
            msg_type == 0x0000) || // STUN Binding Request/Response
           (msg_type >= 0x4000 && msg_type <= 0x4FFF)) && // TURN messages
          msg_len == len - 20) {
        log_message("DEBUG", "Detected WebRTC packet, obfuscating (len=%zu)",
                    len);
        char modified_buf[1500];
        memcpy(modified_buf, buf, len);
        obfuscate_stun_packet((unsigned char *)modified_buf, len);
        return original_sendto(sockfd, modified_buf, len, flags, dest_addr,
                               addrlen);
      }
    }
  }

  return original_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

// Cleanup function
__attribute__((destructor)) static void cleanup(void) {
  free(config.proxy);
  free(config.nekobox_proxy);
  free(config.log_file);
  free(proxy.protocol);
  free(proxy.host);
  free(proxy.login);
  free(proxy.password);
  if (log_fp)
    fclose(log_fp);
}

// Constructor to load configuration
__attribute__((constructor)) static void init(void) {
  srand(time(NULL)); // Initialize random seed for obfuscation
  char *home = getenv("HOME");
  char config_path[256];
  snprintf(config_path, sizeof(config_path),
           "%s/.var/app/dev.vencord.Vesktop/config/drover.ini", home);
  load_config(config_path);

  // Support Discord Canary/PTB by checking alternate paths
  if (!log_fp) {
    char *alt_paths[] = {"%s/.config/discordcanary/drover.ini",
                         "%s/.config/discordptb/drover.ini",
                         "%s/.config/discord/drover.ini"};
    for (int i = 0; i < 3 && !log_fp; i++) {
      snprintf(config_path, sizeof(config_path), alt_paths[i], home);
      load_config(config_path);
    }
  }
}
