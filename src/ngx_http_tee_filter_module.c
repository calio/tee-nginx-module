
/*
 * Copyright (C) Jiale Zhi
 * Copyright (C) CloudFlare, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_str_t  name;
} ngx_http_tee_loc_conf_t;


typedef struct {
    ngx_file_t    file;
    ngx_chain_t  *in;
} ngx_http_tee_ctx_t;


static char *ngx_http_tee_filter(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static void *ngx_http_tee_create_conf(ngx_conf_t *cf);
static char *ngx_http_tee_merge_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_tee_filter_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_tee_filter_commands[] = {

    { ngx_string("tee"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_tee_filter,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_tee_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_tee_filter_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_tee_create_conf,              /* create location configuration */
    ngx_http_tee_merge_conf                /* merge location configuration */
};


ngx_module_t  ngx_http_tee_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_tee_filter_module_ctx,       /* module context */
    ngx_http_tee_filter_commands,          /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_int_t
ngx_http_tee_header_filter(ngx_http_request_t *r)
{
    ngx_err_t                 err;
    ngx_file_t               *file;
    ngx_http_tee_ctx_t       *ctx;
    ngx_http_tee_loc_conf_t  *tlcf;

    tlcf = ngx_http_get_module_loc_conf(r, ngx_http_tee_filter_module);

    if (tlcf->name.len == 0) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_tee_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_tee_filter_module);

    /* TODO disable this */
    r->filter_need_in_memory = 1;

    file = &ctx->file;
    file->name = tlcf->name;
    file->log = r->connection->log;
    file->fd = ngx_open_file(file->name.data, NGX_FILE_WRONLY,
                             NGX_FILE_CREATE_OR_OPEN, NGX_FILE_DEFAULT_ACCESS);

    if (file->fd == NGX_INVALID_FILE) {
        err = ngx_errno;

        ngx_log_error(NGX_LOG_ERR, r->connection->log, err,
                      ngx_open_file_n " \"%V\"", &file->name);

        return ngx_http_next_header_filter(r);
    }

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_tee_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_buf_t           *buf;
    ngx_http_tee_ctx_t  *ctx;
    off_t                offset;

    ctx = ngx_http_get_module_ctx(r, ngx_http_tee_filter_module);

    if (ctx == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    if (in == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    ctx->in = in;

    offset = 0;

    while (ctx->in) {
        buf = ctx->in->buf;

        ngx_write_chain_to_file(&ctx->file, ctx->in, offset, r->pool);
        offset += ngx_buf_size(buf);

        ctx->in = ctx->in->next;
    }

    return ngx_http_next_body_filter(r, in);
}


static char *
ngx_http_tee_filter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tee_loc_conf_t *tlcf = conf;

    ngx_str_t                         *value;

    if (tlcf->name.data) {
        return "is duplicate";
    }

    value = cf->args->elts;

    /* TODO accept nginx variable */
    tlcf->name = value[1];

    ngx_get_full_name(cf->pool, (ngx_str_t *) &ngx_cycle->prefix, &tlcf->name);

    return NGX_CONF_OK;
}


static void *
ngx_http_tee_create_conf(ngx_conf_t *cf)
{
    ngx_http_tee_loc_conf_t  *tlcf;

    tlcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_tee_loc_conf_t));
    if (tlcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->name = { 0, NULL };
     */

    return tlcf;
}


static char *
ngx_http_tee_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_tee_loc_conf_t *prev = parent;
    ngx_http_tee_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->name, prev->name, "");

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_tee_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_tee_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_tee_body_filter;

    return NGX_OK;
}
