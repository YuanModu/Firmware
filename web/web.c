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

#define MAX_BUFFER_SIZE 128
#define MAX_HEADER_COUNT 16
#define MAX_HEADER_NAME_SIZE 64
#define MAX_HEADER_VALUE_SIZE 64
#define MAX_VIEW_PATH_SIZE 128
#define MAX_REQUEST_URL_SIZE 128
#define MAX_REQUEST_BODY_SIZE 1024

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef enum method {
  METHOD_NONE,
  GET,
  POST
} method_t;

typedef enum protocol {
  PROTOCOL_NONE,
  HTTP_1_0,
  HTTP_1_1,
  HTTP_2_0
} protocol_t;

typedef enum status {
  STATUS_NONE,
  OK_200,
  NOT_FOUND_404
} status_t;

typedef enum type {
  TYPE_NONE,
  TEXT_HTML,
  TEXT_CSS,
  APPLICATION_JAVASCRIPT,
  APPLICATION_JSON
} type_t;

typedef struct buffer {
  char *data;
  int len;
} buffer_t;

typedef struct header {
  char *name;
  char *value;
  struct header *next;
} header_t;

typedef struct request {
  method_t method;
  char *url;
  protocol_t protocol;
  header_t *headers;
  char *body;
} request_t;

typedef struct response {
  const char *head;
  const int *head_len;
  const char *body;
  const int *body_len;
} response_t;

typedef struct view {
	char path[MAX_VIEW_PATH_SIZE];
  file_t *file;
  response_t *response;
	struct view * (*get_handler)(struct view *);
	struct view * (*post_handler)(struct view *);
} view_t;

static header_t headers[MAX_HEADER_COUNT];

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

static buffer_t *head_buffer = &(buffer_t) {
  .data = (char [MAX_BUFFER_SIZE]) {},
  .len = 0,
};

static buffer_t *body_buffer = &(buffer_t) {
  .data = (char [MAX_BUFFER_SIZE]) {},
  .len = 0,
};

static request_t *request = &(request_t) {
  .method = METHOD_NONE,
  .url = (char [MAX_REQUEST_URL_SIZE]) {},
  .protocol =  PROTOCOL_NONE,
  .headers = NULL,
  .body = (char [MAX_REQUEST_BODY_SIZE]) {},
};

static response_t *response = &(response_t) {
  .head = NULL,
  .head_len = NULL,
  .body = NULL,
  .body_len = NULL,
};

static char js[256];

static const char *method (method_t m) {
  static const char *methods[] = {
    [METHOD_NONE] = NULL,
    [GET] = "GET",
    [POST] = "POST",
  };
  return methods[m];
}

static const char *protocol(protocol_t p) {
  static const char *protocols[] = {
    [PROTOCOL_NONE] = NULL,
    [HTTP_1_0] = "HTTP/1.0",
    [HTTP_1_1] = "HTTP/1.1",
    [HTTP_2_0] = "HTTP/2.0",
  };
  return protocols[p];
}

static const char *status(status_t s) {
  static const char *status[] = {
    [STATUS_NONE] = NULL,
    [OK_200] = "200 OK",
    [NOT_FOUND_404] = "404 Not Found",
  };
  return status[s];
}

// static const char *type(type_t t) {
//   static const char *types[] = {
//     [TYPE_NONE] = NULL,
//     [TEXT_HTML] = "Content-Type: text/html",
//     [TEXT_CSS] = "Content-Type: text/css",
//     [APPLICATION_JAVASCRIPT] = "Content-Type: application/javascript",
//     [APPLICATION_JSON] = "Content-Type: application/json",
//   };
//   return types[t];
// };

// static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
//   if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
//       strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
//     return 0;
//   }
//   return -1;
// }

static view_t *http_handle_static(view_t *view) {
  head_buffer->len = chsnprintf(head_buffer->data, MAX_BUFFER_SIZE,
    "%s %s\r\n"
    "Content-Type: %s\r\n"
    "Connection: close\r\n"
    "\r\n"
    ,protocol(HTTP_1_1)
    ,status(OK_200)
    ,view->file->type
  );
  response->head = head_buffer->data;
  response->head_len = &(head_buffer->len);
  response->body = (const char *)view->file->data;
  response->body_len = (const int *)view->file->len;
  view->response = response;
  return view;
}


static view_t *http_handle_status(view_t *view) {
  head_buffer->len = chsnprintf(head_buffer->data, MAX_BUFFER_SIZE,
    "%s %s\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "\r\n"
    ,protocol(HTTP_1_1)
    ,status(OK_200)
  );
  response->head = head_buffer->data;
  response->head_len = &(head_buffer->len);

  body_buffer->len= chsnprintf(body_buffer->data, MAX_BUFFER_SIZE
    ,(const char *)view->file->data
    ,protocol(HTTP_1_1)
    ,status(OK_200)
  );
  response->body = body_buffer->data;
  response->body_len = &(body_buffer->len);

  view->response = response;
  return view;
}

// static view_t * http_handle_profile_get(view_t *view) {
//   return view;
// }
//
// static view_t * http_handle_profile_post(view_t *view) {
//   return view;
// }


