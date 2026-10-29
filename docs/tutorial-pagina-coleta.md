# Tutorial · a página de coleta

Como usar a página do [`coleta_wifi`](../software/coleta_wifi/coleta_wifi.ino): o que cada botão
faz, o que apertar em cada situação da coleta e o que fazer com cada mensagem. A matriz do que
gravar (quantas rajadas, de quais obstáculos) está no [plano de coleta](plano-coleta.md).

---

## 1. Como funciona, em um minuto

```
 celular ── Wi-Fi "CARRINHO-COLETA" ──> ESP32-S3 (servidor da página, em 192.168.4.1)
    │                                        │
    ├─ cada botão manda um comando ────────> executa: grava, dirige, roda um roteiro
    └─ a cada 0,4 s pergunta o estado <───── distância, modo, contagens, última mensagem
```

- **O próprio carrinho cria a rede Wi-Fi.** Não precisa de internet nem de roteador.
- **As leituras ficam na memória do ESP32,** agrupadas em **rajadas**: cada trecho gravado, com
  rótulo, obstáculo, velocidade e tipo. Cabem ~25 min de gravação.
- **O CSV só é montado quando você baixa.** Nessa hora o carrinho aplica o pré-processamento
  rajada por rajada e corta cada uma em janelas de 1 s.
- **O jeito recomendado é o painel no notebook** (§2.1): os mesmos botões, mais o gráfico ao vivo e
  o que falta gravar, e cada rajada copiada para o disco do computador.
- **O carrinho guarda cada rajada na memória flash** até o painel copiar. Ela volta depois de um
  reinício.

---

## 2. Ligar e abrir a página

1. Grave o `coleta_wifi` com a chave desligada. Na IDE: ESP32S3 Dev Module · Flash Mode **OPI
   80MHz** · Flash Size **32MB** · PSRAM **OPI PSRAM**, porta COM10.
2. Tire o USB, confira que o fio do diodo está no pino 5V e ligue a chave. O LED da placa pisca em
   **verde**.
3. No celular, conecte na rede **CARRINHO-COLETA**, senha **`carrinho123`**. Se o Android avisar
   que a rede "não tem internet", escolha **manter conectado**.
4. No navegador, digite **`http://192.168.4.1`**, com `http://` e não `https://`.
5. Passe a mão na frente do sensor: o número grande no topo da página tem que acompanhar.

> Embaixo da mensagem, a página mostra há quanto tempo o carrinho está ligado, quantas vezes ele
> reiniciou e **o motivo do último reinício**. Se o tempo voltar a zero no meio da coleta, olhe
> esse motivo: "queda de tensão" é pilha fraca ou pico dos motores; "energia cortada" é chave ou mau
> contato no suporte de pilhas.

---

## 2.1 O painel no computador (recomendado)

O painel tem os mesmos botões da página, no navegador do notebook, e guarda cada rajada no disco do
computador. O celular continua funcionando ao mesmo tempo.

1. Conecte o **notebook** na rede **CARRINHO-COLETA** uma vez. Depois disso, o painel reconecta
   sozinho se o Windows trocar de rede.
2. Na pasta do projeto, clique duas vezes em **`Abrir painel de coleta.bat`**. Abre uma janela
   preta, que tem que **ficar aberta**, e o navegador em `http://localhost:8321`.
3. O selo no topo fica verde: **carrinho conectado**.

| Parte do painel | O que faz |
|---|---|
| Distância e gráfico | os últimos 15 s. Pontos: leitura crua. Linha: filtrada, o que o modelo vê. Tracinho vermelho no alto: sem eco. Fundo vermelho: gravando. Faixa roxa: 8 a 35 cm. Tracejado: 15 cm, onde o Aproximar para |
| Rótulo, Obstáculo, Velocidade | os mesmos da página |
| Roteiros e Dirigir | os mesmos. Também dá para dirigir **segurando as setas do teclado**; a **barra de espaço** é PARAR TUDO |
| Coleta | janelas de cada classe, com a meta de 600 |
| O que falta | o [plano](plano-aproximando-bloqueado.md) em casas: Empurrar, Parado e Aproximar por obstáculo, e o livre. **Clique numa casa** e o painel escolhe rótulo, obstáculo e velocidade |
| Últimas rajadas | cada rajada copiada, com o desenho das leituras e uma conferência da forma: `✓ forma ok` ou `⚠` com o motivo. **remover** manda para a lixeira, que fica em Avançado e permite restaurar |
| Conferir dados | roda o `preparar.js` e gera `treino.csv` e `teste.csv` |
| Mensagens | do carrinho e do painel |

