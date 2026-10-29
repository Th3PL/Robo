# Plano da coleta: aproximando e bloqueado

Continuação do [plano de coleta](plano-coleta.md), feito depois da sessão do `livre` de 28/10. Os
botões e as mensagens estão no [tutorial da página](tutorial-pagina-coleta.md).

**Tempo:** ~2 h, em duas metades com troca de pilhas no meio.

---

## 1. Onde a coleta está

| Classe | Já tem | Esta sessão | Total previsto |
|---|---|---|---|
| `livre` | 0 (o de 28/10 saiu sem eco, ver abaixo) | 31 Livre andando + 22 (parado, girando, ré) · ~580 | ~580 |
| `aproximando` | 0 | ~72 rajadas · ~600 | ~600 |
| `bloqueado` | 0 | 60 rajadas · ~660 | ~660 |

Com isso, a maior classe fica com 1,1× a menor (o limite é 1,5×) e cada uma passa de 400 janelas
de treino.

**O que os logs de 28/10 mostraram:**

- **O sensor não mediu nada.** As 8.166 leituras da sessão do livre e as 4.514 da primeira tentativa
  de bloqueado foram todas "sem eco": a página mostrava 200 o tempo todo. O livre de 28/10 não vale
  e tem que ser refeito. É também por isso que o Empurrar dava `não travou`: ele espera a distância
  parar entre 8 e 35 cm, e ela ficava em 200.
- **O carrinho reiniciou 10 vezes**, sempre com os motores ligando, com "energia cortada" ou "queda
  de tensão". Pilha fraca explica as duas coisas: o sensor é alimentado pelo mesmo 5 V do ESP32 e
  para de medir abaixo de ~4,5 V.
- **A flash guardou todas as rajadas** depois de cada reinício. **O computador recebeu 26 de 31**:
  as outras foram gravadas com o notebook fora da rede.

