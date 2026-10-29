# Software

## Versão 1 — SMARS (Arduino Uno)

| Pasta | Conteúdo |
|---|---|
| `smars_completo/` | **Firmware final** — controle Bluetooth com bloqueio de avanço por obstáculo + modo autônomo (`X` liga / `x` desliga) |
| `smars_bluetooth/` | Só controle Bluetooth (versão didática) |
| `smars_desvio/` | Só navegação autônoma com desvio de obstáculos (versão didática, v3) |
| `historico/` | Versões iniciais (v1) do Bluetooth e do desvio, com os problemas anotados no cabeçalho |

## Versão 2 — carrinho 2WD (ESP32-S3)

| Pasta | Conteúdo |
|---|---|
| `carrinho_serial/` | Controle pelo Monitor Serial: `f` `t` `e` `d`, curvas, `p`, `v 150`, tempo opcional (`f 1000`) |
| `teste_hcsr04/` | Teste do sensor: distância crua no Monitor Serial com barra visual, sem motor |
| `coleta_wifi/` | **Firmware da coleta para o Edge Impulse**: Wi-Fi próprio, página de controle, roteiros automáticos, rajadas na flash, `/rajadas` para o painel do computador |

O painel do computador que conversa com o `coleta_wifi` está em [`../dados/`](../dados/) (`painel.js`,
aberto pelo atalho `Abrir painel de coleta.bat` na raiz).

## Compilar e gravar

**Versão 1**

1. Arduino IDE 2.x → *Library Manager* → instalar **Adafruit Motor Shield library** (v1, `AFMotor.h`).
2. Placa: **Arduino Uno**. Abra a pasta do sketch (ex.: `smars_completo/smars_completo.ino`).
3. Antes de conectar o USB, retire o jumper PWR do shield ou a bateria.
4. Upload. Monitor serial a **9600 bps** (`smars_completo`, `smars_bluetooth`) ou **115200** (`smars_desvio`).

Os três sketches compilam sem erros nem avisos (`arduino-cli compile --fqbn arduino:avr:uno --warnings all`).

**Versão 2**

1. Arduino IDE 2.x com o pacote **esp32 by Espressif** (testado com o core 3.3).
2. Placa **ESP32S3 Dev Module** · Flash Mode **OPI 80MHz** · Flash Size **32MB** · PSRAM **OPI PSRAM**.
3. Solte do pino 5V o fio que vem do diodo e grave pela USB nativa.
4. Monitor serial a **115200**, em qualquer uma das duas entradas USB da placa.

Nenhum dos três precisa de biblioteca externa. O `coleta_wifi` ocupa 990 KB (75% da partição padrão)
e 170 KB de RAM.

## Constantes de calibração (topo de cada sketch)

**Versão 1**

| Constante | Padrão | O que faz |
|---|---|---|
| `VEL_MAX_E`, `VEL_MAX_D` | 255 | Teto de cada motor; abaixe o mais forte se o carrinho puxa para um lado |
| `DIST_PARAR` | 20 cm | Obstáculo: bloqueia avanço (manual) / desvia (autônomo) |
| `DIST_REDUZIR` | 45 cm | Início da desaceleração no autônomo |
| `DIST_LIVRE` | 40 cm | Distância para considerar uma direção livre ao girar |
| `T_MEIA_VOLTA_MS` | 1200 ms | Tempo de giro de ~180° (último recurso) — calibrar no piso |
| `VEL_CRUZEIRO`, `VEL_MINIMA` | 220, 130 | Velocidades do modo autônomo |

**Versão 2 (`coleta_wifi`)**

| Constante | Padrão | O que faz |
|---|---|---|
| `INVERTE_A`, `INVERTE_B` | false | Inverte o sentido de um motor sem refazer a fiação |
| `PERIODO_MS`, `TIMEOUT_US` | 100 ms, 25 ms | (*) 10 leituras por segundo, eco até ~4 m |
| `DIST_MIN`, `DIST_MAX`, `NORM` | 2, 200, 200 | (*) faixa em cm e normalização para 0–1 |
| `MEDIANA_N`, `MEDIA_N`, `JANELA` | 3, 5, 10 | (*) filtros e janela de 1 s |
| `PARADA_CM` | 15 cm | onde o roteiro Aproximar para |
| `LIVRE_MIN_CM` | 100 cm | o roteiro Livre só grava com isto livre à frente |
| `ZONA_CEGA_CM` | 8 cm | abaixo disto o HC-SR04 não mede: a rajada é descartada |

As constantes marcadas com `(*)` precisam ser **idênticas** no sketch de inferência: elas definem a
escala que o modelo vê.

A explicação completa do funcionamento está no README principal, seção 8.