**Por que nada se perde:**

- Cada rajada fechada ganha um número único e fica na flash do carrinho. O painel copia as que
  faltam poucos segundos depois. O topo mostra `no computador: N · no carrinho: M · tudo copiado`.
- Se o notebook cair da rede ou o carrinho reiniciar, quando a conexão volta o painel busca o que
  faltou. Só se perde a rajada que estava em andamento quando o carrinho desligou.
- **Desfazer**, no painel ou no celular, tira a rajada do computador também.
- **Apagar a memória do carrinho** (em Avançado) não apaga nada do computador: o computador é o
  arquivo, o carrinho é o depósito. O painel só deixa apagar depois de copiar tudo.
- Pode fechar e abrir o painel quando quiser: ele continua de onde parou.

**Os arquivos ficam em `dados/coleta/`:**

| Arquivo | O que é |
|---|---|
| `coleta_detalhado.csv` | entrada do `preparar.js`, **sempre atualizado** |
| `coleta_cru.csv` | leituras cruas, para reprocessar sem coletar de novo |
| `rajadas.jsonl` | o arquivo de tudo o que chegou. **Não apague** |
| `treino.csv`, `teste.csv` | gerados pelo botão Conferir; vão para o Edge Impulse |

**Desligar o carrinho** (trocar pilhas, pausa): espere a última rajada aparecer em *Últimas
rajadas*, desligue e ligue de novo. O painel reconecta sozinho.

**Avisos do painel:**

| Aviso | O que fazer |
|---|---|
| `Sem conexão com o carrinho… O notebook está na rede X` | ligar o carrinho ou apertar **Conectar na CARRINHO-COLETA** |
| `O sensor não devolve eco há N s` | mão a 20 cm do sensor; se continuar, conferir pilhas e fios |
| `não consegui gravar …: feche o arquivo no Excel` | fechar o CSV no Excel; o painel tenta de novo |
| `⚠ só X% das janelas descendo` (aproximando) | remover e repetir; a 30° numa parede lisa, usar 15° |
| `⚠ só X% paradas entre 8 e 35 cm` (bloqueado) | remover e repetir; conferir o batente e a distância no gráfico |
| `⚠ X% parecem aproximação` (livre) | remover e repetir em um lugar mais aberto |
| `⚠ nenhum eco: o sensor está medindo?` | mão a 20 cm do sensor antes de seguir |

> O Node.js precisa estar instalado (versão LTS de nodejs.org). O atalho avisa se não estiver.

---

## 3. A página, parte por parte

```
┌──────────────────────────────────────────────────┐
│ ① 87 cm                                  parado  │
│   rajada 12 (aproximando, caixa): 3.4 s, 15 jan. │
├──────────────────────────────────────────────────┤
│ ② RÓTULO E CENÁRIO                               │
│   [livre] [aproximando] [bloqueado]              │
│   [Obstáculo: caixa ▾]   Velocidade ───●── 200   │
├──────────────────────────────────────────────────┤
│ ③ ROTEIROS AUTOMÁTICOS                           │
│   [Aproximar] [Empurrar] [Livre andando] [Rajada 3 s]
├──────────────────────────────────────────────────┤
│ ④ DIRIGIR (SEGURE O BOTÃO)                       │
│              [▲]                                 │
│         [◀]  [■]  [▶]                            │
│              [▼]                                 │
│   [Gravar] [Parar gravação]                      │
├──────────────────────────────────────────────────┤
│ ⑤ [               PARAR TUDO               ]     │
├──────────────────────────────────────────────────┤
│ ⑥ DADOS                                          │
│   classe   rajadas janelas parede caixa cadeira… │
│   [Desfazer última rajada] [CSV detalhado] [CSV Edge Impulse]
│   [Cru] [Formato A] [Apagar tudo]                │
└──────────────────────────────────────────────────┘
```

