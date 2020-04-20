#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"
#include "os_auth/check_cert.h"
#include "os_auth/auth.h"

#ifdef WIN32 
    #include "unit_tests/wrappers/shared/enrollment_op.h"
#else
    static int flag_fopen = 0;
#endif

extern int w_enrollment_concat_src_ip(char *buff, const char* sender_ip);
extern void w_enrollment_concat_group(char *buff, const char* centralized_group);
extern void w_enrollment_verify_ca_certificate(const SSL *ssl, const char *ca_cert, const char *hostname);
extern int w_enrollment_connect(w_enrollment_ctx *cfg, const char * server_address);
extern int w_enrollment_send_message(w_enrollment_ctx *cfg);
extern int w_enrollment_store_key_entry(const char* keys);
extern int w_enrollment_process_agent_key(char *buffer);
extern int w_enrollment_process_response(SSL *ssl);

static w_enrollment_target* local_target;
static w_enrollment_cert* local_cert;
/*************** WRAPS ************************/
void __wrap__merror(const char * file, int line, const char * func, const char *msg, ...) {
    char formatted_msg[OS_MAXSTR];
    va_list args;

    va_start(args, msg);
    vsnprintf(formatted_msg, OS_MAXSTR, msg, args);
    va_end(args);

    check_expected(formatted_msg);
}

void __wrap__mwarn(const char * file, int line, const char * func, const char *msg, ...) {
    char formatted_msg[OS_MAXSTR];
    va_list args;

    va_start(args, msg);
    vsnprintf(formatted_msg, OS_MAXSTR, msg, args);
    va_end(args);

    check_expected(formatted_msg);
}

void __wrap__minfo(const char * file, int line, const char * func, const char *msg, ...) {
    char formatted_msg[OS_MAXSTR];
    va_list args;

    va_start(args, msg);
    vsnprintf(formatted_msg, OS_MAXSTR, msg, args);
    va_end(args);

    check_expected(formatted_msg);
}

int __wrap_OS_IsValidIP(const char *ip_address, os_ip *final_ip) {
    check_expected(ip_address);
    check_expected(final_ip);
    return mock_type(int);
}

int __wrap_check_x509_cert(const SSL *ssl, const char *manager) {
    check_expected_ptr(ssl);
    check_expected(manager);
    return mock_type(int);
}

char *__wrap_OS_GetHost(const char *host, unsigned int attempts) {
    check_expected(host);
    return mock_ptr_type(char *);
}

SSL_CTX *__wrap_os_ssl_keys(int is_server, const char *os_dir, const char *ciphers, const char *cert, const char *key, const char *ca_cert, int auto_method)
{
    check_expected(is_server);
    check_expected(os_dir);
    check_expected(ciphers);
    check_expected(cert);
    check_expected(key);
    check_expected(ca_cert);
    check_expected(auto_method);
    return mock_ptr_type(SSL_CTX *);
}

extern SSL *__real_SSL_new(SSL_CTX *ctx);
SSL *__wrap_SSL_new(SSL_CTX *ctx) {
    check_expected(ctx);
    return mock_ptr_type(SSL *);
}

int __wrap_SSL_connect(SSL *s){
    return mock_type(int);
}

int __wrap_SSL_get_error(const SSL *s, int i)
{
    check_expected(i);
    return mock_type(int);
}

int __wrap_SSL_write(SSL *ssl,	const void *buf, int num) {
    check_expected(ssl);
    check_expected(buf);
    return mock_type(int);
}

int __wrap_SSL_read(SSL *ssl, void *buf, int num) {
    check_expected(ssl);
    snprintf(buf, num, "%s",mock_ptr_type(char*));
    return mock_type(int);
}

BIO *__wrap_BIO_new_socket(int sock, int close_flag) {
    return NULL;
}

int __wrap_OS_ConnectTCP(u_int16_t _port, const char *_ip, int ipv6)
{
    check_expected(_port);
    check_expected(_ip);
    check_expected(ipv6);
    return mock_type(int);
}

void __wrap_SSL_set_bio(SSL *s, BIO *rbio, BIO *wbio) {
    return;    
}

extern FILE * __real_fopen ( const char * filename, const char * mode );
FILE * __wrap_fopen ( const char * filename, const char * mode ) {
    if(!flag_fopen)
        return __real_fopen(filename, mode);
    check_expected(filename);
    check_expected(mode);
    return mock_ptr_type(FILE *);
}

extern int __real_fclose ( FILE * stream );
int __wrap_fclose ( FILE * stream ) {
    if(!flag_fopen)
        return __real_fclose(stream);
    return 0;
}

int __wrap_gethostname(char *name, int len) {
    snprintf(name, len, "%s",mock_ptr_type(char*));
    return mock_type(int);
}

#ifndef WIN32
extern int __real_fprintf ( FILE * stream, const char * format, ... );
int __wrap_fprintf ( FILE * stream, const char * format, ... ) {
    char formatted_msg[OS_MAXSTR];
    va_list args;

    va_start(args, format);
    vsnprintf(formatted_msg, OS_MAXSTR, format, args);
    va_end(args);

    if(!flag_fopen)
        return __real_fprintf(stream, formatted_msg);

    check_expected(stream);
    check_expected(formatted_msg);
    return 0;
}
#endif

// Setup / Teardown global
int setup_file_ops(void **state) {
    flag_fopen = 1;
    return 0;
}

