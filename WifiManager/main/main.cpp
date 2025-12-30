#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "src/WifiManager.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"



nvs_handle_t my_nvs_handle;
static const char *TAG = "MAIN";

void checkStatus() {
    WifiManager::WifiStatus status = WifiManager::getWifiStatus();

        switch (status) {
            case WifiManager::WifiStatus::WIFI_STATE_IDLE:
                ESP_LOGW(TAG, "Status - WiFi idle");
                break;

            case WifiManager::WifiStatus::WIFI_STATE_CONNECTING:
                ESP_LOGW(TAG, "Status - WiFi connecting...");
                break;

            case WifiManager::WifiStatus::WIFI_STATE_CONNECTED:
                ESP_LOGW(TAG, "Status - WiFi connected");
                break;

            case WifiManager::WifiStatus::WIFI_STATE_RECONNECTING:
                ESP_LOGW(TAG, "Status - WiFi reconnecting...");
                break;

            case WifiManager::WifiStatus::WIFI_STATE_FAILED:
                ESP_LOGW(TAG, "Status - WiFi failed!");
                break;
        }
}

extern "C" void app_main(void)
{

    ESP_LOGI(TAG, "===============================");
    ESP_LOGI(TAG, "       RUNNING SETUP");
    ESP_LOGI(TAG, "===============================");

    /* Wi-Fi driver requires the NVS memory to be initialized even if your not going to use it.
        Before calling the WifiManager, make sure to start nvs as shown below. */
    // ------- Initialize NVS (Non-Volatile Storage) --------
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        // If NVS partition is corrupted, erase and recreate it
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    err = nvs_open("WifiManager", NVS_READWRITE, &my_nvs_handle);
    ESP_ERROR_CHECK(err);


    // ------------- Initialize and Connect WiFi ----------------   

    WifiConfig cfg;
    cfg.ssid = "YOUR_SSID";
    cfg.password = "YOUR_PASSWORD";
    cfg.max_retries = 15;

    ESP_LOGW(TAG, "======= TESTING INIT FUNC =======");
    WifiManager::init(cfg);
    checkStatus();

    /*  Only proceeds if we got connection.
        'waitForConnection' has blocking loop that only returns true when connected in a defined period.
        You can pass your Watchdog Reset function on the parameter as a callback.
    */                              
    ESP_LOGW(TAG, "======= TESTING waitForConnection FUNC ======");        
    if (!WifiManager::waitForConnection()) {
        WifiManager::recover();
    }
    checkStatus();


    //If all steps were successful, we should be connected here. Lets run the reconnect func to test it
    ESP_LOGW(TAG, "======= TESTING THE RECONNECT FUNC =======");
    WifiManager::reconnect();
    checkStatus();

    // Intentional delay to allow async Wi-Fi operations to complete and state logs to stabilize.
    vTaskDelay(pdMS_TO_TICKS(10000));


    // If the reconnect worked, we should be connected again. Lets now test the stop and start func
    ESP_LOGW(TAG, "======= TESTING THE STOP FUNC =======");
    WifiManager::stop();
    checkStatus();

    // Intentional delay to allow async Wi-Fi operations to complete and state logs to stabilize.
    vTaskDelay(pdMS_TO_TICKS(10000));

    
    ESP_LOGW(TAG, "======= TESTING THE START FUNC =======");
    WifiManager::start();
    checkStatus();
    
    // Intentional delay to allow async Wi-Fi operations to complete and state logs to stabilize.
    vTaskDelay(pdMS_TO_TICKS(10000));
    

    // Lets now deinit our WifiManager and init it again
    ESP_LOGW(TAG, "======= TESTING THE DEINIT FUNC =======");
    WifiManager::deinit();
    checkStatus();

    // Intentional delay to allow async Wi-Fi operations to complete and state logs to stabilize.
    vTaskDelay(pdMS_TO_TICKS(10000));
    

    ESP_LOGW(TAG, "======= TESTING THE INIT FUNC AGAIN =======");
    WifiManager::init(cfg);
    checkStatus();
     
    if (!WifiManager::waitForConnection()) {
        WifiManager::recover();
    }

    
    
    while(1) {

        // Monitor WiFi Health
        if(WifiManager::hasFailed()) {
            ESP_LOGE(TAG, "Critical. Not able to connect after all attempts.");
            WifiManager::recover();
        }

        // Periodic feedback log
        static int64_t last_log = 0;
        if (esp_timer_get_time() - last_log > 5000000) {
            int8_t rssi = WifiManager::getRssi();

            checkStatus();

            last_log = esp_timer_get_time();
            
            ESP_LOGI(TAG, "===============================");
            ESP_LOGI(TAG, "   System running ");
            ESP_LOGI(TAG, "   RSSI : %d", rssi);
            ESP_LOGI(TAG, "===============================");
        } 
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