| | Parte | O que faz |
|---|---|---|
| ① | **Distância e mensagem** | o número é a distância já filtrada, em cm. À direita, o modo: `parado`, `contagem`, `roteiro` ou `GRAVANDO`. Embaixo, a última mensagem do carrinho (ver §7) |
| ② | **Rótulo** | os três botões escolhem a classe das gravações **manuais** (Gravar e Rajada 3 s). O selecionado fica azul. **Os roteiros ignoram esse botão:** cada um grava a sua classe |
| ② | **Obstáculo** | o que está na frente: nenhum, parede, caixa, cadeira, canto ou outro. Vai para o CSV detalhado e é o que permite separar treino e teste de forma justa. **Troque antes de cada rajada** |
| ② | **Velocidade** | de 100 a 255. Vale para os roteiros e para os botões de dirigir, e fica registrada em cada rajada |
| ③ | **Roteiros** | o carrinho dirige e grava sozinho (§4) |
| ④ | **Dirigir** | anda **enquanto o botão estiver pressionado** e para ao soltar. Se o celular perder a conexão, o carrinho para sozinho em menos de 1 s. O ■ para os motores |
| ④ | **Gravar / Parar gravação** | gravação manual, com o rótulo escolhido em ②. Começa 2 s depois de apertar e vai até **Parar gravação** |
| ⑤ | **PARAR TUDO** | para os motores e a gravação. Numa gravação manual, **guarda** o trecho; no meio de um roteiro, **descarta** a rajada em andamento |
| ⑥ | **Tabela** | por classe: quantas rajadas, quantas janelas (a meta é 600) e quantas rajadas de cada obstáculo. A última linha mostra a memória usada |
| ⑥ | **Desfazer última rajada** | apaga a rajada mais recente. Use sempre que algo saiu errado |
| ⑥ | **Downloads** | ver §6 |
| ⑥ | **Apagar tudo** | pede confirmação e zera a memória. Só funciona com o carrinho parado |

---

## 4. Os quatro botões de roteiro

Todos começam com **2 s de contagem** (LED amarelo piscando) para você sair da frente do sensor.
Durante a contagem e o roteiro, **qualquer botão de dirigir, o ■ ou PARAR TUDO cancela e descarta**
a rajada.

### Aproximar → grava `aproximando`

1. Anda reto na velocidade escolhida.
2. Depois de 0,3 s andando, começa a gravar (LED vermelho).
3. Para sozinho quando o sensor chega a **15 cm** do alvo, ou depois de 8 s.

Se já começar a menos de 20 cm, nem sai do lugar: `já está perto`.

### Empurrar → grava `bloqueado`

1. Anda reto até encostar no **batente** (LED azul enquanto anda).
2. Espera a distância ficar parada por 0,5 s, entre **8 e 35 cm**.
3. Grava **3 s** com o motor ainda forçando contra o batente e depois para.

Se não travar em 6 s: `não travou`. Se o sensor chegar a menos de 8 cm, descarta: `zona cega`.

### Livre andando → grava `livre`

1. Confere se há mais de **1 m** livre à frente. Se não houver: `obstáculo à frente`.
2. Anda e grava **3 s**.
3. Se aparecer algo a menos de 1 m no caminho, para e corta o último 1 s da gravação.

### Rajada 3 s → grava o rótulo selecionado em ②

Grava exatamente 3 s, com o carrinho fazendo o que você mandar. Os botões de dirigir **não
cancelam** a Rajada 3 s: dá para segurar ◀ para gravar o carrinho girando, por exemplo. Serve para
as situações sem roteiro: parado, girando, dando ré, obstáculo vindo.

---

## 5. Receitas: exatamente o que apertar

Antes de cada uma: **escolha Obstáculo e Velocidade** em ②.

### `aproximando`, de frente ou em 30°

1. Obstáculo: o alvo (parede, caixa, cadeira ou canto). Velocidade: 150, 200 ou 250.
2. Ponha o carrinho na marca de partida (150 ou 100 cm; 60 cm só a 150), apontado para o alvo.
3. **Aproximar** e saia da frente.
4. Espere a mensagem `rajada N (aproximando, …): … janelas`.

### `bloqueado` empurrando

