/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  doubaokun@gmail.com                                         |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_rxserver.h"

#include "uv.h"

/* If you declare any globals in php_rxserver.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(rxserver)
*/

/* True global resources - no need for thread safety here */

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("rxserver.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_rxserver_globals, rxserver_globals)
    STD_PHP_INI_ENTRY("rxserver.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_rxserver_globals, rxserver_globals)
PHP_INI_END()
*/
/* }}} */

char response_tpl[] = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"Hello %s.\r\n";

char *response;

typedef struct child_worker {
    uv_pipe_t pipe_handle;
    int socket_fds[2];
} child_worker;

child_worker *workers;

int counter = 0;
int total_cpu_count;

static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) 
{
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void on_write(uv_write_t *req, int status)
{
  if (status)
  {
    fprintf(stderr, "Write error %s.\n", uv_strerror(status));
  }
  free(req);
}

static void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    if (nread < 0) {
        return uv_close((uv_handle_t *) stream, NULL);
    }

    if (nread > 0) {
        //printf("%u, read: %zd\n", buf->base[0], nread);
    }

    int len = strlen(response);
    uv_buf_t wrbuf = uv_buf_init(response, len);
    uv_write_t *wreq = (uv_write_t*) malloc(sizeof(uv_write_t));
    uv_write(wreq, stream, &wrbuf, 1, on_write);

    //fprintf(stdout, "receive pid: %d, Buf %s\n", getpid(), buf->base);
    free(buf->base);
    // close the connection
    uv_close((uv_handle_t *) stream, NULL);

}

static void ipc_send_tcp_connection_to_work_cb(uv_write_t* req, int status)
{
    free(req);
    if (status)  {
        fprintf(stderr,
                "IPC uv_write2 error: %s - %s\n",
                uv_err_name(status),
                uv_strerror(status));
    }
}


static void connection_cb(uv_stream_t* server, int status)
{
    if (status) {
        fprintf(stderr, "connection error: %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(server->loop, client);
    int ret = uv_accept(server, (uv_stream_t *) client);

    if (ret) {
        fprintf(stderr, "uv_accept error: %s\n", uv_strerror(ret));
        uv_close((uv_handle_t*) client, NULL);
        return;
    }

    uv_write_t *write_req = (uv_write_t*) malloc(sizeof(uv_write_t));
    uv_buf_t dummy_buf = uv_buf_init(".", 1);

    struct child_worker *worker = &workers[counter];

    uv_write2(write_req, (uv_stream_t*) &worker->pipe_handle, &dummy_buf, 1, (uv_stream_t*) client, ipc_send_tcp_connection_to_work_cb);
    counter = (counter + 1) % total_cpu_count;
    uv_close((uv_handle_t *) client, NULL);
}

void on_new_connection(uv_stream_t *q, ssize_t nread, const uv_buf_t *buf) 
{

    //fprintf(stderr, "Worker %d %s\n", getpid(), buf->base);

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "pid: %d, Read error %s\n", getpid(), uv_err_name(nread));
        uv_close((uv_handle_t*) q, NULL);
        return;
    }

    uv_pipe_t *pipe = (uv_pipe_t*) q;
    if (!uv_pipe_pending_count(pipe)) {
        fprintf(stderr, "No pending count\n");
        return;
    }

    uv_handle_type pending = uv_pipe_pending_type(pipe);
    assert(pending == UV_TCP);

    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(q->loop, client);
    if (uv_accept(q, (uv_stream_t*) client) == 0) {
        uv_os_fd_t fd;
        uv_fileno((const uv_handle_t*) client, &fd);
        //fprintf(stderr, "Worker %d: Accepted fd %d\n", getpid(), fd);
        uv_read_start((uv_stream_t*) client, alloc_cb, read_cb);
    }
    else {
        uv_close((uv_handle_t*) client, NULL);
    }
}

PHP_FUNCTION(echo_server_run)
{
    char *name;
    size_t name_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
        RETURN_NULL();
    }

    spprintf(&response, 0, response_tpl, name);

    uv_loop_t *loop = uv_default_loop();

    // get CPU count
    uv_cpu_info_t *info;
    int cpu_count;
    uv_cpu_info(&info, &cpu_count);
    uv_free_cpu_info(info, cpu_count);

    total_cpu_count = cpu_count;

    fprintf(stdout, "[master] pid: %d, start server, cpu count: %d\n", getpid(), cpu_count);

    workers = (child_worker*) calloc(cpu_count, sizeof(struct child_worker));

    while (cpu_count--) {
      struct child_worker *worker = &workers[cpu_count];

      // create socket pair
      socketpair(AF_UNIX, SOCK_STREAM, 0, worker->socket_fds);
      uv_pipe_init(loop, &worker->pipe_handle, 1);

      pid_t child_pid = fork();

      if (child_pid != 0) {
        /* parent */
        uv_pipe_open(&worker->pipe_handle, worker->socket_fds[1]);

      } else {
        /* child */
        uv_loop_fork(loop);

        uv_pipe_open(&worker->pipe_handle, worker->socket_fds[0]);

        uv_read_start((uv_stream_t *)&worker->pipe_handle, alloc_cb, on_new_connection);
        
        uv_run(loop, UV_RUN_DEFAULT);

        return;

      }


    }

    // tcp server
    uv_tcp_t server4;
    uv_tcp_init(loop, &server4);

    struct sockaddr_in bind_addr4 = {};
    uv_ip4_addr("0.0.0.0", 7001, &bind_addr4);
    uv_tcp_bind(&server4, (const struct sockaddr*) &bind_addr4, 0);

    int ret = uv_listen((uv_stream_t *)&server4, 128, connection_cb);

    if (ret) {
        fprintf(stderr, "uv_listen ipv4 error: %s\n", uv_strerror(ret));
        return;
    }

    uv_run(loop, UV_RUN_DEFAULT);

}

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_rxserver_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_rxserver_compiled)
{
	char *arg = NULL;
	size_t arg_len, len;
	zend_string *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	strg = strpprintf(0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "rxserver", arg);

	RETURN_STR(strg);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_rxserver_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_rxserver_init_globals(zend_rxserver_globals *rxserver_globals)
{
	rxserver_globals->global_value = 0;
	rxserver_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(rxserver)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(rxserver)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(rxserver)
{
#if defined(COMPILE_DL_RXSERVER) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(rxserver)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(rxserver)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "rxserver support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ rxserver_functions[]
 *
 * Every user visible function must have an entry in rxserver_functions[].
 */
const zend_function_entry rxserver_functions[] = {
	PHP_FE(confirm_rxserver_compiled,	NULL)		/* For testing, remove later. */
    PHP_FE(echo_server_run,   NULL)       /* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in rxserver_functions[] */
};
/* }}} */

/* {{{ rxserver_module_entry
 */
zend_module_entry rxserver_module_entry = {
	STANDARD_MODULE_HEADER,
	"rxserver",
	rxserver_functions,
	PHP_MINIT(rxserver),
	PHP_MSHUTDOWN(rxserver),
	PHP_RINIT(rxserver),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(rxserver),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(rxserver),
	PHP_RXSERVER_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_RXSERVER
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(rxserver)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
