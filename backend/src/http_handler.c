/*
 * http_handler.c
 *
 *  Created on: Feb 2, 2017
 *      Author: pchero
 */

#define _GNU_SOURCE

#include <jansson.h>
#include <stdbool.h>
#include <event2/event.h>
#include <evhtp.h>

#include "common.h"
#include "slog.h"
#include "utils.h"
#include "http_handler.h"
#include "db_handler.h"
#include "resource_handler.h"

#define API_VER "0.1"

extern app* g_app;
extern struct event_base* g_base;
evhtp_t* g_htp = NULL;

// ping
static void cb_htp_ping(evhtp_request_t *req, void *a);

// peers
static void cb_htp_peers(evhtp_request_t *req, void *data);

// databases
static void cb_htp_databases(evhtp_request_t *req, void *data);
static void cb_htp_databases_key(evhtp_request_t *req, void *data);

// registries
static void cb_htp_registries(evhtp_request_t *req, void *data);
static void cb_htp_registries_detail(evhtp_request_t *req, void *data);

// queue_params
static void cb_htp_queue_params(evhtp_request_t *req, void *data);
static void cb_htp_queue_params_detail(evhtp_request_t *req, void *data);

// queue_members
static void cb_htp_queue_members(evhtp_request_t *req, void *data);
static void cb_htp_queue_members_detail(evhtp_request_t *req, void *data);

// queue entries
static void cb_htp_queue_entries(evhtp_request_t *req, void *data);
static void cb_htp_queue_entries_detail(evhtp_request_t *req, void *data);


bool init_http_handler(void)
{
  const char* tmp_const;
  const char* http_addr;
  int http_port;

  g_htp = evhtp_new(g_base, NULL);

  http_addr = json_string_value(json_object_get(json_object_get(g_app->j_conf, "general"), "http_addr"));
  tmp_const = json_string_value(json_object_get(json_object_get(g_app->j_conf, "general"), "http_port"));
  http_port = atoi(tmp_const);

  evhtp_bind_socket(g_htp, http_addr, http_port, 1024);

  // register callback
  evhtp_set_regex_cb(g_htp, "/ping", cb_htp_ping, NULL);

  // peers
  evhtp_set_regex_cb(g_htp, "/peers", cb_htp_peers, NULL);

  // databases - deprecated
  evhtp_set_regex_cb(g_htp, "/databases/", cb_htp_databases_key, NULL);
  evhtp_set_regex_cb(g_htp, "/databases", cb_htp_databases, NULL);

  // registres
  evhtp_set_regex_cb(g_htp, "/registries/", cb_htp_registries_detail, NULL);
  evhtp_set_regex_cb(g_htp, "/registries", cb_htp_registries, NULL);

  // queue_params
  evhtp_set_regex_cb(g_htp, "/queue_params/", cb_htp_queue_params_detail, NULL);
  evhtp_set_regex_cb(g_htp, "/queue_params", cb_htp_queue_params, NULL);

  // queue_members
  evhtp_set_regex_cb(g_htp, "/queue_members/", cb_htp_queue_members_detail, NULL);
  evhtp_set_regex_cb(g_htp, "/queue_members", cb_htp_queue_members, NULL);

  // queue_entries
  evhtp_set_regex_cb(g_htp, "/queue_entries/", cb_htp_queue_entries_detail, NULL);
  evhtp_set_regex_cb(g_htp, "/queue_entries", cb_htp_queue_entries, NULL);



  return true;
}

void term_http_handler(void)
{
  slog(LOG_INFO, "Terminate http handler.");
  if(g_htp != NULL) {
    evhtp_unbind_socket(g_htp);
    evhtp_free(g_htp);
  }
}