1. Batente preso no chão na marca de 10, 15 ou 20 cm do obstáculo.
2. Obstáculo: o que está atrás do batente. Velocidade: 150 ou 200.
3. Carrinho a uns 50 cm do batente, apontado para ele.
4. **Empurrar** e saia da frente.

### `bloqueado` parado

1. Rótulo **bloqueado** em ②. Obstáculo: o que está na frente.
2. Carrinho parado a 10, 20 ou 30 cm dele (confira o número no topo da página).
3. **Rajada 3 s**, sem tocar em mais nada.

### `livre` andando

1. Obstáculo: **nenhum**. Velocidade: 150, 200 ou 250.
2. Carrinho num lugar com mais de 2 m livres à frente.
3. **Livre andando.**

### `livre` parado, girando ou dando ré

| Situação | Rótulo | Como |
|---|---|---|
| Parado em espaço aberto | livre | **Rajada 3 s** e não toque em nada |
| Girando em espaço aberto | livre | **Rajada 3 s** e segure ◀ ou ▶ desde a contagem até o fim |
| Ré se afastando de um obstáculo | livre | carrinho a 30 cm de um obstáculo, **Rajada 3 s**, e segure ▼ **só quando o LED ficar vermelho** |

### `aproximando` com o obstáculo vindo (opcional)

Rótulo **aproximando**, carrinho parado, **Rajada 3 s**. Quando o LED ficar vermelho, alguém
empurra a caixa devagar, de ~100 cm até ~20 cm do sensor.

### Situação fora das receitas

Rótulo certo em ②, **Gravar**, mantenha a situação, **Parar gravação**. Comece já na situação: a
gravação manual não corta nada sozinha.

---

## 6. Baixar os dados

**Com o painel aberto (§2.1), você não precisa destes botões:** o `coleta_detalhado.csv` já está em
`dados/coleta/`. Os downloads da página são o caminho de reserva, a partir da memória do carrinho.
Nunca junte os dois no `preparar.js`: as mesmas rajadas entrariam duas vezes.

| Botão | O que vem | Para que serve |
|---|---|---|
| **CSV detalhado** | uma janela por linha, com rajada, tipo, obstáculo e velocidade | **é o que vai para o `preparar.js`** e daí para o Edge Impulse |
| CSV Edge Impulse | as janelas sem as colunas de rajada | só conferência rápida: não use para treinar, porque não dá para separar o teste por rajada |
| Cru | uma leitura por linha, antes do filtro | arquivo de segurança: permite reprocessar sem coletar de novo |
| Formato A | 1 valor por linha, rótulo por faixa de distância | o contraste da Aula 19 (a armadilha do limiar) |

- **Quando baixar:** a cada ~20 rajadas e no fim. Cada download traz tudo o que está na memória,
  então o último sempre substitui os anteriores.
- **O carrinho precisa estar parado.** Se estiver gravando, a página responde `pare a gravação antes
  de baixar`.
- **No celular, o arquivo vai para Downloads** como `coleta_detalhado.csv`. Downloads repetidos
  ganham `(1)`, `(2)`... Passe o mais recente para a pasta `dados/` com o nome da sessão, por exemplo
  `2026-10-28_sessao1_detalhado.csv`, e rode:

```bash
node dados/preparar.js dados/2026-10-28_sessao1_detalhado.csv
```

---

## 7. Mensagens da página

