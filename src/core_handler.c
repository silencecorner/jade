
#define _GNU_SOURCE

#include <stdio.h>
#include <jansson.h>

#include "slog.h"
#include "resource_handler.h"
#include "http_handler.h"
#include "ami_action_handler.h"

#include "core_handler.h"



/**
 * GET ^/core/channels request handler.
 * @param req
 * @param data
 */
void htp_get_core_channels(evhtp_request_t *req, void *data)
{
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_INFO, "Fired htp_get_core_channels_detail.");

  j_tmp = get_channels_all();
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

  return;
}


/**
 * GET ^/core/channels/ request handler.
 * @param req
 * @param data
 */
void htp_get_core_channels_detail(evhtp_request_t *req, void *data)
{
  json_t* j_res;
  json_t* j_tmp;
  const char* uuid;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_INFO, "Fired htp_get_core_channels_detail.");

  // get channel uuid
  uuid = req->uri->path->file;
  if(uuid == NULL) {
    slog(LOG_NOTICE, "Could not get uuid info.");
    simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
    return;
  }

  // get channel info.
  j_tmp = get_channel_info(uuid);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get channel info.");
    simple_response_error(req, EVHTP_RES_NOTFOUND, 0, NULL);
    return;
  }

  // create result
  j_res = create_default_result(EVHTP_RES_OK);
  json_object_set_new(j_res, "result", j_tmp);

  simple_response_normal(req, j_res);
  json_decref(j_res);

  return;
}

/**
 * DELETE ^/core/channels/(.*) request handler.
 * @param req
 * @param data
 */
void htp_delete_core_channels_detail(evhtp_request_t *req, void *data)
{
  json_t* j_res;
  const char* unique_id;
  int ret;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_INFO, "Fired htp_delete_core_channels_detail.");

  // get channel unique_id
  unique_id = req->uri->path->file;
  if(unique_id == NULL) {
    slog(LOG_NOTICE, "Could not get uuid info.");
    simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
    return;
  }

  // send hangup
  ret = ami_action_hangup_by_uniqueid(unique_id);
  if(ret == false) {
    slog(LOG_ERR, "Could not send hangup. unique_id[%s]", unique_id);
    simple_response_error(req, EVHTP_RES_SERVERR, 0, NULL);
    return;
  }

  // create result
  j_res = create_default_result(EVHTP_RES_OK);

  simple_response_normal(req, j_res);
  json_decref(j_res);

  return;
}

/**
 * GET ^/core/systems/ request handler.
 * @param req
 * @param data
 */
void htp_get_core_systems_detail(evhtp_request_t *req, void *data)
{
  json_t* j_res;
  json_t* j_tmp;
  const char* id;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_INFO, "Fired htp_get_core_systems_detail.");

  // get channel uuid
  id = req->uri->path->file;
  if(id == NULL) {
    slog(LOG_NOTICE, "Could not get id info.");
    simple_response_error(req, EVHTP_RES_BADREQ, 0, NULL);
    return;
  }

  // get channel info.
  j_tmp = get_system_info(id);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get system info.");
    simple_response_error(req, EVHTP_RES_NOTFOUND, 0, NULL);
    return;
  }

  // create result
  j_res = create_default_result(EVHTP_RES_OK);
  json_object_set_new(j_res, "result", j_tmp);

  simple_response_normal(req, j_res);
  json_decref(j_res);

  return;
}

/**
 * GET ^/core/systems request handler.
 * @param req
 * @param data
 */
void htp_get_core_systems(evhtp_request_t *req, void *data)
{
  json_t* j_res;
  json_t* j_tmp;

  if(req == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_INFO, "Fired htp_get_core_systems.");

  j_tmp = get_systems_all();
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

  return;
}
