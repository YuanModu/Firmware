/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/*
 * This file is a modified version of the lwIP web server demo. The original
 * author is unknown because the file didn't contain any license information.
 */

/**
 * @file web.c
 * @brief HTTP server wrapper thread code.
 * @addtogroup WEB_THREAD
 * @{
 */

#define _GNU_SOURCE
#include <string.h>

#include "ch.h"

#include "hal.h" /* chprintf */
#include "chprintf.h" /* chprintf */

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "web.h"

#include "jsmn.h"

#include "ui.h"

#if LWIP_NETCONN

#define BUFFER_SIZE 256
#define HEADER_COUNT 16
#define HEADER_NAME_SIZE 32
#define HEADER_VALUE_SIZE 128
#define JS_VALUE_SIZE 16
#define REQUEST_BODY_SIZE 1024
#define REQUEST_URL_SIZE 128
#define REQUEST_METHOD_SIZE 8
#define REQUEST_PROTOCOL_SIZE 8
#define VIEW_PATH_SIZE 128

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef struct string {
  char *data;
  int len;
} string_t;

typedef struct header {
  char *name;
  char *value;
  struct header *next;
} header_t;

typedef struct request {
  char *method;
  char *url;
  char *protocol;
  header_t *headers;
  char *body;
} request_t;

typedef struct response {
  string_t *head;
  string_t *body;
} response_t;

typedef struct view {
	char path[VIEW_PATH_SIZE];
  file_t *file;
  response_t *response;
	struct view * (*get_handler)(struct view *);
	struct view * (*post_handler)(struct view *);
} view_t;

typedef struct jspair {
  char *name;
  char *value;
} jspair_t;

static header_t headers[] = {
  [0 ... (HEADER_COUNT - 1)] = (header_t) {
    .name = (char [HEADER_NAME_SIZE]) {'\0'},
    .value = (char [HEADER_VALUE_SIZE]) {'\0'},
    .next = NULL,
  }
};

static string_t *head_buffer = &(string_t) {
  .data = (char [BUFFER_SIZE]) {'\0'},
  .len = 0,
};

static string_t *body_buffer = &(string_t) {
  .data = (char [BUFFER_SIZE]) {'\0'},
  .len = 0,
};

static string_t *file = &(string_t) {
  .data = NULL,
  .len = 0,
};

static request_t *request = &(request_t) {
  .method = (char [REQUEST_METHOD_SIZE]) {'\0'},
  .url = (char [REQUEST_URL_SIZE]) {'\0'},
  .protocol = (char [REQUEST_PROTOCOL_SIZE]) {'\0'},
  .headers = NULL,
  .body = (char [REQUEST_BODY_SIZE]) {'\0'},
};

static response_t *response = &(response_t) {
  .head = NULL,
  .body = NULL,
};

static char js[256];

static jsmn_parser p;

static jsmntok_t t[128];

static const char *request_header_get(const char * c) {
  header_t *h = headers;
  while (h) {
    if (memcmp(h->name, c, strlen(c)) == 0) {
      return h->value;
    }
    h = h->next;
  }
  return NULL;
}

