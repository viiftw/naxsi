// SPDX-FileCopyrightText: 2022 wargio <deroad@libero.it>
// SPDX-License-Identifier: LGPL-3.0-only

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_event.h>
#include <ngx_md5.h>
#include <ngx_http_core_module.h>
#include <naxsi.h>

typedef char *(*cmd_set_cb)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

#define ngx_naxsi_return_val_on_fail(x, y) \
	do { \
		if (!(x)) { \
			return (y); \
		} \
	} while (0)

#define ngx_naxsi_error(fmt, ...) ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "naxsi: " fmt, ##__VA_ARGS__)
#define ngx_naxsi_arg(s) \
	{ (char *)s.data, s.len }
#define ngx_naxsi_memory(ppool) naxsi_memory(ppool, &nginx_naxsi_free, &nginx_naxsi_malloc, &nginx_naxsi_calloc)

static void nginx_naxsi_free(ngx_pool_t *pool, void *ptr) {
	ngx_pfree(pool, ptr);
}

static void *nginx_naxsi_malloc(ngx_pool_t *pool, size_t size) {
	return ngx_palloc(pool, size);
}

static void *nginx_naxsi_calloc(ngx_pool_t *pool, size_t nmemb, size_t size) {
	size_t total = nmemb * size;
	return ngx_pcalloc(pool, total);
}

static ngx_int_t ngx_http_naxsi_rewrite_phase_handler(ngx_http_request_t *request) {
	return NGX_ERROR;
}

static ngx_int_t ngx_http_naxsi_post_configuration(ngx_conf_t *cf) {
	ngx_http_handler_pt *rewrite;
	ngx_http_core_main_conf_t *conf;

	conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	if (!conf) {
		return NGX_ERROR;
	}

	/* register rewrite phase handler */
	rewrite = ngx_array_push(&conf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
	if (!rewrite) {
		return NGX_ERROR;
	}

	*rewrite = ngx_http_naxsi_rewrite_phase_handler;
	return NGX_OK;
}

static void *ngx_http_naxsi_create_naxsi(ngx_conf_t *cf) {
	naxsi_mem_t mem = ngx_naxsi_memory(cf->pool);
	return naxsi_new(&mem);
}

static char *ngx_http_naxsi_main_rule(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	return NGX_CONF_ERROR;
}

static char *ngx_http_naxsi_basic_rule(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	return NGX_CONF_ERROR;
}

static char *ngx_http_naxsi_denied_url(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	return NGX_CONF_ERROR;
}

static char *ngx_http_naxsi_ignore_ip_request(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	ngx_naxsi_return_val_on_fail(cf && naxsi_loc, NGX_CONF_ERROR);

	ngx_str_t *value = cf->args->elts;
	naxsi_mem_t mem = ngx_naxsi_memory(cf->pool);
	naxsi_str_t ip_address = ngx_naxsi_arg(value[1]);

	if (!naxsi_ignore_ip(&mem, naxsi_loc, &ip_address)) {
		ngx_naxsi_error("invalid " NAXSI_KEYWORD_IGNORE_IP " value: %s", (const char *)value[1].data);
		return NGX_CONF_ERROR;
	}
	return NGX_OK;
}

static char *ngx_http_naxsi_ignore_cidr_request(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	ngx_naxsi_return_val_on_fail(cf && naxsi_loc, NGX_CONF_ERROR);

	ngx_str_t *value = cf->args->elts;
	naxsi_mem_t mem = ngx_naxsi_memory(cf->pool);
	naxsi_str_t cidr = ngx_naxsi_arg(value[1]);

	if (!naxsi_ignore_cidr(&mem, naxsi_loc, &cidr)) {
		ngx_naxsi_error("invalid " NAXSI_KEYWORD_IGNORE_CIDR " value: %s", (const char *)value[1].data);
		return NGX_CONF_ERROR;
	}
	return NGX_OK;
}

static char *ngx_http_naxsi_check_rule(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	return NGX_CONF_ERROR;
}

static char *ngx_http_naxsi_flags(ngx_conf_t *cf, ngx_command_t *cmd, naxsi_t *naxsi_loc) {
	return NGX_CONF_ERROR;
}

extern ngx_module_t ngx_http_naxsi_module;

static ngx_command_t ngx_http_naxsi_commands[] = {
	/* MainRule */
	{ ngx_string(NAXSI_KEYWORD_MAIN_RULE),
		NGX_HTTP_MAIN_CONF | NGX_CONF_1MORE,
		(cmd_set_cb)ngx_http_naxsi_main_rule,
		NGX_HTTP_MAIN_CONF_OFFSET,
		0,
		NULL },

	/* BasicRule */
	{ ngx_string(NAXSI_KEYWORD_BASIC_RULE),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_1MORE,
		(cmd_set_cb)ngx_http_naxsi_basic_rule,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* DeniedUrl */
	{ ngx_string(NAXSI_KEYWORD_DENIED_URL),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_1MORE,
		(cmd_set_cb)ngx_http_naxsi_denied_url,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* IgnoreIP */
	{ ngx_string(NAXSI_KEYWORD_IGNORE_IP),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_1MORE,
		(cmd_set_cb)ngx_http_naxsi_ignore_ip_request,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* IgnoreCIDR */
	{ ngx_string(NAXSI_KEYWORD_IGNORE_CIDR),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_1MORE,
		(cmd_set_cb)ngx_http_naxsi_ignore_cidr_request,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* CheckRule */
	{ ngx_string(NAXSI_KEYWORD_CHECK_RULE),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_1MORE,
		(cmd_set_cb)ngx_http_naxsi_check_rule,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* LearningMode */
	{ ngx_string(NAXSI_KEYWORD_LEARNING_FLAG),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
		(cmd_set_cb)ngx_http_naxsi_flags,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* SecRulesEnabled */
	{ ngx_string(NAXSI_KEYWORD_ENABLED_FLAG),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
		(cmd_set_cb)ngx_http_naxsi_flags,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* SecRulesDisabled */
	{ ngx_string(NAXSI_KEYWORD_DISABLED_FLAG),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
		(cmd_set_cb)ngx_http_naxsi_flags,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* LibInjectionSql */
	{ ngx_string(NAXSI_KEYWORD_LIBINJECTION_SQL),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
		(cmd_set_cb)ngx_http_naxsi_flags,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	/* LibInjectionXss */
	{ ngx_string(NAXSI_KEYWORD_LIBINJECTION_XSS),
		NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
		(cmd_set_cb)ngx_http_naxsi_flags,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	ngx_null_command
};

static ngx_http_module_t ngx_http_naxsi_module_context = {
	NULL, /* preconfiguration */
	ngx_http_naxsi_post_configuration, /* postconfiguration */
	ngx_http_naxsi_create_naxsi, /* create main configuration */
	NULL, /* init main configuration */
	NULL, /* create server configuration */
	NULL, /* merge server configuration */
	ngx_http_naxsi_create_naxsi, /* create location configuration */
	NULL /* merge location configuration */
};

ngx_module_t ngx_http_naxsi_module = {
	NGX_MODULE_V1,
	&ngx_http_naxsi_module_context, /* module context */
	ngx_http_naxsi_commands, /* module directives */
	NGX_HTTP_MODULE, /* module type */
	NULL, /* init master */
	NULL, /* init module */
	NULL, /* init process */
	NULL, /* init thread */
	NULL, /* exit thread */
	NULL, /* exit process */
	NULL, /* exit master */
	NGX_MODULE_V1_PADDING
};
