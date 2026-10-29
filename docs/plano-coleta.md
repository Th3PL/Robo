# Plano de coleta

Como gravar o dado que o modelo vai aprender. Pressupõe os blocos 0 a 3 do
[plano de testes](plano-testes.md) passando.

**Tempo:** ~1 h 30, das quais uns 20 min são de gravação de verdade. Duas pessoas é melhor: uma
posiciona o carrinho e os obstáculos, a outra opera a página.

---

## 1. Metas

| | Meta |
|---|---|
| Janelas por classe (total) | **≥ 600**, para sobrarem ≥ 400 no treino depois da separação |
| Rajadas por classe | 60 a 80 |
| Equilíbrio | a maior classe com no máximo 1,5× a menor |
| Variedade | todas as células da matriz da §4 com pelo menos 2 rajadas |
| Saída | `preparar.js` com o checklist inteiro em OK |

---

## 2. Antes de começar

**Material**

- Obstáculos: parede lisa, caixa de papelão, cadeira (as pernas), um canto de 90° e um objeto mole
  (mochila ou almofada), que entra como "outro".
- **Batente:** uma régua de madeira ou um cabo de vassoura deitado no chão (até 2,5 cm de altura),
  preso com fita crepe. Ele trava a roda da frente e mantém o sensor longe da zona cega.
- Fita crepe para marcar no chão as distâncias de partida (150, 100 e 60 cm) e o ângulo de 30°.
- Trena, pilhas novas e um jogo reserva.
- Notebook na rede **CARRINHO-COLETA** (senha `carrinho123`) com o **painel de coleta** aberto.
  O celular, na página **http://192.168.4.1**, é opcional: serve de controle remoto perto do
  carrinho.

**Como posicionar o batente:** encoste o carrinho no batente e meça da frente do sensor até o
obstáculo. Ajuste até dar 10, 15 ou 20 cm e marque no chão as três posições. Isso só é feito uma
vez por obstáculo.

**Ambiente:** de preferência o mesmo tipo de piso da apresentação. Anote o piso no registro da
sessão (§7).

**Sensor:** na posição final, com a altura e a inclinação anotadas no bloco 1 dos testes. Se
alguém mexer no sensor no meio da coleta, comece uma nova sessão e anote.

---

## 3. O que deu errado no SMARS e a regra que evita

| Na tentativa 1 | Regra aqui |
|---|---|
| Tomadas de 60 a 146 s: só 28% das janelas de `aproximando` estavam descendo | os roteiros gravam **só o fenômeno** e param sozinhos |
| `bloqueado` gravado dentro da zona cega: a leitura virava 200 cm, igual a `livre` | **batente** a 10–20 cm + pipeline segurando a última leitura perto + o roteiro descarta a rajada se passar de 8 cm |
| `livre` com leituras médias misturadas | o roteiro Livre só grava com mais de 1 m livre e corta o fim se algo aparecer |
| Coleta empurrando com a mão, sem a vibração dos motores | os roteiros **andam com os motores** |
| Pouca variedade | a matriz da §4, acompanhada na tabela da página |
| Erro descoberto só depois do treino | `preparar.js` ao fim de cada bloco de rajadas |
| Teste no mesmo conjunto do treino | separação por rajada (ver o [plano de treino](plano-treino.md)) |

---

## 4. A matriz de coleta

> Depois do livre, siga o [plano de aproximando e bloqueado](plano-aproximando-bloqueado.md): ele
> ajusta esta matriz ao que a sessão do livre mostrou (reinícios e Empurrar só a 150).

Antes de cada rajada, escolha na página o **Obstáculo** e a **Velocidade** certos: eles vão para o
CSV detalhado e são o que permite separar o teste de forma justa.

### `aproximando` · roteiro **Aproximar** · 72 rajadas

O carrinho anda reto, grava e para sozinho com o sensor a 15 cm.

| Variar | Valores |
|---|---|
| Obstáculo | parede · caixa · cadeira · canto |
| Velocidade | 150 · 200 · 250 |
| Ângulo | de frente · 30° |
| Partida | 150 · 100 cm, alternando a cada repetição; 60 cm só na velocidade 150 |

4 obstáculos × 3 velocidades × 2 ângulos = 24 combinações × 3 repetições = **72 rajadas** (~10
janelas cada).

> A aproximação precisa durar mais de 1 s para render janela. Partindo de 60 cm em velocidade alta
> ela acaba antes disso, e a página avisa `rajada curta demais, descartada`: parta de mais longe.

Extra opcional, para variedade: 8 rajadas com o **carrinho parado** e alguém empurrando a caixa
devagar em direção ao sensor (rótulo `aproximando` + **Rajada 3 s**).

### `bloqueado` · roteiro **Empurrar** + rajadas paradas · 60 rajadas

O Empurrar anda até travar no batente, espera a distância estabilizar e grava 3 s com o motor
forçando.

| Variar | Valores |
|---|---|
| Obstáculo | parede · caixa · cadeira · canto |
| Batente | 10 · 15 · 20 cm |
| Velocidade | 150 · 200 |

