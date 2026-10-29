# TinyML no carrinho 2WD (ESP32-S3) — plano adaptado

Adaptação do plano do SMARS 2.0 (`Robo/docs/tinyml-smars-2.0.md` e `Robo/docs/edge-impulse.md`)
para o carrinho 2WD com ESP32-S3. O problema e os parâmetros fixados pela professora continuam os
mesmos. O que muda é o hardware e tudo o que a tentativa 1 do SMARS ensinou.

Documentos deste plano:

| Documento | Para quê |
|---|---|
| este arquivo | o que muda, o pipeline, o roteiro geral e as metas |
| [`plano-testes.md`](plano-testes.md) | testes de bancada, do sinal, da cadeia de dados e do modelo no robô |
| [`plano-coleta.md`](plano-coleta.md) | o protocolo de coleta, com a matriz de variações e as cotas |
| [`tutorial-pagina-coleta.md`](tutorial-pagina-coleta.md) | como usar a página de coleta: cada botão, cada roteiro e cada mensagem |
| [`plano-treino.md`](plano-treino.md) | o Edge Impulse clique a clique, critérios de aceitação e o que fazer se falhar |

Ferramentas:

| Arquivo | O que faz | Estado |
|---|---|---|
| [`software/coleta_wifi`](../software/coleta_wifi/coleta_wifi.ino) | coleta sem cabo: página no celular, roteiros automáticos, download do CSV | compila: 945 KB (72% da partição padrão) |
| [`dados/preparar.js`](../dados/preparar.js) | confere a qualidade e separa treino e teste por rajada | testado com dado sintético |
| [`teste_hcsr04`](../software/teste_hcsr04/teste_hcsr04.ino) | distância ao vivo no Monitor Serial | compila |
| `software/inferencia` | o modelo rodando na placa | depois do treino (precisa do .zip do Edge Impulse) |

---

## 1. O que muda em relação ao SMARS 2.0

| | SMARS 2.0 | Carrinho 2WD | Consequência no plano |
|---|---|---|---|
| Placa | ESP32 (WROOM-32) | ESP32-S3 WROOM-2 N32R16V | Ferramentas: Flash Mode **OPI 80MHz**, Flash Size **32MB**, PSRAM **OPI PSRAM** |
| Sem fio | Bluetooth clássico (SPP) | **não existe no S3** | coleta por **Wi-Fi**: o carrinho cria a rede e serve uma página |
| Captura do CSV | `capturar.ps1` + `extrair_csv.js` | **download direto** pela página | some o risco de perder dados ao abrir a porta serial |
| Sensor | TRIG 27 · ECHO 35 · divisor 1k/2k | **TRIG GPIO1 · ECHO GPIO2 · divisor 1k/1,5k** | o pack chega a 6 V; com 2k o Echo passaria de 3,6 V |
| Motores | esquerdo 26/25 · direito 33/32 | **A = direita 15/16 · B = esquerda 4/5** | mesmos pinos dos testes que já funcionam |
| Energia | 9 V + buck | 4×AA + diodo 1N5819 para o ESP32 | o Wi-Fi sai com potência reduzida para aliviar as pilhas |
| Altura do sensor | — | baixa, uns 6 cm do chão | risco de **eco do chão** confundir `livre` (teste 1.3) |
| Gravação | rajadas manuais (`g` … `p`) | **roteiros automáticos** + rajadas manuais | o carrinho dirige e grava sozinho: rótulo consistente |
| Treino × teste | divisão automática do Edge Impulse | **separação por rajada** + upload em Training e Testing | a nota do teste deixa de ser inflada pela sobreposição |
| Zona cega | "sem eco" virava 200 cm | **"sem eco" logo depois de < 20 cm continua perto** + batente físico | ataca a causa raiz do F1 de 0,14 em `bloqueado` |

---

## 2. O que o modelo decide (mantido)

| # | Classe | Situação | Como se grava |
|---|---|---|---|
| 0 | `livre` | nada a menos de ~1 m à frente: andando, parado, girando ou se afastando | roteiro **Livre andando** + rajadas manuais |
| 1 | `aproximando` | distância **caindo**: o carrinho indo em direção a algo, de ~150 cm até ~15 cm | roteiro **Aproximar** |
| 2 | `bloqueado` | algo **perto (8 a 35 cm) e a distância não muda**: empurrando ou parado diante do obstáculo | roteiro **Empurrar** + rajadas paradas |

O argumento do projeto continua o mesmo: a 20 cm, `aproximando` e `bloqueado` dão a mesma leitura
instantânea. Só a forma da janela de 1 s separa as duas.

**Fora do escopo, de propósito:** carrinho parado com obstáculo entre 35 e 100 cm. O controle
(§5 do plano do SMARS) nunca para nessa faixa: em `aproximando` ele só reduz a velocidade. A decisão
está registrada para não ser descoberta na apresentação.

---

## 3. O pipeline `(*)` — idêntico na coleta e na inferência