static void json_get(const char *raw) {
  while (*raw != '{' && *raw != '\0') {
    raw++;
  }

  size_t json_len = strcspn(raw, "}");
  memcpy(js, raw, json_len + 1);
  js[json_len + 1] = '\0';
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static view_t *http_handle_static(view_t *view) {
  head_buffer->len = chsnprintf(head_buffer->data, BUFFER_SIZE,
    "HTTP/1.1 200\r\n"
    "Content-Type: %s\r\n"
    "Connection: close\r\n"
    "\r\n"
    ,view->file->type
  );

  file->data = (char *)view->file->data;
  file->len = *(view->file->len);

  response->head = head_buffer;
  response->body = file;
  view->response = response;
  return view;
}

static view_t *http_handle_status(view_t *view) {
  head_buffer->len = chsnprintf(head_buffer->data, BUFFER_SIZE,
    "HTTP/1.1 200\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "\r\n"
  );

  body_buffer->len= chsnprintf(body_buffer->data, BUFFER_SIZE,
    "Handle Status\r\n"
  );

  response->head = head_buffer;
  response->body = body_buffer;
  view->response = response;
  return view;
}

static view_t *http_handle_profile_get(view_t *view) {
  head_buffer->len = chsnprintf(head_buffer->data, BUFFER_SIZE,
    "HTTP/1.1 200\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "\r\n"
  );

  body_buffer->len= chsnprintf(body_buffer->data, BUFFER_SIZE,
    "Profile Get\r\n"
  );

  response->head = head_buffer;
  response->body = body_buffer;
  view->response = response;
  return view;
}

static view_t *http_handle_profile_post(view_t *view) {
  head_buffer->len = chsnprintf(head_buffer->data, BUFFER_SIZE,
    "HTTP/1.1 200\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "\r\n"
  );


  json_get(request->body);

  jsmn_init(&p);
  int r = jsmn_parse(&p, js, strlen(js), t, ARRAY_SIZE(t));

  jspair_t *pair = &(jspair_t){
    .name = "user",
    .value = (char [JS_VALUE_SIZE]) {'\0'},
  };

  for (int i=0; i<r; i++) {
    if (jsoneq(js, &t[i], pair->name) == 0) {
      memcpy(pair->value, (js + t[i + 1].start), (t[i + 1].end - t[i + 1].start));
      pair->value[(t[i + 1].end - t[i + 1].start)] = '\0';
      i++;
    }
  }
  body_buffer->len= chsnprintf(body_buffer->data, BUFFER_SIZE,
    "{"
    "\"%s\": \"%s\""
    "}"
    ,pair->name
    ,pair->value
  );

  response->head = head_buffer;
  response->body = body_buffer;
  view->response = response;
  return view;
}

extern file_t file_index_html;
extern file_t file_bootstrap_min_css;
extern file_t file_bootstrap_min_js;
extern file_t file_Chart_bundle_min_js;
extern file_t file_custom_js;
extern file_t file_jquery_3_4_1_min_js;
extern file_t file_popper_min_js;
static view_t views[] = {
  {
    .path = "/",
    .file = &file_index_html,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/bootstrap.min.css",
    .file = &file_bootstrap_min_css,
    .get_handler = http_handle_static,
    .post_handler = NULL
  },
  {
    .path = "/bootstrap.min.js",
    .file = &file_bootstrap_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/Chart.bundle.min.js",
    .file = &file_Chart_bundle_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/custom.js",
    .file = &file_custom_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/jquery-3.4.1.min.js",
    .file = &file_jquery_3_4_1_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/popper.min.js",
    .file = &file_popper_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL
  },
  {
    .path = "/profile",
    .file = NULL,
    .get_handler = http_handle_profile_get,
    .post_handler = http_handle_profile_post,
  },
  {
    .path = "/status",
    .file = NULL,
    .get_handler = http_handle_status,
    .post_handler = NULL,
  },
};

void request_parse(const char *raw) {
  size_t method_len = strcspn(raw, " ");
  if (memcmp(raw, "GET", strlen("GET")) == 0) {
    memcpy(request->method, "GET", strlen("GET"));
    request->method[method_len] = '\0';
  } else if (memcmp(raw, "POST", strlen("POST")) == 0) {
    memcpy(request->method, "POST", strlen("POST"));
    request->method[method_len] = '\0';
  }
  raw += method_len + 1;

  size_t url_len = strcspn(raw, " ");
  memcpy(request->url, raw, url_len);
  request->url[url_len] = '\0';
  raw += url_len + 1;

  size_t protocol_len = strcspn(raw, "\r\n");
  if (memcmp(raw, "HTTP/1.0", strlen("HTTP/1.0")) == 0) {
    memcpy(request->protocol, "HTTP/1.0", strlen("HTTP/1.0"));
    request->protocol[protocol_len] = '\0';
  } else if (memcmp(raw, "HTTP/1.1", strlen("HTTP/1.1")) == 0) {
    memcpy(request->protocol, "HTTP/1.1", strlen("HTTP/1.1"));
    request->protocol[protocol_len] = '\0';
  } else if (memcmp(raw, "HTTP/2.0", strlen("HTTP/2.0")) == 0) {
    memcpy(request->protocol, "HTTP/2.0", strlen("HTTP/2.0"));
    request->protocol[protocol_len] = '\0';
  }
  raw += protocol_len + 2;

  int i = 0;
  while (raw[0]!='\r' || raw[1]!='\n') {
    size_t name_len = strcspn(raw, ":");
    memcpy(headers[i].name, raw, name_len);
    headers[i].name[name_len] = '\0';
    raw += name_len + 1;

    while (*raw == ' ') {
      raw++;
    }

    size_t value_len = strcspn(raw, "\r\n");
    memcpy(headers[i].value, raw, value_len);
    headers[i].value[value_len] = '\0';
    raw += value_len + 2;

    headers[i].next = NULL;
    if (i > 0) {
      headers[i-1].next = &headers[i];
    }

    i++;
  }
  raw += 2;

  request->headers = headers;

  size_t body_len = strlen(raw);
  memcpy(request->body, raw, body_len);
  request->body[body_len] = '\0';
}

static void http_server_serve(struct netconn *conn) {
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void **)&buf, &buflen);
    buf[buflen] = '\0';

    request_parse((const char *)buf);

    for (unsigned int i = 0; i < ARRAY_SIZE(views); i++) {
      if (strcmp(request->url, views[i].path) == 0) {
        view_t *view = NULL;
        if ((strcmp(request->method, "GET") == 0) && views[i].get_handler) {
          view = views[i].get_handler(&views[i]);
        }
        if ((strcmp(request->method, "POST") == 0) && views[i].post_handler) {
          view = views[i].post_handler(&views[i]);
        }
        if (view) {
          netconn_write(conn,
                        view->response->head->data,
                        view->response->head->len,
                        NETCONN_NOCOPY);
          netconn_write(conn,
                        view->response->body->data,
                        view->response->body->len,
                        NETCONN_NOCOPY);
        }
      }
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}

