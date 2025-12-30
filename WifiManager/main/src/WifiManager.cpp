#include <string.h>
#include "WifiManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "sdkconfig.h"


#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID ""
#endif

#ifndef CONFIG_WIFI_PASSWORD
#define CONFIG_WIFI_PASSWORD ""
#endif

#ifndef CONFIG_WIFI_MAX_RETRIES
#define CONFIG_WIFI_MAX_RETRIES 5
#endif


#define WIFI_CONNECTED_BIT BIT0 // Conexão efetuada e IP obtido
#define WIFI_FAIL_BIT BIT1 //  Falha definitiva após MAX_WIFI_RETRIES
#define MAX_WAIT_MS 100000 // 100 segundos de tolerancia
#define TICK_WAIT_MS 500 

static char s_final_ssid[33];
static char s_final_pass[64];
static int  s_final_retries = 10;
static bool s_reset_enable = true;

static portMUX_TYPE s_wifi_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_netif_t *s_wifi_netif = NULL; 
static EventGroupHandle_t s_wifi_event_group; // Handle para o Event Group 
static const char *TAG = "WifiManager"; // Para logs de debug
static int s_retry_count = 0; // Contador de tentativas de reconexão Wi-Fi
static bool s_is_initialized = false; // Flag para evitar re-init

// ... outras variáveis estáticas ...
static esp_event_handler_instance_t s_instance_any_id;
static esp_event_handler_instance_t s_instance_got_ip;

static volatile WifiManager::WifiStatus  s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_IDLE;
static volatile WifiManager::FailReason s_fail_reason = WifiManager::FailReason::WIFI_FAIL_NONE;

static const char* failReasonToString(WifiManager::FailReason reason) {
    switch(reason) {
        case WifiManager::FailReason::WIFI_FAIL_NONE: return "NONE";
        case WifiManager::FailReason::WIFI_FAIL_AUTH: return "AUTH FAIL";
        case WifiManager::FailReason::WIFI_FAIL_NO_AP: return "NO AP FOUND";
        case WifiManager::FailReason::WIFI_FAIL_MAX_RETRIES: return "MAX RETRIES";
        case WifiManager::FailReason::WIFI_FAIL_TIMEOUT: return "TIMEOUT";
        case WifiManager::FailReason::WIFI_FAIL_UNKNOWN: return "UNKNOWN";
        default: return "INVALID REASON";
    }
}

static void set_failure(WifiManager::FailReason reason) {

    portENTER_CRITICAL(&s_wifi_mux);
    if (s_wifi_status != WifiManager::WifiStatus::WIFI_STATE_FAILED) {
        s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_FAILED;
        s_fail_reason = reason;
    }

    portEXIT_CRITICAL(&s_wifi_mux);

    ESP_LOGE(TAG, "Wifi failed, reason: %s", failReasonToString(reason));

    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);

}

static void set_success() {
    portENTER_CRITICAL(&s_wifi_mux);

    s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_CONNECTED;
    s_fail_reason = WifiManager::FailReason::WIFI_FAIL_NONE;
    s_retry_count = 0;
    
    portEXIT_CRITICAL(&s_wifi_mux);

    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
}