**Por isso:** a §2 começa pelo sensor e pelas pilhas, e o Empurrar fica só na velocidade 150. A
coleta passa a ser feita pelo **painel do computador** ([tutorial, §2.1](tutorial-pagina-coleta.md#21-o-painel-no-computador-recomendado)).
Ele copia da flash do carrinho tudo o que faltar, mostra se o sensor está sem eco e confere a forma
de cada rajada assim que ela chega.

---

## 2. Antes de começar (15 min)

1. **Pilhas novas.**
2. **Sensor medindo.** Ligue o carrinho, abra a página e ponha a mão a 20 cm do sensor: o número
   grande tem que acompanhar a mão. Se ficar em 200:

   | Teste | Resultado | Causa |
   |---|---|---|
   | ligue o cabo USB do ESP32 no notebook e repita a mão | passa a medir | energia: pilhas fracas (ou o 5 V caindo no diodo) |
   | | continua em 200 | fios: confira VCC, Trig (GPIO1), Echo (pelo divisor até GPIO2) e GND no sensor, e os dois resistores |
   | multímetro entre VCC e GND do sensor, sem USB | abaixo de 4,5 V | energia |

   Não grave nada antes de a mão aparecer no número.
3. **Firmware novo e livre de 28/10 fora.** Grave o `coleta_wifi` atualizado. Na primeira ligação,
   ele descarta sozinho a cópia antiga da flash, que é o livre sem eco, e mostra `cópia antiga na
   flash descartada`. No computador, feche o terminal do `receber.js` e mova os arquivos
   `2026-10-28_*` de `dados/` para uma pasta `dados/descartado/`, para não entrarem no treino.
4. **Wi-Fi do notebook.** Em *Configurações > Rede e Internet > Wi-Fi > Gerenciar redes
   conhecidas*, marque **Conectar automaticamente** na CARRINHO-COLETA. O painel também pede ao
   Windows para voltar a essa rede quando ele troca sozinho.
5. **Teste de mau contato (1 min).** Ligue o carrinho e deixe a página aberta, com os motores
   parados. Mexa no suporte de pilhas, na chave e na emenda dos fios. Se a página cair ou o
   "ligado há" voltar a zero, é mau contato: estique as molas do suporte e refaça a emenda antes de
   seguir.
6. **Abra o painel:** clique duas vezes em `Abrir painel de coleta.bat`, na pasta do projeto, e
   espere o selo verde **carrinho conectado**. A partir daqui, use a tabela *O que falta*: clicar
   numa casa acerta rótulo, obstáculo e velocidade.
7. **Teste de parada do Aproximar**, que também já é coleta. Selecione **caixa** no Obstáculo da
   página e, com a caixa de frente, parta
   de 150 cm, uma vez em cada velocidade: 150, 200 e 255. O carrinho tem que parar **sem
   encostar**: o filtro atrasa ~0,3 s, e em velocidade alta ele passa da marca de 15 cm. A maior
   velocidade que não encosta é o **teto** da §3.2. Se encostar, aperte **Desfazer** e não use
   essa velocidade.
8. **Teste de energia.** O teste do passo 7 também serve para isto. Se o carrinho reiniciar nele,
   troque as pilhas antes do Aproximar. O bloqueado não depende disto: ele é gravado sem motor
   (§3.1).

---

## 3. Ciclo por obstáculo

Repita o ciclo para cada obstáculo, nesta ordem: **parede**, **caixa**, **cadeira**, **canto**.
Antes de cada ciclo, escolha o **Obstáculo** certo na página.

Cada ciclo tem 33 rajadas (15 de bloqueado e 18 de Aproximar) e leva ~25 min. **Troque as pilhas depois do segundo obstáculo**, ou
antes disso se o carrinho ficar mais lento ou reiniciar.

### 3.1 `bloqueado`: 15 rajadas, com o carrinho parado

> **Mudou em 29/10.** Na parede, o Empurrar gravou só 2 rajadas boas em 13 tentativas. O carrinho
> reiniciou 5 vezes com o motor travado, e 6 rajadas foram descartadas porque o sensor chegou a
> menos de 8 cm: o carrinho passou do batente, ou o sensor errou com o 5 V caindo. A 200, o sensor
> ficou sem eco enquanto o motor forçava. Por isso o bloqueado agora é gravado **parado, sem
> motor**. Para um modelo que só vê a distância, a janela é a mesma: distância parada entre 8 e
> 35 cm.

| Como | Rajadas |
|---|---|
| rótulo **bloqueado** + **Rajada 3 s**, motor desligado, número da tela em **~10, ~20 e ~30 cm**, 5 de cada | 15 |

- No painel, clique na casa **Bloqueado** do obstáculo: ele escolhe o rótulo e o obstáculo.
- Coloque o carrinho com a mão e ajuste pela distância ao vivo. Não precisa de batente.
- Entre as 5 de cada distância, mexa um pouco o carrinho: 1 a 2 cm para a frente ou para trás e um
  pouco de ângulo. Assim o modelo não decora uma posição só.
- Na cadeira, confira na distância ao vivo que o sensor vê a perna da frente.
- As 2 rajadas de Empurrar que já estão boas contam no total da parede.

### 3.2 `aproximando` (sem o batente): 18 rajadas

**Tire o batente:** o Aproximar travaria nele.

| Variar | Valores |
|---|---|
| Velocidade | 150 · 200 · teto (do teste do passo 7 da §2) |
| Ângulo | de frente · 30° |
| Partida | 150, 100 e 150 cm, nas 3 repetições |

3 velocidades × 2 ângulos × 3 repetições = **18 rajadas**. Se o teto for 200, use 150, 180 e 200.

- **A 30°, de olho na distância ao vivo:** numa parede lisa, o eco pode escapar de lado e a leitura
  pular para 200. Se isso acontecer, use 15° nesse obstáculo.
- Se aparecer `rajada curta demais, descartada`, parta de mais longe.
- Rajada boa: a mensagem mostra `rajada N (aproximando, ...)` com **5 janelas ou mais**.

**Conta depois do primeiro obstáculo.** Divida as janelas de `aproximando` da tabela por 18. Se der
menos de 8 por rajada, as 72 rajadas não chegam a 600 janelas. Nesse caso, nos obstáculos
seguintes, acrescente uma 4ª repetição partindo de 150 cm nas duas velocidades mais lentas.

### 3.3 Conferência depois do primeiro obstáculo

Cada rajada já chega no painel com `✓ forma ok` ou `⚠` e o motivo. Depois do primeiro obstáculo,
aperte também **Conferir dados**. Os volumes ainda vão falhar, o que é normal nesta hora. Importam
as formas:

| Linha do checklist | Se falhar |
|---|---|
| `aproximando`: ≥ 80% das janelas descendo | provavelmente é o eco a 30°: use 15° |
| `bloqueado`: ≥ 85% paradas entre 8 e 35 cm | batente perto demais (zona cega) ou o sensor vendo outra coisa: afaste o batente e confira a distância ao vivo |

Se algo falhou, desfaça as rajadas ruins e corrija antes de seguir para o próximo obstáculo.

---

## 4. Refazer o `livre`: 53 rajadas

Depois dos 4 obstáculos, ou antes deles se preferir, com o sensor conferido na mão.

**Livre andando: 31 rajadas.** O mesmo que você fez em 28/10: ~10 rajadas em cada velocidade (150,
200 e 255), espalhadas entre o corredor e as salas. Agora a página tem que mostrar distâncias reais
durante o percurso, e não 200 o tempo todo.

**Rótulo `livre` + Rajada 3 s: 22 rajadas.**

| Situação | Como | Rajadas |
|---|---|---|
| Parado | carrinho parado, nada a menos de 2 m | 8 |
| Girando | segure ◀ ou ▶ desde a contagem, metade para cada lado | 8 |
| Ré | partindo a 30 cm de um obstáculo, segure ▼ só quando o LED ficar vermelho | 6 |

Sem estes casos, o modelo nunca viu o carrinho parado ou girando.

---

## 5. Se o carrinho reiniciar

A rajada em andamento se perde; as outras voltam sozinhas da flash. Leia o motivo na página:

| O que aparece | O que fazer |
|---|---|
| 1 reinício | refaça a rajada perdida e siga |
| "queda de tensão" de novo | troque as pilhas |
| "energia cortada" com o carrinho andando | mau contato: refaça o teste do passo 5 da §2 |
| `O sensor não devolve eco há N s` no painel | mão a 20 cm do sensor; se continuar sem eco, pare e volte ao passo 2 da §2 |
| reinícios seguidos no Aproximar | pilhas novas. Se continuar, alimente o ESP32 por um power bank no USB durante a coleta, soltando do pino 5V o fio do diodo, como no USB do computador: as pilhas ficam só para os motores |
| reinício no Aproximar sempre na velocidade mais alta | tire essa velocidade |

Se o painel mostrar `Sem conexão com o carrinho` por mais de alguns segundos, aperte **Conectar na
CARRINHO-COLETA** no aviso. Não é urgente: a flash tem tudo, e o painel copia quando voltar.

---

## 6. Fechamento

1. Confira no topo do painel: `no computador: N · no carrinho: M · tudo copiado`.
2. Aperte **Conferir dados**. O painel roda o `preparar.js` com `dados/coleta/coleta_detalhado.csv`
   e mostra o checklist.
3. Com tudo em OK, `dados/coleta/treino.csv` e `dados/coleta/teste.csv` vão para o Edge Impulse.

> O CSV detalhado da página do carrinho é só reserva, para o caso de o painel falhar. Nunca junte o
> da página com o do painel no `preparar.js`: as mesmas rajadas entrariam duas vezes, e cópias da
> mesma rajada podiam cair uma no treino e outra no teste. O acerto medido sairia falso.

Checklist todo em OK: siga para o [plano de treino](plano-treino.md). Se algum critério falhar, o
próprio checklist diz qual classe ou variação gravar a mais.

---

## 7. Folha de acompanhamento

| Bloco | Rajadas | ✓ |
|---|---|---|
| Preparação (§2): sensor medindo, firmware novo, painel aberto, testes de parada e de energia | 3 + 3 | |
| Parede: 15 bloqueado parado · 18 Aproximar | 33 | |
| Conferência com o `preparar.js` (§3.3) | | |
| Caixa | 33 | |
| **Troca de pilhas** | | |
| Cadeira | 33 | |
| Canto | 33 | |
| Livre: 31 Livre andando · 8 parado · 8 girando · 6 ré | 53 | |
| Fechamento (§6) | | |
