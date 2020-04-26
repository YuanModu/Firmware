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

#include <string.h>

#include "ch.h"

#include "hal.h" /* chprintf */
#include "chprintf.h" /* chprintf */

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "web.h"

#if LWIP_NETCONN

extern const unsigned char bootstrap_min_css[];
extern const unsigned int bootstrap_min_css_len;
extern const unsigned char bootstrap_min_js[];
extern const unsigned int bootstrap_min_js_len;
extern const unsigned char Chart_bundle_min_js[];
extern const unsigned int Chart_bundle_min_js_len;
extern const unsigned char css_http[];
extern const unsigned int css_http_len;
extern const unsigned char html_http[];
extern const unsigned int html_http_len;
extern const unsigned char index_html[];
extern const unsigned int index_html_len;
extern const unsigned char jquery_3_4_1_slim_min_js[];
extern const unsigned int jquery_3_4_1_slim_min_js_len;
extern const unsigned char js_http[];
extern const unsigned int js_http_len;
extern const unsigned char json_http[];
extern const unsigned int json_http_len;
extern const unsigned char popper_min_js[];
extern const unsigned int popper_min_js_len;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

//
// static const char http_resp_ok[] =
//   "HTTP/1.1 200\r\n"
//   "\r\n";
//
// static const char http_resp_serverfault[] =
//   "HTTP/1.1 500\r\n"
//   "\r\n";
//
// static const char http_resp_notimp[] =
//   "HTTP/1.1 501\r\n"
//   "\r\n";
//
// static const char http_resp_redir[] =
//   "HTTP/1.1 301\r\n"
//   "Location: /\r\n"
//   "\r\n";
//
// static const char http_resp_badreq[] =
//   "HTTP/1.1 400˝\r\n"
//   "\r\n";

struct file {
  const unsigned int *len;
  const unsigned char *data;
};

static struct file ui_html_http = {
  .len = &html_http_len,
  .data = html_http,
};

static struct file ui_css_http = {
  .len = &css_http_len,
  .data = css_http,
};

static struct file ui_js_http = {
  .len = &js_http_len,
  .data = js_http,
};

static struct file ui_json_http = {
  .len = &json_http_len,
  .data = json_http,
};

static struct file ui_index_html = {
  .len = &index_html_len,
  .data = index_html,
};

static struct file ui_bootstrap_min_css = {
  .len = &bootstrap_min_css_len,
  .data = bootstrap_min_css,
};

static struct file ui_bootstrap_min_js = {
  .len = &bootstrap_min_js_len,
  .data = bootstrap_min_js,
};

static struct file ui_Chart_bundle_min_js = {
  .len = &Chart_bundle_min_js_len,
  .data = Chart_bundle_min_js,
};

static struct file ui_jquery_3_4_1_slim_min_js = {
  .len = &jquery_3_4_1_slim_min_js_len,
  .data = jquery_3_4_1_slim_min_js,
};

static struct file ui_popper_min_js = {
  .len = &popper_min_js_len,
  .data = popper_min_js,
};

struct http_response {
	char path[32];
  struct file *head;
	struct file *body;
	struct http_response * (*get_handler)(struct http_response *);
	struct http_response * (*post_handler)(struct http_response *);
};

static struct http_response * http_handle_static (struct http_response *response) {
  return response;
}


static unsigned char sbuf[128];
static unsigned int slen;
static struct file body = {
  .len = &slen,
  .data = sbuf,
};

static struct http_response * http_handle_status(struct http_response *response) {
  slen = chsnprintf((char *)sbuf, ARRAY_SIZE(sbuf),
            "{"
            "\"satatus\": \"ok\""
            "}"
          );
  response->body = &body;
  return response;
}

static struct http_response http_responses[] = {
  {
    .path = "/",
    .head = &ui_html_http,
    .body = &ui_index_html,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/bootstrap.min.css",
    .head = &ui_css_http,
    .body = &ui_bootstrap_min_css,
    .get_handler = http_handle_static,
    .post_handler = NULL
  },
  {
    .path = "/bootstrap.min.js",
    .head = &ui_js_http,
    .body = &ui_bootstrap_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/Chart.bundle.min.js",
    .head = &ui_js_http,
    .body = &ui_Chart_bundle_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/jquery-3.4.1.slim.min.js",
    .head = &ui_js_http,
    .body = &ui_jquery_3_4_1_slim_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL,
  },
  {
    .path = "/popper.min.js",
    .head = &ui_js_http,
    .body = &ui_popper_min_js,
    .get_handler = http_handle_static,
    .post_handler = NULL
  },
  {
    .path = "/status",
    .head = &ui_json_http,
    .body = NULL,
    .get_handler = http_handle_status,
    .post_handler = NULL,
  },
};

static void http_server_serve(struct netconn *conn) {
  struct netbuf *inbuf;
  char *buf, *method, *url;
  u16_t buflen;
  err_t err;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void **)&buf, &buflen);

    buf[buflen-1] = 0;

    method = strtok(buf, " \r\n");
    url = strtok(NULL, " \r\n");

    for (unsigned int i = 0; i < ARRAY_SIZE(http_responses); i++) {
      if (!strcmp(url, http_responses[i].path)) {
        struct http_response *response = NULL;
        if (!strcmp(method, "GET") && http_responses[i].get_handler) {
          response = http_responses[i].get_handler(&http_responses[i]);
        }
        if (!strcmp(method, "POST") && http_responses[i].post_handler) {
          response = http_responses[i].post_handler(&http_responses[i]);
        }
        if (response){
          netconn_write(conn,
                        response->head->data,
                        *(response->head->len),
                        NETCONN_NOCOPY);
          netconn_write(conn,
                        response->body->data,
                        *(response->body->len),
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
