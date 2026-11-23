// OpenSSL 3 (Debian 13: libssl.so.3, libcrypto.so.3) для связи с планшетом (tablet.cpp): библиотеки системные,
// заголовков (-dev) на Pi нет — объявления по ssl.h, err.h; все объекты — непрозрачные указатели. Макросы ssl.h
// (SSL_CTX_set_min_proto_version, SSL_set_mode) — через *_ctrl с их номерами.
#pragma once

#include <cstddef>

extern "C" {
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
const SSL_METHOD *TLS_server_method(void);
SSL_CTX *SSL_CTX_new(const SSL_METHOD *method);
void SSL_CTX_free(SSL_CTX *ctx);
int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file);
int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);
int SSL_CTX_check_private_key(const SSL_CTX *ctx);
long SSL_CTX_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg);
int SSL_CTX_set_num_tickets(SSL_CTX *ctx, size_t num_tickets);
SSL *SSL_new(SSL_CTX *ctx);
void SSL_free(SSL *ssl);
int SSL_set_fd(SSL *ssl, int fd);
long SSL_ctrl(SSL *ssl, int cmd, long larg, void *parg);
int SSL_accept(SSL *ssl);
int SSL_read(SSL *ssl, void *buf, int num);
int SSL_write(SSL *ssl, const void *buf, int num);
int SSL_get_error(const SSL *ssl, int ret);
int SSL_shutdown(SSL *ssl);
unsigned long ERR_get_error(void);
void ERR_error_string_n(unsigned long e, char *buf, size_t len);
void ERR_clear_error(void);
}

constexpr int kSslFiletypePem = 1;               // SSL_FILETYPE_PEM
constexpr int kSslCtrlMode = 33;                 // SSL_CTRL_MODE
constexpr int kSslCtrlSetMinProto = 123;         // SSL_CTRL_SET_MIN_PROTO_VERSION
constexpr long kTls13 = 0x0304;                  // TLS1_3_VERSION
constexpr long kSslModePartialWrite = 0x1;       // SSL_MODE_ENABLE_PARTIAL_WRITE
constexpr long kSslModeMovingBuffer = 0x2;       // SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
constexpr int kSslErrorWantRead = 2, kSslErrorWantWrite = 3, kSslErrorZeroReturn = 6;
