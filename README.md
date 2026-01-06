# esp32-wifi-module


Este repositório contém o WifiManager, uma classe desenvolvida para abstrair e gerenciar conexões Wi-Fi no ESP32 utilizando o framework ESP-IDF.

O módulo implementa um gerenciamento robusto do ciclo de vida da conexão Wi-Fi, garantindo estabilidade no uso da interface de rede e tratamento adequado de falhas e reconexões por meio de uma máquina de estados interna.

A conexão é conduzida de forma assíncrona, baseada em eventos do ESP-IDF, onde os handlers assumem o controle do fluxo de conexão, reconexão e recuperação em background.

> Foi produzido um exemplo prático e simples de utilização do WifiManager, demonstrando o ciclo completo de uso e testando todos os métodos. [Clique aqui](https://github.com/ericdenser/esp32-wifi-module/blob/main/WifiManager/main/main.cpp) para visualizar.


## Estrutura do Módulo

O componente é composto por dois arquivos principais:

- [ ] [WifiManager.h](https://github.com/ericdenser/esp32-wifi-module/blob/main/WifiManager/main/src/WifiManager.h): Definição da classe, enumerações de estado e assinatura dos métodos estáticos.
- [ ] [WifiManager.cpp](https://github.com/ericdenser/esp32-wifi-module/blob/main/WifiManager/main/src/WifiManager.cpp): Implementação da lógica de eventos, máquina de estados, controle do driver Wi-Fi.

## Dependências e Como Utilizar

Para utilizar esta classe, o ambiente de desenvolvimento deve atender aos seguintes requisitos:

- [ ] Hardware da família **ESP32**.
- [ ] **ESP-IDF v5.x**.

### **Implementando Módulo**
1. Copie os arquivos [WifiManager.h](https://github.com/ericdenser/esp32-wifi-module/blob/main/WifiManager/main/src/WifiManager.h) e [WifiManager.cpp](https://github.com/ericdenser/esp32-wifi-module/blob/main/WifiManager/main/src/WifiManager.cpp) diretamente na pasta em que o arquivo `main` esta ou em uma subpasta (por exemplo, /src).
2. Atualize o `CMakeLists.txt` para incluir o arquivo `src/WifiManager.cpp` 
3. Inclua o cabeçalho do módulo 

    ```cpp
    #include "WifiManager.h"
    ```
4. Inicialize o Wi-Fi chamando o método de inicialização do módulo `WifiManager::init()`
   ```cpp

    // Inicialização utilizando parâmetros definidos via Kconfig
    WifiManager::init();
    
    // Inicialização utilizando credenciais definidas em código
    WifiConfig cfg;
    cfg.ssid = "SSID";
    cfg.password = "PASSWORD";
    cfg.max_retries = 5;
    
    WifiManager::init(cfg);
   ```
    
5. (Opcional) Caso a aplicação dependa obrigatoriamente de conectividade para continuar, utilize o método síncrono em seguida:
    ```cpp
    if (!WifiManager::waitForConnection()) {
        WifiManager::recover();
    }
    ```

6. Acompanhe e gerencie os estados da conexão e eventos conforme a documentação completa abaixo:

## API Reference

Método | Descrição 
--------|-----
`void init()` | Inicializa a infraestrutura de rede (Event Loop, LwIP e driver Wi-Fi) e delega o fluxo de conexão aos handlers em background. As credenciais podem ser definidas via código ou via Kconfig (valores passados por código têm prioridade sobre os definidos no menuconfig.
`void deinit()` | Para o Wi-Fi, destrói a interface Netif e o Driver, limpando a memória.
`bool waitForConnection()` | Bloqueia a execução até obter IP ou estourar o timeout. Aceita callback opcional para watchdog ou outras tarefas de sua escolha.
`void stop()` | Para o driver Wi-Fi (define status como IDLE e previne autoreconnect no handler)
`void start()` | Inicia o driver Wi-Fi a partir do estado IDLE.
`void reconnect()` | Força um ciclo de Stop/Start para resetar a lógica de conexão.
`bool isConnected()` | Retorna true se o IP foi obtido com sucesso.
`bool hasFailed()` | Retorna true se o número máximo de tentativas foi excedido.
`void recover()` | Executa a estratégia de recuperação após falha definitiva de conexão. O comportamento depende da configuração WIFI_AUTO_RESET.
`WifiStatus getWifiStatus()` | Retorna o estado atual (IDLE, CONNECTING, CONNECTED, FAILED, RECONNECTING).
`FailReason getFailReason()` | Retorna a ultima falha (NONE, AUTH, NO_AP, TIMEOUT, MAX_RETRIES...).
`int getRssi()` | Retorna o valor de RSSI da rede conectada.
`std::string getSSID()` | Retorna o SSID da rede conectada atualmente. |
`std::string getIp()` | Retorna o endereço IP. |
`std::string getMacAddress()` | Retorna o MAC Address do dispositivo formatado. |


## Máquina de Estados

O módulo opera com uma máquina de estados interna para garantir a estabilidade e evitar comportamentos indefinidos (como loops de reconexão indesejados).

* **IDLE:** Estado inicial ou definido após chamar `stop()`. Neste estado, eventos de desconexão são ignorados no handler intencionalmente e o *auto-reconnect* é desativado.
* **CONNECTING:** O driver foi iniciado e está tentando se associar ao AP ou realizar o handshake.
* **CONNECTED:** Endereço IP obtido com sucesso.
* **RECONNECTING:** Estado transisório utilizado durante o ciclo manual de `reconnect()` para evitar condições de corrida (Race Conditions) entre o comando de parada e o evento de desconexão.
* **FAILED:** O número máximo de tentativas (`max_retries`) foi excedido. O módulo para de tentar conectar e aguarda uma intervenção manual (como o `recover()`).



## Estratégia de Recuperação 

O módulo possui um sistema inteligente de recuperação (`WifiManager::recover()`) configurável via **Kconfig**. Quando o número máximo de tentativas de conexão (`max_retries`) é excedido, o sistema pode adotar uma das seguintes estratégias:

1.  **System Reset:** Reinicia o ESP32 imediatamente.
2.  **Swap SSID:**
    * Tenta conectar em uma **Rede de Backup** (SSID/Senha secundários definidos no Kconfig).
    * Se conectar com sucesso, o sistema opera na rede de backup.
    * Se a rede de backup *também* falhar (após `max_retries`), o sistema reinicia o ESP32.
3.  **Keep Idle:** Apenas marca o estado como `FAILED` e aguarda intervenção manual do código do usuário.

## Configuração (Kconfig)

Para utilizar configurações padrão sem recompilar o código fonte e personalizar o WifiManager, execute `idf.py menuconfig` e navegue até **Wifi Manager Configuration**:

* **Main SSID / Password:** Credenciais da rede principal.
* **Max Retries:** Número de tentativas antes de declarar falha.
* **Recovery Strategy on Failure:**
    * `System Reset`: Reinicia o chip (Padrão).
    * `Swap SSID`: Habilita a rede de backup.
    * `Keep Idle`: Não faz nada automático.
* **Backup SSID / Password:** Credenciais da rede secundária (visíveis apenas se *Swap SSID* for selecionado).

> **Nota:** Não esqueça de recompilar o projeto (`idf.py build`) após alterar configurações do menuconfig.



## Informações Importantes

### Inicialização do NVS
O driver Wi-Fi do ESP32 exige que a memória NVS esteja inicializada para rodar. Certifique-se de chamar o código abaixo no início do seu `app_main`, **antes** de chamar `WifiManager::init()`:

```cpp
// ------- Initialize NVS (Non-Volatile Storage) --------

    nvs_handle_t my_nvs_handle;
    
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
```

### Comportamento Bloqueante e Watchdog
Ao utilizar a função `waitForConnection()`, a tarefa atual permanece em loop até que a conexão Wi-Fi seja estabelecida ou o tempo limite seja atingido. Caso precise executar alguma tarefa durante o loop, basta passar no parâmetro como o exemplo a seguir:

O método reset e sua classe estão disponíveis para consulta no repositório [WatchdogManager](https://github.com/ericdenser/esp32-watchdog-manager)
```cpp

WifiManager::init();

/*
Passamos o reset do watchdog como parâmetro e caso waitForConnection retorne false, significa que todas tentativas de conexão falharam,
e portanto usamos o método recover para prosseguir com o fluxo */

if (!WifiManager::waitForConnection(WatchdogManager::reset)) {  
    WifiManager::recover();
}
```

## Limitações Conhecidas

* **Configuração de IP:** Não há suporte implementado na estrutura WifiConfig para configuração de IP Estático
* **Segurança:** O módulo utiliza a configuração padrão do driver para autenticação, suportando WPA/WPA2. Redes que exigem certificado/usuário ou redes abertas podem exigir ajustes manuais na estrutura wifi_config_t dentro do método init(). Consulte a documentação oficial da espressif para implementação.

## Fontes e Documentação Consultada
Abaixo estão alguns materiais utilizados durante a implementação do WifiManager que podem ajudar na sua utilização ou ampliação.

* **[Wi-Fi Driver API Reference](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html)** - Documentação oficial da API de Wi-Fi do ESP-IDF.

* **[ESP-IDF Wi-Fi Station Example](https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c)** - Exemplo oficial da Espressif utilizado como base para a implementação e fluxo do módulo.

* **[ESP-IDF FreeRTOS Concurrency and Critical Sections](https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32c3/api-guides/freertos-smp.html)** - Utilizado para implementar as seções críticas (portENTER_CRITICAL).

* **[ESP32 Forum. FreeRTOS Critical Sections and Spinlocks](https://esp32.com/viewtopic.php?t=12621)** - Discussões técnicas da comunidade sobre ESP32, utilizadas para compreender o comportamento de seções críticas.

* **[Non-Volatile Storage (NVS) API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html)** - Documentação oficial da API de armazenamento flash do ESP-IDF.

## Licença

Este projeto é licenciado sob os termos da licença **MIT**.  