4 × 3 × 2 = 24 combinações × 2 repetições = **48 rajadas**.

Mais **12 rajadas paradas**: carrinho parado, sem motor, a 10, 20 e 30 cm de cada obstáculo.
Rótulo `bloqueado`, botão **Rajada 3 s**.

> Na cadeira, o batente fica entre as pernas e o carrinho, e o sensor deve "ver" a perna da frente.
> Confira a distância ao vivo na página antes de gravar.

### `livre` · roteiro **Livre andando** + rajadas manuais · 70 rajadas

| Situação | Como | Rajadas |
|---|---|---|
| Andando em espaço aberto | **Livre andando**: 3 velocidades × 4 lugares (corredor, sala apontando para a porta aberta, sala com parede a mais de 2 m, área aberta) × 4 | 48 |
| Parado em espaço aberto | rótulo `livre` + **Rajada 3 s** | 8 |
| Girando em espaço aberto | rótulo `livre` + **Rajada 3 s**, segurando ◀ ou ▶ desde a contagem | 8 |
| Ré se afastando de um obstáculo | partindo a 30 cm, rótulo `livre` + **Rajada 3 s**, segurando ▼ só quando o LED ficar vermelho | 6 |

Os casos parado, girando e ré existem porque o robô vai passar por eles de verdade. Sem eles, o
modelo nunca viu essas janelas e decide por acaso.

---

## 5. Procedimento

O passo a passo de cada botão e as mensagens da página estão no
[tutorial da página](tutorial-pagina-coleta.md).

1. **Ligar e conferir.** Página aberta, distância ao vivo acompanhando a mão, LED verde.
2. **Ensaio** (se não fez no bloco 3 dos testes): 2 rajadas de cada roteiro, baixe o
   **CSV detalhado** e rode `node dados/preparar.js <arquivo>`. As formas têm que passar.
3. **Para cada linha da matriz:**
   1. escolha Obstáculo e Velocidade na página;
   2. posicione o carrinho na marca do chão;
   3. aperte o roteiro e **saia da frente do sensor** nos 2 s de contagem (LED amarelo piscando);
   4. espere o fim (LED vermelho enquanto grava, verde quando termina);
   5. leia a mensagem, por exemplo `rajada 23 (aproximando, caixa): 3.4 s, 11 janelas`.
4. **Algo saiu errado?** Bateu torto, alguém passou na frente, o carrinho desviou: aperte
   **Desfazer última rajada** na hora. Uma rajada a menos custa menos do que uma errada.
5. **Grave pelo painel do computador** (`Abrir painel de coleta.bat`, ver o
   [tutorial, §2.1](tutorial-pagina-coleta.md#21-o-painel-no-computador-recomendado)): cada rajada
   é copiada para o disco, e o que faltar o painel busca na flash do carrinho. Sem o notebook, baixe
   o CSV detalhado da página a cada ~20 rajadas.
6. **Pilhas:** se o carrinho ficar visivelmente mais lento, troque as pilhas e anote no registro.
   A velocidade real muda com a carga.
7. **No fim:** **Conferir dados** no painel, que roda o `preparar.js` com
   `dados/coleta/coleta_detalhado.csv`, e leia o checklist.
   Critério falhando: grave mais da classe ou da variação apontada. O que já existe não se perde.

Use **PARAR TUDO** sempre que o carrinho for para onde não devia. No meio de um roteiro, ele
descarta a rajada em andamento.

---

## 6. Regras de ouro durante a coleta

- **Ninguém na frente do sensor:** a contagem de 2 s existe para isso.
- **Não mexer no sensor.**
- **Desfazer na hora** qualquer rajada duvidosa.
- **Mesmo piso e mesma montagem** em todas as sessões. Se mudar, anote.
- **Baixar o CSV a cada 20 rajadas.**
- **Não gravar transições:** os roteiros já cortam. Nas rajadas manuais, comece com o carrinho já na
  situação.

---

## 7. Registro da sessão

| Campo | Valor |
|---|---|
| Data e local | |
| Piso | |
| Altura e inclinação do sensor | |
| Pilhas (novas? trocadas quando?) | |
| Firmware | `coleta_wifi` (data do arquivo) |
| Arquivos baixados | |
| Rajadas desfeitas e motivo | |
| Observações | |

---

## 8. Arquivos

```
dados/coleta/                 <- o painel cuida desta pasta
  rajadas.jsonl               <- tudo o que chegou, de todas as sessões: GUARDE
  coleta_detalhado.csv        <- entrada do preparar.js, sempre atualizado
  coleta_cru.csv              <- leitura crua: permite reprocessar sem coletar de novo
  treino.csv / teste.csv      <- gerados pelo botão Conferir, vão para o Edge Impulse
```

Nunca edite um CSV à mão. As sessões se juntam sozinhas: o painel acrescenta cada sessão nova ao
mesmo `rajadas.jsonl`. Para tirar uma rajada, use **remover** no painel.

Próximo passo: [plano de treino](plano-treino.md).
