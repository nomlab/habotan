/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "app_wifi.h"

#include "esp_http_client.h"


/// Perform URL encode on `s` copying the result into `buf`.
///
/// Return value is the strlen of encoded string.
/// If the return value is larger than `buf_size` -1,
/// that indicates `buf` doesn't enclose whole encoded string.
///
/// `buf` can be NULL. In this case, urlencode estimates
/// strlen of encoded string without updating `buf`.
/// For example:
///
///   size_t buf_size = urlencode(s, NULL, 0) + 1;
///   char *buf = malloc(buf_size);
///   urlencode(s, buf, buf_size);
///
/// SEE ALSO: https://url.spec.whatwg.org/#urlencoded-serializing
///
size_t urlencode(const char *s, char *buf, size_t buf_size)
{
  const char *hex = "0123456789ABCDEF";
  size_t size = 0;

  while (*s) {
    if (('a' <= *s && *s <= 'z') ||
        ('A' <= *s && *s <= 'Z') ||
        ('0' <= *s && *s <= '9') ||
        *s == '*' || *s == '.'   ||
        *s == '-' || *s == '_') {

      if (buf && size + 1 < buf_size) { *buf++ = *s; }
      size++;

    } else if (*s == ' ') {

      if (buf && size + 1 < buf_size) { *buf++ = '+'; }
      size++;

    } else {

      if (buf && size + 3 < buf_size) {
        *buf++ = '%';
        *buf++ = hex[(*s >> 4) & 0x0f];
        *buf++ = hex[(*s >> 0) & 0x0f];
      }
      size += 3;
    }
    s++;
  }

  if (buf && buf_size > 0) { *buf = '\0'; }
  return size;
}

size_t strlen_if_urlencode(const char *s)
{
  return urlencode(s, NULL, 0);
}

#define MAX_HTTP_RECV_BUFFER 512
static const char *TAG = "HTTP_CLIENT";

/* Root cert for slack.com, taken from slack_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect slack.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const char slack_com_root_cert_pem_start[] asm("_binary_slack_com_root_cert_pem_start");
extern const char slack_com_root_cert_pem_end[]   asm("_binary_slack_com_root_cert_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

#define SLACK_API_ENDPOINT CONFIG_SLACK_API_ENDPOINT
#define SLACK_API_TOKEN    CONFIG_SLACK_API_TOKEN
#define SLACK_CHANNEL_NAME CONFIG_SLACK_CHANNEL_NAME
#define SLACK_MESSAGE      CONFIG_SLACK_MESSAGE

static void post_to_slack()
{
    esp_http_client_config_t config = {
        .url = SLACK_API_ENDPOINT,
        .event_handler = _http_event_handler,
        .cert_pem = slack_com_root_cert_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    const char *post_data =
      "token="  SLACK_API_TOKEN
      "&channel="  SLACK_CHANNEL_NAME
      "&text=" SLACK_MESSAGE
      "&as_user=true";

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTPS POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void http_test_task(void *pvParameters)
{
    app_wifi_wait_connected();
    ESP_LOGI(TAG, "Connected to AP, begin posting to slack");
    post_to_slack();
    ESP_LOGI(TAG, "Finish posting to slack");
    vTaskDelete(NULL);
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    app_wifi_initialise();

    xTaskCreate(&http_test_task, "http_test_task", 8192, NULL, 5, NULL);
}
