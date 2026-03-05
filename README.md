# SonarESP32 📡

Este projeto utiliza um **ESP32** e um sensor ultrassônico (HC-SR04) para medir distâncias em tempo real, funcionando como um sonar digital. É ideal para aplicações de robótica, detecção de obstáculos ou monitoramento de nível de tanques.

## 🚀 Funcionalidades

* Medição de distância em centímetros (cm) ou polegadas (in).
* Leitura em tempo real via Monitor Serial.

## 🛠️ Hardware Necessário

| Componente | Quantidade |
| :--- | :--- |
| ESP32 (DevKit V1 ou similar) | 1 |
| Sensor Ultrassônico HC-SR04 | 1 |
| Cabos Jumper | 4 |
| Protoboard | 1 |

## 🔌 Esquema de Ligação

Certifique-se de conectar os pinos corretamente para evitar danos ao sensor (o HC-SR04 geralmente opera em 5V, mas o ESP32 usa 3.3V nos pinos de dados).

| HC-SR04 | ESP32 | Observação |
| :--- | :--- | :--- |
| **VCC** | VIN (5V) | Alimentação do sensor |
| **Trig** | GPIO 5 | Pino de gatilho (Output) |
| **Echo** | GPIO 18 | Pino de eco (Input) |
| **GND** | GND | Terra comum |

> **Nota:** Recomenda-se o uso de um divisor de tensão no pino **Echo** para reduzir o sinal de 5V para 3.3V antes de entrar no ESP32.

## 💻 Configuração do Software

### Pré-requisitos
1.  Tenha a [Arduino IDE](https://www.arduino.cc/en/software) instalada.
2.  Instale o suporte para placas ESP32 na IDE (Gerenciador de Placas).
3.  (Opcional) Instale a biblioteca `NewPing` para leituras mais estáveis.

### Instalação
1.  Clone este repositório:
    ```bash
    git clone [https://github.com/Captando/SonarESP32.git](https://github.com/Captando/SonarESP32.git)
    ```
2.  Abra o arquivo `.ino` na pasta principal com sua Arduino IDE.
3.  Selecione a placa **DOIT ESP32 DEVKIT V1**.
4.  Clique em **Upload**.

## 📖 Como Usar
Após o upload, abra o **Serial Monitor** (Baud rate: 115200). O sistema começará a exibir a distância medida pelo sensor a cada segundo (ou conforme configurado no código).

## 📄 Licença
Este projeto está sob a licença MIT - veja o arquivo [LICENSE](LICENSE) para detalhes.

---
Desenvolvido por [Captando](https://github.com/Captando)