int teardown_file_ops(void **state) {
    flag_fopen = 0;
    return 0;
}

// Setup
int test_setup_concats(void **state) {
    char *buf;
    os_calloc(OS_SIZE_65536, sizeof(char), buf);
    buf[OS_SIZE_65536 - 1] = '\0';
    *state = buf;
    return 0;
}
//Teardown
int test_teardown_concats(void **state) {
    free(*state);
    return 0;
}

// Setup
int test_setup_context(void **state) {
    local_target = w_enrollment_target_init();
    local_target->manager_name = strdup("valid_hostname");
    local_target->agent_name = NULL;
    local_target->sender_ip = NULL;
    local_target->port = 1234; 
    local_target->centralized_group = NULL;
    local_cert = w_enrollment_cert_init();
    local_cert->ciphers = DEFAULT_CIPHERS;
    local_cert->auto_method = 0;
    local_cert->authpass = NULL;
    local_cert->agent_cert = strdup("CERT");
    local_cert->agent_key = strdup("KEY");
    local_cert->ca_cert = strdup("CA_CERT");
    w_enrollment_ctx *cfg = w_enrollment_init(local_target, local_cert);
    *state = cfg;
    return 0;
}

//Teardown 
int test_teardown_context(void **state) {
    w_enrollment_ctx *cfg = *state;
    os_free(local_target->manager_name);
    os_free(local_target->agent_name);
    os_free(local_target);
    os_free(local_cert->agent_cert);
    os_free(local_cert->agent_key);
    os_free(local_cert->ca_cert);
    os_free(local_cert);
    w_enrollment_destroy(cfg);
    return 0;
}

//Setup
int test_setup_context_2(void **state) {
    local_target = w_enrollment_target_init();
    local_target->manager_name = strdup("valid_hostname");
    local_target->agent_name = strdup("test_agent");
    local_target->sender_ip = "192.168.1.1";
    local_target->port = 1234; 
    local_target->centralized_group = "test_group";
    local_cert = w_enrollment_cert_init();
    local_cert->ciphers = DEFAULT_CIPHERS;
    local_cert->auto_method = 0;
    local_cert->authpass = "test_password";
    local_cert->agent_cert = strdup("CERT");
    local_cert->agent_key = strdup("KEY");
    local_cert->ca_cert = strdup("CA_CERT");
    w_enrollment_ctx *cfg = w_enrollment_init(local_target, local_cert);
    *state = cfg;
    return 0;
}

//Setup 
int test_setup_context_3(void **state) {
    os_malloc(sizeof(w_enrollment_target), local_target);
    local_target->manager_name = strdup("valid_hostname");
    local_target->agent_name = strdup("Invalid\'!@Hostname\'");
    local_target->sender_ip = NULL;
    local_target->port = 1234; 
    local_target->centralized_group = NULL;
    os_malloc(sizeof(w_enrollment_cert), local_cert);
    local_cert->ciphers = DEFAULT_CIPHERS;
    local_cert->auto_method = 0;
    local_cert->authpass = NULL;
    local_cert->agent_cert = strdup("CERT");
    local_cert->agent_key = strdup("KEY");
    local_cert->ca_cert = strdup("CA_CERT");
    w_enrollment_ctx *cfg = w_enrollment_init(local_target, local_cert);
    *state = cfg;
    return 0;
}

//Setup
int test_setup_w_enrolment_request_key(void **state) {
    local_target = w_enrollment_target_init();
    local_target->manager_name = strdup("valid_hostname");
    local_target->agent_name = "test_agent";
    local_target->sender_ip = "192.168.1.1";
    local_target->port = 1234; 
    local_target->centralized_group = "test_group";
    local_cert = w_enrollment_cert_init();
    local_cert->ciphers = DEFAULT_CIPHERS;
    local_cert->auto_method = 0;
    local_cert->authpass = "test_password";
    local_cert->agent_cert = strdup("CERT");
    local_cert->agent_key = strdup("KEY");
    local_cert->ca_cert = strdup("CA_CERT");
    w_enrollment_ctx *cfg = w_enrollment_init(local_target, local_cert);
    *state = cfg;
    flag_fopen = 1;
    return 0;
}

//Teardown
int test_teardown_w_enrolment_request_key(void **state){
    w_enrollment_ctx *cfg = *state;
    os_free(local_target->manager_name);
    os_free(local_target);
    os_free(local_cert->agent_cert);
    os_free(local_cert->agent_key);
    os_free(local_cert->ca_cert);
    os_free(local_cert);
    w_enrollment_destroy(cfg);
    flag_fopen = 0;
    return 0;
}