extern file_t file_index_html;
extern file_t file_bootstrap_min_css;
extern file_t file_bootstrap_min_js;
extern file_t file_Chart_bundle_min_js;
extern file_t file_custom_js;
extern file_t file_jquery_3_4_1_min_js;
extern file_t file_popper_min_js;
extern file_t file_status_json;
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
  // {
  //   .path = "/profile",
  //   .head = &ui_js_http,
  //   .body = NULL,
  //   .get_handler = http_handle_profile_get,
  //   .post_handler = http_handle_profile_post,
  // },
  {
    .path = "/status",
    .file = &file_status_json,
    .get_handler = http_handle_status,
    .post_handler = NULL,
  },
};

void request_parse(const char *raw) {
  // chprintf((BaseSequentialStream*)&SD3,
  //   "\r\nREQUEST:\r\n"
  //   "%s\r\n",
  //   raw
  // );
  request_t *r = request;
  // header_t *h = headers_create();

  size_t method_len = strcspn(raw, " ");
  if (memcmp(raw, method(GET), strlen(method(GET))) == 0) {
    r->method = GET;
  } else if (memcmp(raw, method(POST), strlen(method(POST))) == 0) {
    r->method = POST;
  } else {
    r->method = METHOD_NONE;
  }
  raw += method_len + 1;

  size_t url_len = strcspn(raw, " ");
  memcpy(r->url, raw, url_len);
  r->url[url_len] = '\0';
  raw += url_len + 1;

  size_t protocol_len = strcspn(raw, "\r\n");
  if (memcmp(raw, protocol(HTTP_1_0), strlen(protocol(HTTP_1_0))) == 0) {
    r->protocol = HTTP_1_0;
  } else if (memcmp(raw, protocol(HTTP_1_1), strlen(protocol(HTTP_1_1))) == 0) {
    r->protocol = HTTP_1_1;
  } else if (memcmp(raw, protocol(HTTP_2_0), strlen(protocol(HTTP_2_0))) == 0) {
    r->protocol = HTTP_2_0;
  } else {
    r->protocol = PROTOCOL_NONE;
  }

  raw += protocol_len + 2;

  while (raw[0]!='\r' || raw[1]!='\n') {
    static unsigned int i = 0;

    size_t name_len = strcspn(raw, ":");
    if (i < MAX_HEADER_COUNT) {
      memcpy(headers[i].name, raw, name_len);
      headers[i].name[name_len] = '\0';
    }
    raw += name_len + 1;
    while (*raw == ' ') {
      raw++;
    }

    size_t value_len = strcspn(raw, "\r\n");
    if (i < MAX_HEADER_COUNT) {
      memcpy(headers[i].value, raw, value_len);
      headers[i].value[value_len] = '\0';
    }
    raw += value_len + 2;

    if (i < MAX_HEADER_COUNT) {
      headers[i].next = NULL;
      if (i > 0) {
        headers[i-1].next = &headers[i];
      }
    }

    i++;
  }

  r->headers = headers;
  // while (h) {
  //   printf(
  //     "%s: %s\r\n"
  //     ,h->name
  //     ,h->value
  //   );
  //   h = h->next;
  // }

  raw += 2;

  size_t body_len = strlen(raw);
  memcpy(r->body, raw, body_len);
  r->body[body_len] = '\0';

  const char *header = request_header_get("User-Agent");
  // chprintf((BaseSequentialStream*)&SD3,
  //   "method: %s\r\n"
  //   "url: %s\r\n"
  //   "protocol: %s\r\n"
  //   "User-Agent: %s\r\n"
  //   "body: %s\r\n"
  //   ,method(r->method)
  //   ,r->url
  //   ,protocol(r->protocol)
  //   ,header
  //   ,r->body
  // );

  size_t js_len = strcspn(r->body, "}");
  memcpy(js, r->body, js_len + 1);
  // js_len = strcspn(r->body, "}");
  // char *js = NULL;
  // memcpy(js, r->body, js_len);
  // printf("%s\n", js);

}

static void http_server_serve(struct netconn *conn) {
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void **)&buf, &buflen);
    request_parse((const char *)buf);

    for (unsigned int i = 0; i < ARRAY_SIZE(views); i++) {
      if (!strcmp(request->url, views[i].path)) {
        view_t *view = NULL;
        if ((request->method == GET) && views[i].get_handler) {
          view = views[i].get_handler(&views[i]);
        }
        if ((request->method == POST) && views[i].post_handler) {
          view = views[i].post_handler(&views[i]);
        }
        if (view){
          netconn_write(conn,
                        view->response->head,
                        *(view->response->head_len),
                        NETCONN_NOCOPY);
          netconn_write(conn,
                        view->response->body,
                        *(view->response->body_len),
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

/**
 * Stack area for the http thread.
 */
THD_WORKING_AREA(wa_http_server, WEB_THREAD_STACK_SIZE);

/**
 * HTTP server thread.
 */
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

#endif /* LWIP_NETCONN */

/** @} */
