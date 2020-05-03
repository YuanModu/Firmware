// extern const unsigned char bootstrap_min_css[];
// extern const unsigned int bootstrap_min_css_len;
// extern const unsigned char bootstrap_min_js[];
// extern const unsigned int bootstrap_min_js_len;
// extern const unsigned char Chart_bundle_min_js[];
// extern const unsigned int Chart_bundle_min_js_len;
// extern const unsigned char css_http[];
// extern const unsigned int css_http_len;
// extern const unsigned char html_http[];
// extern const unsigned int html_http_len;
// extern const unsigned char index_html[];
// extern const unsigned int index_html_len;
// extern const unsigned char jquery_3_4_1_min_js[];
// extern const unsigned int jquery_3_4_1_min_js_len;
// extern const unsigned char js_http[];
// extern const unsigned int js_http_len;
// extern const unsigned char json_http[];
// extern const unsigned int json_http_len;
// extern const unsigned char popper_min_js[];
// extern const unsigned int popper_min_js_len;
// extern const unsigned char custom_js[];
// extern const unsigned int custom_js_len;


typedef struct file {
  const unsigned char *data;
  const unsigned int *len;
  const unsigned char *type;
} file_t;


//
// file_t ui_html_http = {
//   .len = &html_http_len,
//   .data = html_http,
// };
//
// file_t ui_css_http = {
//   .len = &css_http_len,
//   .data = css_http,
// };
//
// file_t ui_js_http = {
//   .len = &js_http_len,
//   .data = js_http,
// };
//
// file_t ui_json_http = {
//   .len = &json_http_len,
//   .data = json_http,
// };
//
// file_t file_index_html = {
//   .len = &index_html_len,
//   .data = index_html,
//   .type = (const unsigned *)"text/html"
// };
//
// file_t ui_bootstrap_min_css = {
//   .len = &bootstrap_min_css_len,
//   .data = bootstrap_min_css,
// };
//
// file_t ui_bootstrap_min_js = {
//   .len = &bootstrap_min_js_len,
//   .data = bootstrap_min_js,
// };
//
// file_t ui_Chart_bundle_min_js = {
//   .len = &Chart_bundle_min_js_len,
//   .data = Chart_bundle_min_js,
// };
//
// file_t ui_custom_js = {
//   .len = &custom_js_len,
//   .data = custom_js,
// };
//
// file_t ui_jquery_3_4_1_min_js = {
//   .len = &jquery_3_4_1_min_js_len,
//   .data = jquery_3_4_1_min_js,
// };
//
// file_t ui_popper_min_js = {
//   .len = &popper_min_js_len,
//   .data = popper_min_js,
// };