//Setup
int test_setup_ssl_context(void **state) {
    SSL_CTX *ctx = get_ssl_context(DEFAULT_CIPHERS, 0);
    SSL *ssl = __real_SSL_new(ctx);
    SSL_CTX_free(ctx);
    *state = ssl;
    setup_file_ops(state);
    return 0;
}
//Teardown
int test_teardow_ssl_context(void **state) {
    SSL_free(*state);
    teardown_file_ops(state);
    return 0;
}
/**********************************************/
/************* w_enrollment_concat_src_ip ****************/
void test_w_enrollment_concat_src_ip_invalid_ip(void **state) {
    char *buf = *state;
    const char* sender_ip = "256.300.1";
    expect_string(__wrap_OS_IsValidIP, ip_address, sender_ip);
    expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
    will_return(__wrap_OS_IsValidIP, 0);

    expect_string(__wrap__merror, formatted_msg, "Invalid IP address provided for sender IP.");
    int ret = w_enrollment_concat_src_ip(buf, sender_ip);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_concat_src_ip_valid_ip(void **state) {
    char *buf = *state;
    const char* sender_ip = "192.168.1.1";
    expect_string(__wrap_OS_IsValidIP, ip_address, sender_ip);
    expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
    will_return(__wrap_OS_IsValidIP, 1);

    int ret = w_enrollment_concat_src_ip(buf, sender_ip);
    assert_int_equal(ret, 0);
    assert_string_equal(buf, " IP:'192.168.1.1'");
}

void test_w_enrollment_concat_src_ip_empty_ip(void **state) {
    char *buf = *state;
    const char* sender_ip = NULL;

    int ret = w_enrollment_concat_src_ip(buf, sender_ip);
    assert_int_equal(ret, 0);
    assert_string_equal(buf, " IP:'src'");
}

void test_w_enrollment_concat_src_ip_empty_buff(void **state) {
    expect_assert_failure(w_enrollment_concat_src_ip(NULL, NULL));
}
/**********************************************/
/************* w_enrollment_concat_group ****************/
void test_w_enrollment_concat_group_empty_buff(void **state) {
    expect_assert_failure(w_enrollment_concat_group(NULL, "EXAMPLE_GROUP"));
}

void test_w_enrollment_concat_group_empty_group(void **state) {
    char *buf = *state;
    expect_assert_failure(w_enrollment_concat_group(buf, NULL));
}

void test_w_enrollment_concat_group(void **state) {
    char *buf = *state;
    const char *group = "EXAMPLE_GROUP";
    w_enrollment_concat_group(buf, group);
    assert_string_equal(buf, " G:'EXAMPLE_GROUP'");
}
/**********************************************/
/********** w_enrollment_verify_ca_certificate *************/
void test_w_enrollment_verify_ca_certificate_null_connection(void **state) {
    expect_assert_failure(w_enrollment_verify_ca_certificate(NULL, "certificate_path", "hostname"));
}

void test_w_enrollment_verify_ca_certificate_no_certificate(void **state) {
    SSL *ssl = *state;
    expect_string(__wrap__mwarn, formatted_msg, "Registering agent to unverified manager.");
    w_enrollment_verify_ca_certificate(ssl, NULL, "hostname");
}

void test_verificy_ca_certificate_invalid_certificate(void **state) {
    SSL *ssl = *state;
    const char *hostname = "hostname";
    expect_value(__wrap_check_x509_cert, ssl, ssl);
    expect_string(__wrap_check_x509_cert, manager, hostname);
    will_return(__wrap_check_x509_cert, VERIFY_FALSE);

    expect_string(__wrap__minfo, formatted_msg, "Verifying manager's certificate");
    expect_string(__wrap__merror, formatted_msg, "Unable to verify server certificate.");
    w_enrollment_verify_ca_certificate(ssl, "BAD_CERTIFICATE", "hostname");
}

void test_verificy_ca_certificate_valid_certificate(void **state) {
    SSL *ssl = *state;
    const char *hostname = "hostname";
    expect_value(__wrap_check_x509_cert, ssl, ssl);
    expect_string(__wrap_check_x509_cert, manager, hostname);
    will_return(__wrap_check_x509_cert, VERIFY_TRUE);

    expect_string(__wrap__minfo, formatted_msg, "Verifying manager's certificate");
    expect_string(__wrap__minfo, formatted_msg, "Manager has been verified successfully");
    w_enrollment_verify_ca_certificate(ssl, "GOOD_CERTIFICATE", "hostname");
}
/**********************************************/
/********** w_enrollment_connect *******/
void test_w_enrollment_connect_empty_address(void **state) {
    w_enrollment_ctx *cfg = *state;  
    expect_assert_failure(w_enrollment_connect(cfg, NULL));
}

void test_w_enrollment_connect_empty_config(void **state) {
    expect_assert_failure(w_enrollment_connect(NULL, "hostname"));
}

void test_w_enrollment_connect_invalid_hostname(void **state) {
    w_enrollment_ctx *cfg = *state; 

    expect_string(__wrap_OS_GetHost, host, cfg->target_cfg->manager_name);
    will_return(__wrap_OS_GetHost, NULL);
    expect_string(__wrap__merror, formatted_msg, "Could not resolve hostname: valid_hostname\n");

    int ret = w_enrollment_connect(cfg, cfg->target_cfg->manager_name);
    assert_int_equal(ret, ENROLLMENT_WRONG_CONFIGURATION);
}

void test_w_enrollment_connect_could_not_setup(void **state) {
    w_enrollment_ctx *cfg = *state; 

    expect_string(__wrap_OS_GetHost, host, cfg->target_cfg->manager_name);
    will_return(__wrap_OS_GetHost, strdup("127.0.0.1"));
    expect_value(__wrap_os_ssl_keys, is_server, 0);
    expect_value(__wrap_os_ssl_keys, os_dir, NULL);
    expect_string(__wrap_os_ssl_keys, ciphers, DEFAULT_CIPHERS);
    expect_string(__wrap_os_ssl_keys, cert, "CERT");
    expect_string(__wrap_os_ssl_keys, key, "KEY");
    expect_string(__wrap_os_ssl_keys, ca_cert, "CA_CERT");
    expect_value(__wrap_os_ssl_keys, auto_method, 0);
    will_return(__wrap_os_ssl_keys, NULL);

    expect_string(__wrap__merror, formatted_msg, "Could not set up SSL connection! Check ceritification configuration.");
    int ret = w_enrollment_connect(cfg, cfg->target_cfg->manager_name);
    assert_int_equal(ret, ENROLLMENT_WRONG_CONFIGURATION);
}

void test_w_enrollment_connect_socket_error(void **state) {
    w_enrollment_ctx *cfg = *state;
    SSL_CTX *ctx = get_ssl_context(DEFAULT_CIPHERS, 0);

    // GetHost
    expect_string(__wrap_OS_GetHost, host, cfg->target_cfg->manager_name);
    will_return(__wrap_OS_GetHost, strdup("127.0.0.1"));
    // os_ssl_keys
    expect_value(__wrap_os_ssl_keys, is_server, 0);
    expect_value(__wrap_os_ssl_keys, os_dir, NULL);
    expect_string(__wrap_os_ssl_keys, ciphers, DEFAULT_CIPHERS);
    expect_string(__wrap_os_ssl_keys, cert, "CERT");
    expect_string(__wrap_os_ssl_keys, key, "KEY");
    expect_string(__wrap_os_ssl_keys, ca_cert, "CA_CERT");
    expect_value(__wrap_os_ssl_keys, auto_method, 0);
    will_return(__wrap_os_ssl_keys, ctx);
    // OS_ConnectTCP
    expect_value(__wrap_OS_ConnectTCP, _port, 1234);
    expect_string(__wrap_OS_ConnectTCP, _ip, "127.0.0.1");
    expect_value(__wrap_OS_ConnectTCP, ipv6, 0);
    will_return(__wrap_OS_ConnectTCP, -1);

    expect_string(__wrap__merror, formatted_msg, "Unable to connect to 127.0.0.1:1234");
    int ret = w_enrollment_connect(cfg, cfg->target_cfg->manager_name);
    assert_int_equal(ret, ENROLLMENT_CONNECTION_FAILURE);
}

void test_w_enrollment_connect_SSL_connect_error(void **state) {
    w_enrollment_ctx *cfg = *state; 
    SSL_CTX *ctx = get_ssl_context(DEFAULT_CIPHERS, 0);
    // GetHost
    expect_string(__wrap_OS_GetHost, host, cfg->target_cfg->manager_name);
    will_return(__wrap_OS_GetHost, strdup("127.0.0.1"));
    // os_ssl_keys
    expect_value(__wrap_os_ssl_keys, is_server, 0);
    expect_value(__wrap_os_ssl_keys, os_dir, NULL);
    expect_string(__wrap_os_ssl_keys, ciphers, DEFAULT_CIPHERS);
    expect_string(__wrap_os_ssl_keys, cert, "CERT");
    expect_string(__wrap_os_ssl_keys, key, "KEY");
    expect_string(__wrap_os_ssl_keys, ca_cert, "CA_CERT");
    expect_value(__wrap_os_ssl_keys, auto_method, 0);
    will_return(__wrap_os_ssl_keys, ctx);
    // OS_ConnectTCP
    expect_value(__wrap_OS_ConnectTCP, _port, 1234);
    expect_string(__wrap_OS_ConnectTCP, _ip, "127.0.0.1");
    expect_value(__wrap_OS_ConnectTCP, ipv6, 0);
    will_return(__wrap_OS_ConnectTCP, 5);
    // Connect SSL
    expect_value(__wrap_SSL_new, ctx, ctx);
    cfg->ssl = __real_SSL_new(ctx);
    will_return(__wrap_SSL_new, cfg->ssl);
    will_return(__wrap_SSL_connect, -1);

    expect_value(__wrap_SSL_get_error, i, -1);
    will_return(__wrap_SSL_get_error, 100);
    expect_string(__wrap__merror, formatted_msg, "SSL error (100). Connection refused by the manager. Maybe the port specified is incorrect. Exiting.");

    int ret = w_enrollment_connect(cfg, cfg->target_cfg->manager_name);
    assert_int_equal(ret, ENROLLMENT_CONNECTION_FAILURE);
}

void test_w_enrollment_connect_success(void **state) {
    w_enrollment_ctx *cfg = *state; 
    SSL_CTX *ctx = get_ssl_context(DEFAULT_CIPHERS, 0);
    // GetHost
    expect_string(__wrap_OS_GetHost, host, cfg->target_cfg->manager_name);
    will_return(__wrap_OS_GetHost, strdup("127.0.0.1"));
    // os_ssl_keys
    expect_value(__wrap_os_ssl_keys, is_server, 0);
    expect_value(__wrap_os_ssl_keys, os_dir, NULL);
    expect_string(__wrap_os_ssl_keys, ciphers, DEFAULT_CIPHERS);
    expect_string(__wrap_os_ssl_keys, cert, "CERT");
    expect_string(__wrap_os_ssl_keys, key, "KEY");
    expect_string(__wrap_os_ssl_keys, ca_cert, "CA_CERT");
    expect_value(__wrap_os_ssl_keys, auto_method, 0);
    will_return(__wrap_os_ssl_keys, ctx);
    // OS_ConnectTCP
    expect_value(__wrap_OS_ConnectTCP, _port, 1234);
    expect_string(__wrap_OS_ConnectTCP, _ip, "127.0.0.1");
    expect_value(__wrap_OS_ConnectTCP, ipv6, 0);
    will_return(__wrap_OS_ConnectTCP, 5);
    // Connect SSL
    expect_value(__wrap_SSL_new, ctx, ctx);
    cfg->ssl = __real_SSL_new(ctx);
    will_return(__wrap_SSL_new, cfg->ssl);
    will_return(__wrap_SSL_connect, 1);

    expect_string(__wrap__minfo, formatted_msg, "Connected to 127.0.0.1:1234");

    // verify_ca_certificate
    expect_value(__wrap_check_x509_cert, ssl, cfg->ssl);
    expect_string(__wrap_check_x509_cert, manager, cfg->target_cfg->manager_name);
    will_return(__wrap_check_x509_cert, VERIFY_TRUE);
    expect_string(__wrap__minfo, formatted_msg, "Verifying manager's certificate");
    expect_string(__wrap__minfo, formatted_msg, "Manager has been verified successfully");

    int ret = w_enrollment_connect(cfg, cfg->target_cfg->manager_name);
    assert_int_equal(ret, 5);
}

/**********************************************/
/********** w_enrollment_send_message *******/

void test_w_enrollment_send_message_empty_config(void **state) {
    expect_assert_failure(w_enrollment_send_message(NULL));
}

void test_w_enrollment_send_message_wrong_hostname(void **state) {
    w_enrollment_ctx *cfg = *state; 
    #ifdef WIN32
        will_return(wrap_enrollment_op_gethostname, NULL);
        will_return(wrap_enrollment_op_gethostname, -1);
    #else 
        will_return(__wrap_gethostname, NULL);
        will_return(__wrap_gethostname, -1);
    #endif
    expect_string(__wrap__merror, formatted_msg, "Unable to extract hostname. Custom agent name not set.");
    int ret = w_enrollment_send_message(cfg);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_send_message_invalid_hostname(void **state) {
    w_enrollment_ctx *cfg = *state; 
    expect_string(__wrap__merror, formatted_msg, "Invalid agent name \"Invalid\'!@Hostname\'\". Please pick a valid name.");
    int ret = w_enrollment_send_message(cfg);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_send_message_fix_invalid_hostname(void **state) {
    w_enrollment_ctx *cfg = *state; 
    #ifdef WIN32
        will_return(wrap_enrollment_op_gethostname, "Invalid\'!@Hostname\'");
        will_return(wrap_enrollment_op_gethostname, 0);
    #else 
        will_return(__wrap_gethostname, "Invalid\'!@Hostname\'");
        will_return(__wrap_gethostname, 0);
    #endif
    // If gethostname returns an invalid string should be fixed by OS_ConvertToValidAgentName
    expect_string(__wrap__minfo, formatted_msg, "Using agent name as: InvalidHostname");
    expect_value(__wrap_SSL_write, ssl, cfg->ssl);
    expect_string(__wrap_SSL_write, buf, "OSSEC A:'InvalidHostname' IP:'src'\n");
    will_return(__wrap_SSL_write, -1);
    expect_string(__wrap__merror, formatted_msg, "SSL write error (unable to send message.)");
    expect_string(__wrap__merror, formatted_msg, "If Agent verification is enabled, agent key and certifiates are required!");
    int ret = w_enrollment_send_message(cfg);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_send_message_ssl_error(void **state) {
    w_enrollment_ctx *cfg = *state; 
    #ifdef WIN32
        will_return(wrap_enrollment_op_gethostname, "host.name");
        will_return(wrap_enrollment_op_gethostname, 0);
    #else
        will_return(__wrap_gethostname, "host.name");
        will_return(__wrap_gethostname, 0);
    #endif
    expect_string(__wrap__minfo, formatted_msg, "Using agent name as: host.name");
    expect_value(__wrap_SSL_write, ssl, cfg->ssl);
    expect_string(__wrap_SSL_write, buf, "OSSEC A:'host.name' IP:'src'\n");
    will_return(__wrap_SSL_write, -1);
    expect_string(__wrap__merror, formatted_msg, "SSL write error (unable to send message.)");
    expect_string(__wrap__merror, formatted_msg, "If Agent verification is enabled, agent key and certifiates are required!");
    int ret = w_enrollment_send_message(cfg);
    assert_int_equal(ret, -1);

}

void test_w_enrollment_send_message_success(void **state) {
    w_enrollment_ctx *cfg = *state; 
    expect_string(__wrap__minfo, formatted_msg, "Using agent name as: test_agent");
    expect_string(__wrap_OS_IsValidIP, ip_address, "192.168.1.1");
    expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
    will_return(__wrap_OS_IsValidIP, 1);
    expect_value(__wrap_SSL_write, ssl, cfg->ssl);
    expect_string(__wrap_SSL_write, buf, "OSSEC PASS: test_password OSSEC A:'test_agent' G:'test_group' IP:'192.168.1.1'\n");
    will_return(__wrap_SSL_write, 0);
    expect_string(__wrap__minfo, formatted_msg,"Request sent to manager");
    int ret = w_enrollment_send_message(cfg);
    assert_int_equal(ret, 0);
}
/**********************************************/
/********** w_enrollment_send_message *******/
void test_w_enrollment_store_key_entry_null_key(void **state) {
    expect_assert_failure(w_enrollment_store_key_entry(NULL));
}

void test_w_enrollment_store_key_entry_cannot_open(void **state) {
    const char* key_string = "KEY EXAMPLE STRING";
    char key_file[1024];
    expect_string(__wrap_fopen, filename, KEYSFILE_PATH);
    expect_string(__wrap_fopen, mode, "w");
    will_return(__wrap_fopen, 0);
    snprintf(key_file, 1024, "Unable to open key file: %s", KEYSFILE_PATH);
    expect_string(__wrap__merror, formatted_msg, key_file);
    int ret = w_enrollment_store_key_entry(key_string);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_store_key_entry_success(void **state) {
    FILE file;
    const char* key_string = "KEY EXAMPLE STRING";
    expect_string(__wrap_fopen, filename, KEYSFILE_PATH);
    expect_string(__wrap_fopen, mode, "w");
    will_return(__wrap_fopen, &file);
    #ifdef WIN32 
        expect_value(wrap_enrollment_op_fprintf, stream, &file);
        expect_string(wrap_enrollment_op_fprintf, formatted_msg, "KEY EXAMPLE STRING\n");
    #else
        expect_value(__wrap_fprintf, stream, &file);
        expect_string(__wrap_fprintf, formatted_msg, "KEY EXAMPLE STRING\n"); 
    #endif
    int ret = w_enrollment_store_key_entry(key_string);
    assert_int_equal(ret, 0);
}
/**********************************************/
/********** w_enrollment_send_message *******/
void test_w_enrollment_process_agent_key_empty_buff(void **state) {
    expect_assert_failure(w_enrollment_process_agent_key(NULL));
}

void test_w_enrollment_process_agent_key_short_buff(void **state) {
    expect_assert_failure(w_enrollment_process_agent_key("short"));
}

void test_w_enrollment_process_agent_key_invalid_format(void **state) {
    char key[] = "OSSEC KEY WRONG FORMAT";
    expect_string(__wrap__merror, formatted_msg, "Invalid keys format received.");
    w_enrollment_process_agent_key(key);
}

void test_w_enrollment_process_agent_key_invalid_key(void **state) {
    char key[] = "OSSEC K:'006 ubuntu1610 NOT_AN_IP 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f'\n\n";
    expect_string(__wrap_OS_IsValidIP, ip_address, "NOT_AN_IP");
    expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
    will_return(__wrap_OS_IsValidIP, 0);
    expect_string(__wrap__merror, formatted_msg, "One of the received key parameters does not have a valid format.");
    int ret = w_enrollment_process_agent_key(key);
    assert_int_equal(ret,-1);
}

void test_w_enrollment_process_agent_key_valid_key(void **state) {
    char key[] = "OSSEC K:'006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f'\n\n";
    expect_string(__wrap_OS_IsValidIP, ip_address, "192.168.1.1");
    expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
    will_return(__wrap_OS_IsValidIP, 1);
    expect_string(__wrap_fopen, filename, KEYSFILE_PATH);
    expect_string(__wrap_fopen, mode, "w");
    will_return(__wrap_fopen, 4);
    #ifdef WIN32
        expect_value(wrap_enrollment_op_fprintf, stream, 4);
        expect_string(wrap_enrollment_op_fprintf, formatted_msg, "006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f\n");
    #else
        expect_value(__wrap_fprintf, stream, 4);
        expect_string(__wrap_fprintf, formatted_msg, "006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f\n");
    #endif
    expect_string(__wrap__minfo, formatted_msg, "Valid key created. Finished.");
    int ret = w_enrollment_process_agent_key(key);
    assert_int_equal(ret,0);
}
/**********************************************/
/******* w_enrollment_process_response ********/
void test_w_enrollment_process_response_ssl_null(void **state) {
    expect_assert_failure(w_enrollment_process_response(NULL));
}

void test_w_enrollment_process_response_ssl_error(void **state) {
     SSL *ssl = *state;
    expect_string(__wrap__minfo, formatted_msg, "Waiting for manager reply");
    expect_value(__wrap_SSL_read, ssl, ssl);
    will_return(__wrap_SSL_read, "");
    will_return(__wrap_SSL_read, -1);
    expect_value(__wrap_SSL_get_error, i, -1);
    will_return(__wrap_SSL_get_error, SSL_ERROR_WANT_READ);
    expect_string(__wrap__merror, formatted_msg, "SSL read (unable to receive message)");
     expect_string(__wrap__merror, formatted_msg, "If Agent verification is enabled, agent key and certifiates may be incorrect!");
    int ret = w_enrollment_process_response(ssl);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_process_response_message_error(void **state) {
    SSL *ssl = *state;
    expect_string(__wrap__minfo, formatted_msg, "Waiting for manager reply");
    expect_value(__wrap_SSL_read, ssl, ssl);
    will_return(__wrap_SSL_read, "ERROR: Unable to add agent.\n\n");
    will_return(__wrap_SSL_read, strlen("ERROR: Unable to add agent.\n\n"));
    expect_string(__wrap__merror, formatted_msg, "Unable to add agent.\n\n (from manager)");
    expect_value(__wrap_SSL_read, ssl, ssl);
    will_return(__wrap_SSL_read, "");
    will_return(__wrap_SSL_read, 0);
    expect_value(__wrap_SSL_get_error, i, 0);
    will_return(__wrap_SSL_get_error, SSL_ERROR_NONE);
    expect_string(__wrap__minfo, formatted_msg, "Connection closed.");
    int ret = w_enrollment_process_response(ssl);
    assert_int_equal(ret, -1);
}

void test_w_enrollment_process_response_success(void **state) { 
    const char *string = "OSSEC K:'006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f'\n\n";
    SSL *ssl = *state;
    expect_string(__wrap__minfo, formatted_msg, "Waiting for manager reply");
    expect_value(__wrap_SSL_read, ssl, ssl);
    will_return(__wrap_SSL_read, string);
    will_return(__wrap_SSL_read, strlen(string));
    expect_string(__wrap__minfo, formatted_msg, "Received response with agent key");
    // w_enrollment_process_agent_key
    {
        expect_string(__wrap_OS_IsValidIP, ip_address, "192.168.1.1");
        expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
        will_return(__wrap_OS_IsValidIP, 1);
        expect_string(__wrap_fopen, filename, KEYSFILE_PATH);
        expect_string(__wrap_fopen, mode, "w");
        will_return(__wrap_fopen, 4);
        #ifdef WIN32
            expect_value(wrap_enrollment_op_fprintf, stream, 4);
            expect_string(wrap_enrollment_op_fprintf, formatted_msg, "006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f\n");
        #else 
            expect_value(__wrap_fprintf, stream, 4);
            expect_string(__wrap_fprintf, formatted_msg, "006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f\n");
        #endif
        expect_string(__wrap__minfo, formatted_msg, "Valid key created. Finished.");
    }
    expect_value(__wrap_SSL_get_error, i, strlen(string));
    will_return(__wrap_SSL_get_error, SSL_ERROR_NONE);
    expect_string(__wrap__minfo, formatted_msg, "Connection closed.");

    int ret = w_enrollment_process_response(ssl);
    assert_int_equal(ret, 0);
}
/**********************************************/
/******* w_enrollment_request_key ********/
void test_w_enrollment_request_key_null_cfg(void **state) {
    expect_assert_failure(w_enrollment_request_key(NULL, "server_adress"));
}

void test_w_enrollment_request_key(void **state) {
    w_enrollment_ctx *cfg = *state; 
    SSL_CTX *ctx = get_ssl_context(DEFAULT_CIPHERS, 0);
    
    // w_enrollment_connect
    {
        // GetHost
        expect_string(__wrap_OS_GetHost, host, cfg->target_cfg->manager_name);
        will_return(__wrap_OS_GetHost, strdup("192.168.1.1"));
        // os_ssl_keys
        expect_value(__wrap_os_ssl_keys, is_server, 0);
        expect_value(__wrap_os_ssl_keys, os_dir, NULL);
        expect_string(__wrap_os_ssl_keys, ciphers, DEFAULT_CIPHERS);
        expect_string(__wrap_os_ssl_keys, cert, "CERT");
        expect_string(__wrap_os_ssl_keys, key, "KEY");
        expect_string(__wrap_os_ssl_keys, ca_cert, "CA_CERT");
        expect_value(__wrap_os_ssl_keys, auto_method, 0);
        will_return(__wrap_os_ssl_keys, ctx);
        // OS_ConnectTCP
        expect_value(__wrap_OS_ConnectTCP, _port, 1234);
        expect_string(__wrap_OS_ConnectTCP, _ip, "192.168.1.1");
        expect_value(__wrap_OS_ConnectTCP, ipv6, 0);
        will_return(__wrap_OS_ConnectTCP, 5);
        // Connect SSL
        expect_value(__wrap_SSL_new, ctx, ctx);
        cfg->ssl = __real_SSL_new(ctx);
        will_return(__wrap_SSL_new, cfg->ssl);
        will_return(__wrap_SSL_connect, 1);

        expect_string(__wrap__minfo, formatted_msg, "Connected to 192.168.1.1:1234");

        // verify_ca_certificate
        expect_value(__wrap_check_x509_cert, ssl, cfg->ssl);
        expect_string(__wrap_check_x509_cert, manager, cfg->target_cfg->manager_name);
        will_return(__wrap_check_x509_cert, VERIFY_TRUE);
        expect_string(__wrap__minfo, formatted_msg, "Verifying manager's certificate");
        expect_string(__wrap__minfo, formatted_msg, "Manager has been verified successfully");
    }
    // w_enrollment_send_message
    {
        expect_string(__wrap__minfo, formatted_msg, "Using agent name as: test_agent");
        expect_string(__wrap_OS_IsValidIP, ip_address, "192.168.1.1");
        expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
        will_return(__wrap_OS_IsValidIP, 1);
        expect_value(__wrap_SSL_write, ssl, cfg->ssl);
        expect_string(__wrap_SSL_write, buf, "OSSEC PASS: test_password OSSEC A:'test_agent' G:'test_group' IP:'192.168.1.1'\n");
        will_return(__wrap_SSL_write, 0);
        expect_string(__wrap__minfo, formatted_msg,"Request sent to manager");
    }
    // w_enrollment_process_repsonse
    {
        const char *string = "OSSEC K:'006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f'\n\n";
        expect_string(__wrap__minfo, formatted_msg, "Waiting for manager reply");
        expect_value(__wrap_SSL_read, ssl, cfg->ssl);
        will_return(__wrap_SSL_read, string);
        will_return(__wrap_SSL_read, strlen(string));
        expect_string(__wrap__minfo, formatted_msg, "Received response with agent key");
        // w_enrollment_process_agent_key
        {
            FILE file;
            expect_string(__wrap_OS_IsValidIP, ip_address, "192.168.1.1");
            expect_value(__wrap_OS_IsValidIP, final_ip, NULL);
            will_return(__wrap_OS_IsValidIP, 1);
            expect_string(__wrap_fopen, filename, KEYSFILE_PATH);
            expect_string(__wrap_fopen, mode, "w");
            will_return(__wrap_fopen, &file);
            #ifdef WIN32
                expect_value(wrap_enrollment_op_fprintf, stream, &file);
                expect_string(wrap_enrollment_op_fprintf, formatted_msg, "006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f\n");
            #else
                expect_value(__wrap_fprintf, stream, &file);
                expect_string(__wrap_fprintf, formatted_msg, "006 ubuntu1610 192.168.1.1 95fefb8f0fe86bb8121f3f5621f2916c15a998728b3d50479aa64e6430b5a9f\n");
            #endif
            expect_string(__wrap__minfo, formatted_msg, "Valid key created. Finished.");
        }
        expect_value(__wrap_SSL_get_error, i, strlen(string));
        will_return(__wrap_SSL_get_error, SSL_ERROR_NONE);
        expect_string(__wrap__minfo, formatted_msg, "Connection closed.");
    }
    int ret = w_enrollment_request_key(cfg, NULL);
    assert_int_equal(ret, 0);
}
/**********************************************/
int main()
{
    const struct CMUnitTest tests[] = 
    {
        // w_enrollment_concat_src_ip
        cmocka_unit_test_setup_teardown(test_w_enrollment_concat_src_ip_invalid_ip, test_setup_concats, test_teardown_concats),
        cmocka_unit_test_setup_teardown(test_w_enrollment_concat_src_ip_valid_ip, test_setup_concats, test_teardown_concats),
        cmocka_unit_test_setup_teardown(test_w_enrollment_concat_src_ip_empty_ip, test_setup_concats, test_teardown_concats),
        cmocka_unit_test(test_w_enrollment_concat_src_ip_empty_buff),
        // w_enrollment_concat_group
        cmocka_unit_test(test_w_enrollment_concat_group_empty_buff),
        cmocka_unit_test_setup_teardown(test_w_enrollment_concat_group_empty_group, test_setup_concats, test_teardown_concats),
        cmocka_unit_test_setup_teardown(test_w_enrollment_concat_group, test_setup_concats, test_teardown_concats),
        //  w_enrollment_verify_ca_certificate
        cmocka_unit_test_setup_teardown(test_w_enrollment_verify_ca_certificate_null_connection, test_setup_ssl_context, test_teardow_ssl_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_verify_ca_certificate_no_certificate, test_setup_ssl_context, test_teardow_ssl_context),
        cmocka_unit_test_setup_teardown(test_verificy_ca_certificate_invalid_certificate, test_setup_ssl_context, test_teardow_ssl_context),
        cmocka_unit_test_setup_teardown(test_verificy_ca_certificate_valid_certificate, test_setup_ssl_context, test_teardow_ssl_context),
        // w_enrollment_connect
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_empty_address, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_empty_config, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_invalid_hostname, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_could_not_setup, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_socket_error, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_SSL_connect_error, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_connect_success, test_setup_context, test_teardown_context),
        // w_enrollment_send_message
        cmocka_unit_test(test_w_enrollment_send_message_empty_config),
        cmocka_unit_test_setup_teardown(test_w_enrollment_send_message_wrong_hostname, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_send_message_invalid_hostname, test_setup_context_3, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_send_message_fix_invalid_hostname, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_send_message_ssl_error, test_setup_context, test_teardown_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_send_message_success, test_setup_context_2, test_teardown_context),
        // w_enrollment_store_key_entry
        cmocka_unit_test_setup_teardown(test_w_enrollment_store_key_entry_null_key, setup_file_ops, teardown_file_ops),
        cmocka_unit_test_setup_teardown(test_w_enrollment_store_key_entry_cannot_open, setup_file_ops, teardown_file_ops),
        cmocka_unit_test_setup_teardown(test_w_enrollment_store_key_entry_success, setup_file_ops, teardown_file_ops),
        // w_enrollment_process_agent_key
        cmocka_unit_test(test_w_enrollment_process_agent_key_empty_buff),
        cmocka_unit_test(test_w_enrollment_process_agent_key_short_buff),
        cmocka_unit_test(test_w_enrollment_process_agent_key_invalid_format),
        cmocka_unit_test(test_w_enrollment_process_agent_key_invalid_key),
        cmocka_unit_test_setup_teardown(test_w_enrollment_process_agent_key_valid_key, setup_file_ops, teardown_file_ops),
        // w_enrollment_process_response
        cmocka_unit_test(test_w_enrollment_process_response_ssl_null),
        cmocka_unit_test_setup_teardown(test_w_enrollment_process_response_ssl_error, test_setup_ssl_context, test_teardow_ssl_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_process_response_message_error, test_setup_ssl_context, test_teardow_ssl_context),
        cmocka_unit_test_setup_teardown(test_w_enrollment_process_response_success, test_setup_ssl_context, test_teardow_ssl_context),
        // w_enrollment_request_key (wrapper)
        cmocka_unit_test(test_w_enrollment_request_key_null_cfg),
        cmocka_unit_test_setup_teardown(test_w_enrollment_request_key, test_setup_w_enrolment_request_key, test_teardown_w_enrolment_request_key)  
    };

    return cmocka_run_group_tests(tests, NULL, NULL);

}