| Etapa | Constante | Novidade em relação ao SMARS |
|---|---|---|
| µs → cm | `/ 58` | — |
| sem eco (timeout 25 ms, ~4 m) | `TIMEOUT_US = 25000` | — |
| **zona cega** | `PERTO_CM = 20` | **nova**: sem eco logo depois de uma leitura < 20 cm repete a última leitura em vez de virar 200 |
| limitação de faixa | `DIST_MIN = 2`, `DIST_MAX = 200` | — |
| **mediana de 3** | `MEDIANA_N = 3` | **nova**: tira picos isolados antes da média, que os espalharia por 5 amostras |
| média móvel | `MEDIA_N = 5` | — |
| normalização | `/ 200` → 0..1 | — |
| janela | 10 × 100 ms = 1 s | — |
| passo entre janelas no CSV | `STRIDE = 2` (80% de sobreposição) | — |

O pipeline está numa `struct Pipeline` no `coleta_wifi.ino`. **O sketch de inferência vai copiar a
mesma struct**, sem alterar nada. Se uma constante divergir, o sintoma conhecido aparece: acurácia
alta no Studio e robô burro na prática.

O que fica na RAM é a leitura **crua** (0 = sem eco). O pipeline só roda no download, rajada por
rajada. Por isso o CSV "Cru" da página permite reprocessar tudo sem coletar de novo.

---

## 4. Roteiro geral

Cada sessão tem um **ponto de parada**: se ele não acontecer, não siga adiante.

| # | Sessão | Tempo | Ponto de parada | Onde |
|---|---|---|---|---|
| 0 | Preparação física | 30 min | sensor fixo na posição final, batente pronto, cenário montado | [coleta §2](plano-coleta.md#2-antes-de-começar) |
| 1 | Bancada e sinal | 1 h | blocos 0 a 2 passando | [testes, blocos 0–2](plano-testes.md) |
| 2 | Ensaio da cadeia | 20 min | 2 rajadas de cada roteiro, CSV baixado e `preparar.js` mostrando as formas certas | [testes, bloco 3](plano-testes.md#bloco-3--cadeia-de-dados) |
| 3 | Coleta completa | 1 h 30 | `preparar.js` com o checklist todo OK | [plano-coleta](plano-coleta.md) |
| 4 | Treino e avaliação | 40 min | Model testing dentro dos critérios | [plano-treino](plano-treino.md) |
| 5 | Inferência na bancada e no chão | 1 h | blocos 5 e 6 passando | [testes, blocos 5–6](plano-testes.md#bloco-5--inferência-na-bancada) |
| 6 | Modelo comandando os motores | 2 h | 3 min na sala sem travar | [testes, bloco 7](plano-testes.md#bloco-7--modelo-comandando-os-motores) |
| 7 | Vídeo, prints e repositório | 1 h | entrega da Aula 21 | — |

---

## 5. Metas de acerto

"Máximo de acerto" aqui é medido em **situações que o modelo nunca viu**, e não na tela do
Classifier:

| Onde se mede | Meta |
|---|---|
| Model testing do Edge Impulse (rajadas separadas) | acurácia **≥ 92%** · F1 **≥ 0,90 em cada classe** |
| `bloqueado` (a classe que justifica o projeto) | recall **≥ 0,90**; no máximo 5% virando `aproximando` |
| Estabilidade | duas rodadas de treino com diferença **≤ 2 pontos** |
| No robô, por cenário (bloco 6) | **≥ 18 de 20** repetições certas |
| Tempo para reconhecer `bloqueado` | **≤ 1,5 s** depois de travar |

Como os parâmetros de treino são fixos (20 ciclos, 0,005), o acerto vem do dado. As alavancas, em
ordem de impacto:

1. **Classes fisicamente separáveis:** a zona cega tratada no pipeline e o batente mantendo o sensor
   a 10 cm ou mais.
2. **Rótulo consistente:** os roteiros automáticos gravam só o fenômeno e cortam as transições.
3. **Cobertura:** a matriz obstáculo × velocidade × ângulo × distância inicial do plano de coleta.
4. **Mesma condição do uso real:** motores ligados, sensor na posição final, mesmo piso e as mesmas
   velocidades.
5. **Avaliação honesta:** teste separado por rajada, para o número refletir o robô.
6. **Corrigir na coleta, não no treino:** o SMARS mostrou que filtrar o CSV piora o resultado no
   dado real.

---

## 6. O que precisa de decisão do grupo

- **Nome do projeto no Edge Impulse**, que aparece no print da entrega. Sugestão:
  `tinyml-ultrassonico-2wd-grupoX`.
- **Onde fica o repositório:** estes arquivos estão na pasta do chassi. Para a entrega da Aula 21
  (`src/`, `model/`, `docs/`), dá para copiar para o repositório `Robo` ou criar um novo.
- **Velocidade de cruzeiro do robô final:** a coleta cobre 150, 200 e 250. Se o robô final for usar
  outra, inclua essa velocidade na matriz.
