# esp32-wifi-module

## About

Este repositório contém o WifiManager, uma classe desenvolvida para abstrair e gerenciar conexões Wi-Fi em modo Station (STA) no ESP32 utilizando o framework ESP-IDF.

O módulo implementa um gerenciamento robusto do ciclo de vida da conexão, garantindo estabilidade no uso da interface de rede e tratamento adequado de erros de reconexão.
Ele tambem oferece suporte para inicializações síncronas através da função `waitForConnection()`, permitindo bloquear a execução da tarefa até que a conexão seja efetivamente estabelecida, ideal para cenários onde o Wi-Fi é pré-requisito obrigatório para o boot.

## Estrutura do Módulo

O componente é composto por dois arquivos principais:

- [ ] [WifiManager.h](https://mackcloud.mackenzie.br/gitlab/iot-devices/software-update-ota/-/blob/main/OTA_MANAGER/src/OtaManager.h?ref_type=heads): Definição da classe, enumerações de estado e assinatura dos métodos estáticos.
- [ ] [WifiManager.cpp](https://mackcloud.mackenzie.br/gitlab/iot-devices/software-update-ota/-/blob/main/OTA_MANAGER/src/OtaManager.cpp?ref_type=heads): Implementação da lógica de eventos, máquina de estados, controle do driver Wi-Fi.

## Dependências e Como Utilizar

Para utilizar esta classe, o ambiente de desenvolvimento deve atender aos seguintes requisitos:

- [ ] Hardware da família **ESP32**.
- [ ] **ESP-IDF v5.x**.

### Configuração (Kconfig)

Se desejar utilizar configurações padrão sem recompilar o código fonte, execute `idf.py menuconfig` e navegue até **Wifi Manager Configuration**:

* **WIFI_SSID**: SSID padrão da rede.
* **WIFI_PASSWORD**: Senha padrão.
* **WIFI_MAX_RETRIES**: Número máximo de tentativas de reconexão antes de declarar falha.
* **WIFI_AUTO_RESET**: Se ativado, o ESP32 reinicia automaticamente após exceder o limite de tentativas.

### **Implementando Módulo**
1. Copie os arquivos [WifiManager.h](https://mackcloud.mackenzie.br/gitlab/iot-devices/software-update-ota/-/blob/main/OTA_MANAGER/src/OtaManager.h?ref_type=heads) e [WifiManager.cpp](https://mackcloud.mackenzie.br/gitlab/iot-devices/software-update-ota/-/blob/main/OTA_MANAGER/src/OtaManager.cpp?ref_type=heads) diretamente na pasta em que o arquivo `main` esta ou em uma subpasta (por exemplo, /src).
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
        
    /* Observação Importante:
      O uso do Kconfig é recomendado para aplicações finais, pois permite alterar credenciais e parâmetros sem modificar o código-fonte.
      A inicialização via código tem prioridade sobre os valores configurados no Kconfig.*/
   ```
    
5. (Opcional) Caso a aplicação dependa obrigatoriamente de conectividade Wi-Fi para continuar a execução, utilize o método síncrono: `WifiManager::waitForConnection();`

6. Acompanhe e gerencie os estados da conexão e eventos conforme a documentação completa abaixo:

## API Reference

Método | Descrição 
--------|-----|---------
void init(WifiConfig cfg) | 