// System events handler 
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    
    if (event_base == WIFI_EVENT) {
        switch(event_id) {
            // Caso o evento seja de início da conexão Wi-Fi 
            case WIFI_EVENT_STA_START:

                portENTER_CRITICAL(&s_wifi_mux);
                s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_CONNECTING;
                portEXIT_CRITICAL(&s_wifi_mux);

                ESP_LOGI(TAG, "HANDLER >> WiFi STA started, attempting connection...");
                esp_wifi_connect();

                break;

            // Caso o evento seja a desconexão ===============
            case WIFI_EVENT_STA_DISCONNECTED: {

                // Check if the disconnection was intentionally by the user
                WifiManager::WifiStatus currentStatus;
                portENTER_CRITICAL(&s_wifi_mux);
                currentStatus = s_wifi_status;
                portEXIT_CRITICAL(&s_wifi_mux);

                if (currentStatus == WifiManager::WifiStatus::WIFI_STATE_IDLE || 
                    currentStatus == WifiManager::WifiStatus::WIFI_STATE_RECONNECTING) {
                    ESP_LOGI(TAG, "HANDLER >> Intentionally disconnection. Cancelling automatic reconnection.");
                    break;
                }

                // Obtém o motivo da desconexão para diagnóstico
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "HANDLER >> Disconnected. Reason: %d", event->reason);
                
                WifiManager::FailReason tempReason; 

                // Falhas de Autenticação
                if (event->reason == WIFI_REASON_AUTH_EXPIRE || 
                    event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || 
                    event->reason == WIFI_REASON_AUTH_FAIL || 
                    event->reason == WIFI_REASON_ASSOC_FAIL || 
                    event->reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
                    event->reason == WIFI_REASON_NOT_AUTHED) {

                    tempReason = WifiManager::FailReason::WIFI_FAIL_AUTH;

                // Falhas de disponibilidade do AP
                } else if (event->reason == WIFI_REASON_NO_AP_FOUND || 
                           event->reason == WIFI_REASON_BEACON_TIMEOUT ||
                           event->reason == WIFI_REASON_ASSOC_EXPIRE ||   
                           event->reason == WIFI_REASON_NOT_ASSOCED) {    
    
                    tempReason = WifiManager::FailReason::WIFI_FAIL_NO_AP;

                } else if (event->reason == WIFI_REASON_CONNECTION_FAIL) {

                    tempReason = WifiManager::FailReason::WIFI_FAIL_CONNECTION;

                } else {
                    tempReason = WifiManager::FailReason::WIFI_FAIL_UNKNOWN;
                }
            

                // Tentativa de reconectar
                if (s_retry_count < s_final_retries) {

                    portENTER_CRITICAL(&s_wifi_mux);
                    s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_CONNECTING;
                    portEXIT_CRITICAL(&s_wifi_mux);

                    s_retry_count++;
                    ESP_LOGI(TAG, "HANDLER >> Trying WiFi connection (%d/%d)", s_retry_count, s_final_retries);
                    esp_wifi_connect();

                // Caso o número de tentativas atinja o máximo, marca a falha
                } else {
                    if (tempReason == WifiManager::FailReason::WIFI_FAIL_UNKNOWN) {
                        tempReason = WifiManager::FailReason::WIFI_FAIL_MAX_RETRIES;
                    }
                    set_failure(tempReason);
                }

                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            }   
            default:
                // Ignora outros eventos (Scan done, etc)
                break;
        }
    // ======= Caso o evento seja a obtenção de IP =========
    } else if (event_base == IP_EVENT) {
        switch(event_id) {
            case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data; // Obtém as informações do IP
            ESP_LOGI(TAG, "HANDLER >> Connected to ip: " IPSTR, IP2STR(&event->ip_info.ip));
        
            set_success();
            break;
            }
        }
    }
}

