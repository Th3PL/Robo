# Software

| Pasta | Conteúdo |
|---|---|
| `smars_completo/` | **Firmware final** — controle Bluetooth com bloqueio de avanço por obstáculo + modo autônomo (`X` liga / `x` desliga) |
| `smars_bluetooth/` | Só controle Bluetooth (versão didática) |
| `smars_desvio/` | Só navegação autônoma com desvio de obstáculos (versão didática, v3) |
| `historico/` | Versões iniciais (v1) do Bluetooth e do desvio, com os problemas anotados no cabeçalho |

## Compilar e gravar

1. Arduino IDE 2.x → *Library Manager* → instalar **Adafruit Motor Shield library** (v1, `AFMotor.h`).
2. Placa: **Arduino Uno**. Abra a pasta do sketch (ex.: `smars_completo/smars_completo.ino`).
3. Antes de conectar o USB, retire o jumper PWR do shield ou a bateria.
4. Upload. Monitor serial a **9600 bps** (`smars_completo`, `smars_bluetooth`) ou **115200** (`smars_desvio`).

Os três sketches compilam sem erros nem avisos (`arduino-cli compile --fqbn arduino:avr:uno --warnings all`).

## Constantes de calibração (topo de cada sketch)

| Constante | Padrão | O que faz |
|---|---|---|
| `VEL_MAX_E`, `VEL_MAX_D` | 255 | Teto de cada motor; abaixe o mais forte se o carrinho puxa para um lado |
| `DIST_PARAR` | 20 cm | Obstáculo: bloqueia avanço (manual) / desvia (autônomo) |
| `DIST_REDUZIR` | 45 cm | Início da desaceleração no autônomo |
| `DIST_LIVRE` | 40 cm | Distância para considerar uma direção livre ao girar |
| `T_MEIA_VOLTA_MS` | 1200 ms | Tempo de giro de ~180° (último recurso) — calibrar no piso |
| `VEL_CRUZEIRO`, `VEL_MINIMA` | 220, 130 | Velocidades do modo autônomo |

A explicação completa do funcionamento está no README principal, seção 8.