mailbox_t mb[WEB_HELPER_THREADS];
msg_t b[WEB_HELPER_THREADS][WEB_MAILBOX_SIZE];
THD_WORKING_AREA(wa_http_helper[WEB_HELPER_THREADS], WEB_THREAD_STACK_SIZE);
THD_FUNCTION(http_helper, p) {
  int i = (int)p;
  msg_t msg;

  string_t *thread_name = &(string_t) {
    .data = (char [16]) {'\0'},
    .len = 0,
  };
  thread_name->len = chsnprintf(thread_name->data, 16, "http_%d", i);

  chRegSetThreadName(thread_name->data);

  chThdSetPriority(WEB_THREAD_PRIORITY - 1);

  chMBObjectInit(&mb[i], b[i], WEB_HELPER_THREADS);

  while (chThdShouldTerminateX() == false) {
   chMBFetchTimeout(&mb[i], &msg, TIME_INFINITE);
 }
}

THD_WORKING_AREA(wa_http_server, WEB_THREAD_STACK_SIZE);

THD_FUNCTION(http_server, p) {
  struct netconn *conn, *newconn;
  err_t err;

  (void)p;
  chRegSetThreadName("http");

  /* Create a new TCP connection handle */
  conn = netconn_new(NETCONN_TCP);
  LWIP_ERROR("http_server: invalid conn", (conn != NULL), chThdExit(MSG_RESET););

  /* Bind to port 80 (HTTP) with default IP address */
  netconn_bind(conn, NULL, WEB_THREAD_PORT);

  /* Put the connection into LISTEN state */
  netconn_listen(conn);

  /* Goes to the final priority after initialization.*/
  chThdSetPriority(WEB_THREAD_PRIORITY);

  while (true) {
    err = netconn_accept(conn, &newconn);
    if (err != ERR_OK)
      continue;
    http_server_serve(newconn);
    netconn_delete(newconn);
  }
}

#endif