void WifiManager::init(WifiConfig config) {

    if (s_is_initialized) {
        ESP_LOGW(TAG, "WifiManager ja foi inicializado! Ignorando nova chamada.");
        return;
    }

    // Event Group que irá controlar o estado da conexão Wi-Fi
    s_wifi_event_group = xEventGroupCreate();

    s_is_initialized = true;

    /*------- Lógica das Credenciais ---------------
    Credenciais via código tem preferência, caso nao tenha utiliza do menuconfig*/
   
    //DEFINE SSID
    if (config.ssid != nullptr && strlen(config.ssid) > 0) {
        strlcpy(s_final_ssid, config.ssid, sizeof(s_final_ssid));
        ESP_LOGI(TAG, "Usando ssid definido via código: %s", s_final_ssid);
    } else {
        strlcpy(s_final_ssid, CONFIG_WIFI_SSID, sizeof(s_final_ssid));
        ESP_LOGI(TAG, "Usando ssid definido via menuconfig: %s", s_final_ssid);
    }

    //DEFINE PASS
    if (config.password != nullptr && strlen(config.password) > 0) {
        strlcpy(s_final_pass, config.password, sizeof(s_final_pass));
        ESP_LOGI(TAG, "Usando pass definido via código: %s", s_final_pass);
    } else {
        strlcpy(s_final_pass, CONFIG_WIFI_PASSWORD, sizeof(s_final_pass));
        ESP_LOGI(TAG, "Usando pass definido via menuconfig: %s", s_final_pass);
    }

    //DEFINE TENTATIVAS DE RECONEXÃO
    if (config.max_retries >= 0) {
        s_final_retries = config.max_retries;
    } else {
        s_final_retries = CONFIG_WIFI_MAX_RETRIES;
    }

    // Lógica do RESET controlada no menuconfig
    s_reset_enable = CONFIG_WIFI_AUTO_RESET;


    // --- Inicialização Padrão ESP-IDF ---
    ESP_ERROR_CHECK(esp_netif_init()); // Inicializa a camada TCP/IP

    esp_err_t err = esp_event_loop_create_default();// Cria o loop de eventos
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    } 

    if (s_wifi_netif == NULL) {
        s_wifi_netif = esp_netif_create_default_wifi_sta(); // Cria a interface da rede
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // Configuração default para Wifi
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Inicializa o driver Wi-Fi

     // Registra o handler de eventos para Wifi e IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                        &event_handler, NULL, &s_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                        &event_handler, NULL, &s_instance_got_ip));

    // Configuração de credenciais e segurança
    wifi_config_t wifi_config = {};
    strlcpy((char *) wifi_config.sta.ssid , s_final_ssid, sizeof(s_final_ssid)); 
    strlcpy((char *) wifi_config.sta.password , s_final_pass, sizeof(s_final_pass)); 

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // Aplica as configurações

    // Inicia o driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi Started!!!");
}

bool WifiManager::waitForConnection(std::function<void()> onProgressCallback) {
    if (!s_wifi_event_group) return false; // Verifica se o Event Group foi criado

    ESP_LOGI(TAG, "AGUARDANDO CONEXAO WIFI...");

    int64_t start_time = esp_timer_get_time();
   
    while (true) {

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, // Monitora este event group
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, // Resultados possíveis
            pdFALSE, // (xClearOnExit = pdFALSE) Os bits mantêm seu estado mesmo após finalizar
            pdFALSE, // (xWaitForAllBits = pdFALSE) Desbloqueia quando algum dos bits for setado
            pdMS_TO_TICKS(TICK_WAIT_MS) // Bloqueio temporário da task; repetido até CONNECTED ou FAIL
        );

        if (onProgressCallback) {
            onProgressCallback();
        }

        // Timeout Global 
        int64_t current_time = esp_timer_get_time();
        if ((current_time - start_time) >= (int64_t)MAX_WAIT_MS * 1000) {

            ESP_LOGE(TAG, "Global connection timeout (%d ms)", MAX_WAIT_MS);

            set_failure(WifiManager::FailReason::WIFI_FAIL_TIMEOUT);

            return false;
        }

        // Se a conexão foi bem-sucedida
        if (bits & WIFI_CONNECTED_BIT) {
            return true;
        }  
        // Se a conexão falhou
        if (bits & WIFI_FAIL_BIT) {
            ESP_LOGE(TAG, "WiFi failed to connect after %d attempts.", s_final_retries);
            return false;
        } 
        // Se nenhum bit foi setado (timeout), continua aguardando
    }
    
}

