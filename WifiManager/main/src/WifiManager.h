#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <functional>
#include <stdint.h>


struct WifiConfig {
    const char* ssid = nullptr;      // Null = ssid do Kconfig
    const char* password = nullptr;  // Null = password do Kconfig
    int max_retries = -1;            // -1 = max_retries do Kconfig
};

class WifiManager {
    public:

        enum class WifiStatus {
            WIFI_STATE_IDLE,
            WIFI_STATE_CONNECTING,
            WIFI_STATE_CONNECTED,
            WIFI_STATE_FAILED,
            WIFI_STATE_RECONNECTING
        };

        enum class FailReason {
            WIFI_FAIL_NONE,
            WIFI_FAIL_TIMEOUT,
            WIFI_FAIL_AUTH,
            WIFI_FAIL_NO_AP,
            WIFI_FAIL_CONNECTION,
            WIFI_FAIL_MAX_RETRIES,
            WIFI_FAIL_UNKNOWN
        };



        static void init(WifiConfig config = WifiConfig());  

        static bool waitForConnection(std::function<void()> onProgressCallback = nullptr);

        static bool isConnected(); // retorna se WIFI_CONNECTED_BIT está setado

        static bool hasFailed();

        static void recover();
        
        // Limpa estado e tenta reconectar
        static void reconnect();

        static void deinit();

        static void start();

        static void stop();

        static int getRssi();

        static FailReason getFailReason();

        static WifiStatus getWifiStatus();

};
#endif