void simple_response_normal(evhtp_request_t *req, json_t* j_msg)
{
  char* res;

  if((req == NULL) || (j_msg == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired simple_response_normal.");

  res = json_dumps(j_msg, JSON_ENCODE_ANY);

  evbuffer_add_printf(req->buffer_out, "%s", res);
  evhtp_send_reply(req, EVHTP_RES_OK);
  sfree(res);

  return;
}

void simple_response_error(evhtp_request_t *req, int status_code, int err_code, const char* err_msg)
{
  char* res;
  json_t* j_tmp;
  json_t* j_res;
  int code;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired simple_response_error.");

  // create default result
  j_res = create_default_result(status_code);

  // create error
  if(err_code == 0) {
    code = status_code;
  }
  else {
    code = err_code;
  }
  j_tmp = json_pack("{s:i, s:s}",
      "code",     code,
      "message",  err_msg? : ""
      );
  json_object_set_new(j_res, "error", j_tmp);

  res = json_dumps(j_res, JSON_ENCODE_ANY);
  json_decref(j_res);

  evbuffer_add_printf(req->buffer_out, "%s", res);
  evhtp_send_reply(req, status_code);
  sfree(res);

  return;
}

json_t* create_default_result(int code)
{
  json_t* j_res;
  char* timestamp;

  timestamp = get_utc_timestamp();

  j_res = json_pack("{s:s, s:s, s:i}",
      "api_ver",    API_VER,
      "timestamp",  timestamp,
      "statuscode", code
      );
  sfree(timestamp);

  return j_res;
}

static void cb_htp_ping(evhtp_request_t *req, void *a)
{
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_ping");

  // create result
  j_tmp = json_pack("{s:s}",
      "message",  "pong"
      );

  j_res = create_default_result(EVHTP_RES_OK);
  json_object_set_new(j_res, "result", j_tmp);

  // send response
  simple_response_normal(req, j_res);
  json_decref(j_res);

  return;
}

static void cb_htp_peers(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_peers.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    j_tmp = get_peers_all_name();
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_databases(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_res;
  json_t* j_tmp;

  slog(LOG_WARNING, "Deprecated api.");
  simple_response_error(req, EVHTP_RES_NOTFOUND, 0, NULL);

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_databases.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    j_tmp = get_databases_all_key();
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_databases_key(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_data;
  json_t* j_res;
  json_t* j_tmp;
  const char* tmp_const;
  char* tmp;

  slog(LOG_WARNING, "Deprecated api.");
  simple_response_error(req, EVHTP_RES_NOTFOUND, 0, NULL);

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_databases_key.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    // get data
    tmp_const = (char*)evbuffer_pullup(req->buffer_in, evbuffer_get_length(req->buffer_in));
    if(tmp_const == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // create json
    tmp = strndup(tmp_const, evbuffer_get_length(req->buffer_in));
    slog(LOG_DEBUG, "Requested data. data[%s]", tmp);
    j_data = json_loads(tmp, JSON_DECODE_ANY, NULL);
    sfree(tmp);
    if(j_data == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // get value
    j_tmp = get_database_info(json_string_value(json_object_get(j_data, "key")));
    json_decref(j_data);
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_registries(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_registries.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    j_tmp = get_registries_all_account();
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", json_object());
    json_object_set_new(json_object_get(j_res, "result"), "list", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_registries_detail(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_data;
  json_t* j_res;
  json_t* j_tmp;
  const char* tmp_const;
  char* tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_registries_account.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    // get data
    tmp_const = (char*)evbuffer_pullup(req->buffer_in, evbuffer_get_length(req->buffer_in));
    if(tmp_const == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // create json
    tmp = strndup(tmp_const, evbuffer_get_length(req->buffer_in));
    slog(LOG_DEBUG, "Requested data. data[%s]", tmp);
    j_data = json_loads(tmp, JSON_DECODE_ANY, NULL);
    sfree(tmp);
    if(j_data == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // get value
    j_tmp = get_registry_info(json_string_value(json_object_get(j_data, "account")));
    json_decref(j_data);
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_queue_params(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_queue_params.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    j_tmp = get_queue_params_all_name();
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", json_object());
    json_object_set_new(json_object_get(j_res, "result"), "list", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_queue_params_detail(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_data;
  json_t* j_res;
  json_t* j_tmp;
  const char* tmp_const;
  char* tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_queue_params_name.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    // get data
    tmp_const = (char*)evbuffer_pullup(req->buffer_in, evbuffer_get_length(req->buffer_in));
    if(tmp_const == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // create json
    tmp = strndup(tmp_const, evbuffer_get_length(req->buffer_in));
    slog(LOG_DEBUG, "Requested data. data[%s]", tmp);
    j_data = json_loads(tmp, JSON_DECODE_ANY, NULL);
    sfree(tmp);
    if(j_data == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // get value
    j_tmp = get_queue_param_info(json_string_value(json_object_get(j_data, "name")));
    json_decref(j_data);
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_queue_members(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_queue_members.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    j_tmp = get_queue_members_all_name_queue();
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", json_object());
    json_object_set_new(json_object_get(j_res, "result"), "list", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_queue_members_detail(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_data;
  json_t* j_res;
  json_t* j_tmp;
  const char* tmp_const;
  char* tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_queue_members_detail.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    // get data
    tmp_const = (char*)evbuffer_pullup(req->buffer_in, evbuffer_get_length(req->buffer_in));
    if(tmp_const == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // create json
    tmp = strndup(tmp_const, evbuffer_get_length(req->buffer_in));
    slog(LOG_DEBUG, "Requested data. data[%s]", tmp);
    j_data = json_loads(tmp, JSON_DECODE_ANY, NULL);
    sfree(tmp);
    if(j_data == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // get value
    j_tmp = get_queue_member_info(
        json_string_value(json_object_get(j_data, "name")),
        json_string_value(json_object_get(j_data, "queue_name"))
        );
    json_decref(j_data);
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_queue_entries(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_queue_entries.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    j_tmp = get_queue_entries_all_unique_id_queue_name();
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", json_object());
    json_object_set_new(json_object_get(j_res, "result"), "list", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}

static void cb_htp_queue_entries_detail(evhtp_request_t *req, void *data)
{
  int method;
  json_t* j_data;
  json_t* j_res;
  json_t* j_tmp;
  const char* tmp_const;
  char* tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired cb_htp_queue_entries_detail.");

  // method check
  method = evhtp_request_get_method(req);
  if(method != htp_method_GET) {
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
    return;
  }

  if(method == htp_method_GET) {
    // get data
    tmp_const = (char*)evbuffer_pullup(req->buffer_in, evbuffer_get_length(req->buffer_in));
    if(tmp_const == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // create json
    tmp = strndup(tmp_const, evbuffer_get_length(req->buffer_in));
    slog(LOG_DEBUG, "Requested data. data[%s]", tmp);
    j_data = json_loads(tmp, JSON_DECODE_ANY, NULL);
    sfree(tmp);
    if(j_data == NULL) {
      simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
      return;
    }

    // get value
    j_tmp = get_queue_entry_info(
        json_string_value(json_object_get(j_data, "unique_id")),
        json_string_value(json_object_get(j_data, "queue_name"))
        );
    json_decref(j_data);
    if(j_tmp == NULL) {
      simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
      return;
    }

    // create result
    j_res = create_default_result(EVHTP_RES_OK);
    json_object_set_new(j_res, "result", j_tmp);

    simple_response_normal(req, j_res);
    json_decref(j_res);
  }
  else {
    // should not reach to here.
    simple_response_error(req, EVHTP_RES_METHNALLOWED, 0, NULL);
  }

  return;
}