void WifiManager::reconnect() {
    if (!s_wifi_event_group) return;
    
    ESP_LOGW(TAG, "Iniciando reconexao WiFi...");

    portENTER_CRITICAL(&s_wifi_mux);
    s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_RECONNECTING;
    portEXIT_CRITICAL(&s_wifi_mux);
    
    // Limpa bits anteriores e reseta contador
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_count = 0;

    esp_wifi_stop(); 
    esp_wifi_start();
}

bool WifiManager::isConnected() {
    if (!s_wifi_event_group) return false;
    return xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT;
}

bool WifiManager::hasFailed() {
    if (!s_wifi_event_group) return false;
    return xEventGroupGetBits(s_wifi_event_group) & WIFI_FAIL_BIT;
}

void WifiManager::recover() {
    if (s_fail_reason != FailReason::WIFI_FAIL_NONE) {

        if (s_reset_enable) {
            ESP_LOGE(TAG, "Unrecoverable WiFi error. Restarting system...");
            vTaskDelay(pdMS_TO_TICKS(1000)); 
            esp_restart();
        } else {
            ESP_LOGE(TAG, "Unrecoverable WiFi error. AUTO_RESET is disabled, trying reconnection...");
            reconnect();
        }
       
    }
}

void WifiManager::deinit() {
    if (!s_is_initialized) return;

    ESP_LOGW(TAG, "Parando WifiManager...");

    // --------- DEREGISTRANDO OS HANDLERS ---------
    if (s_instance_any_id != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
        s_instance_any_id = NULL;
    }
    if (s_instance_got_ip != NULL) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
        s_instance_got_ip = NULL;
    }

    // Limpeza de Memória
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    // Destruingo A INTERFACE DE REDE ---
    if (s_wifi_netif != NULL) {
        esp_netif_destroy(s_wifi_netif);
        s_wifi_netif = NULL;
    }

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Resetando estados
    s_is_initialized = false;
    s_retry_count = 0;
    s_wifi_status = WifiStatus::WIFI_STATE_IDLE;
    s_fail_reason = FailReason::WIFI_FAIL_NONE;

    ESP_LOGI(TAG, "WifiManager closed and cleaned successfully.");
}

void WifiManager::start() {
    if (!s_is_initialized) {
        ESP_LOGW(TAG, "WifiManager not initialized!");
        return;
    }
    
    ESP_LOGI(TAG, "Starting Wi-Fi...");
    
    portENTER_CRITICAL(&s_wifi_mux);
    s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_CONNECTING;
    portEXIT_CRITICAL(&s_wifi_mux);

    esp_wifi_start(); 

}

void WifiManager::stop() {
    if (!s_is_initialized) return;

    ESP_LOGI(TAG, "Stopping Wi-Fi.");

    portENTER_CRITICAL(&s_wifi_mux);
    s_wifi_status = WifiManager::WifiStatus::WIFI_STATE_IDLE; 
    portEXIT_CRITICAL(&s_wifi_mux);

    if (s_wifi_event_group) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    esp_err_t err = esp_wifi_stop();
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi successfully stopped.");
    } else {
        ESP_LOGE(TAG, "Error at stopping Wi-Fi: %s", esp_err_to_name(err));
    }
}

int WifiManager::getRssi() {

    wifi_ap_record_t ap_info;

    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);

    if (err == ESP_OK) {
        return ap_info.rssi; 
    }

    if (err != ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGW(TAG, "Falha ao obter RSSI: %s", esp_err_to_name(err));
    }
    return -127;
}

WifiManager::FailReason WifiManager::getFailReason() {
    WifiManager::FailReason r;
    portENTER_CRITICAL(&s_wifi_mux);
    r = s_fail_reason;
    portEXIT_CRITICAL(&s_wifi_mux);
    return r;
}

WifiManager::WifiStatus WifiManager::getWifiStatus() {
    WifiManager::WifiStatus s;
    portENTER_CRITICAL(&s_wifi_mux);
    s = s_wifi_status;
    portEXIT_CRITICAL(&s_wifi_mux);
    return s;
}