| Mensagem | O que aconteceu | O que fazer |
|---|---|---|
| `… em 2 s... saia da frente do sensor` | contagem começou | sair da frente |
| `GRAVANDO livre` (ou outra classe) | gravação manual em andamento | — |
| `rajada N (classe, obstáculo): X s, Y janelas` | rajada guardada | conferir se faz sentido; se não, **Desfazer** |
| `rajada curta demais, descartada` | gravou menos de ~1 s útil | partir de mais longe ou gravar por mais tempo |
| `rajada descartada: roteiro interrompido…` | PARAR TUDO, ■ ou dirigir no meio de um roteiro | repetir |
| `rajada descartada: perto demais (zona cega)…` | no Empurrar, o sensor chegou a menos de 8 cm | afastar o batente |
| `já está perto: afaste o carrinho do alvo` | Aproximar começou a menos de 20 cm | partir de mais longe |
| `não travou: confira o batente…` | Empurrar andou 6 s sem estabilizar | conferir se o batente está preso e no caminho da roda da frente |
| `obstáculo à frente: procure mais espaço` | Livre começou com algo a menos de 1 m | mudar de lugar ou de direção |
| `ocupado: pare antes (p)` | apertou um roteiro com outro em andamento | esperar ou PARAR TUDO |
| `memória cheia: baixe o CSV e apague` | 25 min gravados no carrinho | com o painel: **Apagar a memória do carrinho** em Avançado. Sem o painel: baixar tudo e depois **Apagar tudo** |
| `rajada N apagada` | Desfazer funcionou | — |
| `sem conexão` (no lugar do modo) | o celular saiu da rede ou o carrinho reiniciou | reconectar e olhar o motivo do último reinício |
| `recuperadas N rajadas salvas na flash` | o carrinho reiniciou e recarregou a cópia de reserva | seguir; o painel copia o que faltar |
| `cópia antiga na flash descartada (formato anterior ao painel)` | primeira ligação depois de gravar o firmware novo | normal, uma vez só |

---

## 8. O LED da placa

| Cor | Significa |
|---|---|
| verde piscando fraco | pronto, parado |
| amarelo piscando | contagem de 2 s: saia da frente |
| azul | roteiro andando, ainda sem gravar (Empurrar esperando travar, por exemplo) |
| vermelho | gravando |

---

## 9. Os mesmos comandos pelo Monitor Serial

Com o USB conectado (115200), as letras fazem o mesmo que os botões. Lembre que, no USB, o fio do
diodo fica desconectado do pino 5V.

| Letra | Botão | Letra | Botão |
|---|---|---|---|
| `0` `1` `2` | rótulo livre / aproximando / bloqueado | `F` `B` `L` `R` | dirigir (sem precisar segurar) |
| `o` | passa para o próximo obstáculo | `S` | parar os motores |
| `x` | Aproximar | `+` `-` | velocidade ±10 |
| `y` | Empurrar | `n` | resumo por classe |
| `w` | Livre andando | `t` | distância crua e filtrada |
| `r` | Rajada 3 s | `d` / `D` | imprime o CSV Edge Impulse / detalhado |
| `g` | Gravar | `z` | apagar a memória do carrinho |
| `p` | PARAR TUDO | `?` | lista de comandos |
| `u` | Desfazer última rajada | | |

---

## 10. Problemas comuns

| Problema | Causa provável | Solução |
|---|---|---|
| A página não abre | celular em outra rede, `https://` digitado, ou dados móveis no lugar do Wi-Fi | confirmar a rede CARRINHO-COLETA, digitar `http://192.168.4.1`, desligar os dados móveis |
| A distância fica em `--` | página sem resposta do carrinho | recarregar a página |
| Os botões não respondem | conexão caiu | ver se a rede ainda está conectada |
| A tabela zerou / o tempo de "ligado há" voltou a zero | o carrinho reiniciou | olhar o motivo na página. "Queda de tensão": pilhas novas (ou NiMH) e velocidade menor. "Energia cortada": apertar as molas do suporte de pilhas e conferir a chave. A tabela volta sozinha com a cópia da flash |
| O carrinho reinicia no Empurrar | pilhas fracas com o motor travado | pilhas novas; velocidade 150 no Empurrar |
| O carrinho não sai do lugar no roteiro | velocidade baixa demais para o piso | subir a velocidade |
| `livre` sempre interrompido no meio da sala | o sensor está vendo o chão | refazer o teste 1.3 do plano de testes e inclinar o sensor para cima |

---

## 11. Cola de bolso

1. Chave ligada · notebook na rede **CARRINHO-COLETA** · **Abrir painel de coleta.bat** · selo
   verde · a distância acompanha a mão.
2. Clique na casa de *O que falta* que vai gravar: o painel acerta rótulo, obstáculo e velocidade.
3. Carrinho na marca → **roteiro** → saia da frente → a rajada aparece em *Últimas rajadas* com
   `✓ forma ok`.
4. Saiu errado ou veio `⚠`? **Desfazer a última rajada** (ou **remover** na lista).
5. No fim: **Conferir dados** → checklist em OK → `dados/coleta/treino.csv` e `teste.csv` para o
   Edge Impulse.
