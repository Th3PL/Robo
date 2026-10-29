# Plano de testes

Do sensor montado até o modelo comandando os motores. Siga em ordem: cada bloco assume que o
anterior passou. Cada teste tem o que fazer, o que esperar e o que fazer se falhar. Os resultados
anotados nas tabelas entram no relatório.

Sketches: [`teste_hcsr04`](../software/teste_hcsr04/teste_hcsr04.ino) para os blocos 0 e 1 e
[`coleta_wifi`](../software/coleta_wifi/coleta_wifi.ino) para os blocos 2 e 3. A inferência, nos
blocos 5 a 7, vem depois do treino.

> Multímetro sempre em **V⎓** (tensão) ou **Ω** (resistência), com tudo desligado para medir Ω.
> Nunca em modo corrente encostado nos polos da bateria: isso é um curto.

---

## Bloco 0 · Antes de energizar

| | Teste | Como | Esperado | Se falhar |
|---|---|---|---|---|
| 0.1 | R1 do divisor | Ω entre o pino Echo do sensor e o GPIO2 | ~1 kΩ | refaça o divisor antes de ligar |
| 0.2 | R2 do divisor | Ω entre o GPIO2 e o GND | ~1,5 kΩ (pode dar um pouco menos com o sensor conectado) | idem |
| 0.3 | GND comum | continuidade entre o − do pack e os GND da ponte, do ESP32 e do sensor | bipa nos três | sem isso nada funciona |
| 0.4 | Pack | V⎓ no pack, chave desligada | 4×AA novas: 6,2 a 6,5 V | abaixo de 5,6 V, troque as pilhas antes da coleta |

---

## Bloco 1 · Sensor na posição final

> O sensor tem que estar **fixo onde vai ficar no robô final**. Mudar a altura ou a inclinação
> depois da coleta muda o dado, e o modelo treinado deixa de valer.

| | Teste | Como | Esperado | Se falhar |
|---|---|---|---|---|
| 1.1 | Montagem | meça a altura do centro do sensor ao chão e a inclinação | anote os dois (tabela abaixo) | — |
| 1.2 | Precisão | `teste_hcsr04`, parede a 10, 15, 20, 30, 50, 100 e 150 cm medidos com trena | erro ≤ 2 cm até 100 cm | conferir divisor, VCC em 5 V e se o sensor está reto |
| 1.3 | **Eco do chão** | carrinho no chão, no meio da sala, nada a menos de 2 m à frente, 30 s | ≥ 95% das leituras em "sem eco" ou ≥ 150 cm | leitura fixa entre 20 e 60 cm é o chão: incline o sensor 5° a 10° para cima e repita. Faça em cada piso que for usar |
| 1.4 | Zona cega | aproxime uma parede devagar, de 15 cm até encostar | anote a distância em que começa "sem eco" ou leitura absurda (esperado: 3 a 5 cm) | se começar acima de 8 cm, aumente `ZONA_CEGA_CM` e afaste o batente |
| 1.5 | Caracterização (entrega da Aula 19) | parede a 10, 40 e 100 cm e apontado para o vazio, 30 s parado cada. Na página do `coleta_wifi`: **Gravar**, 30 s, **Parar gravação**, e baixe o **Cru** | tabela abaixo preenchida | — |

**Registro do bloco 1**

| Item | Valor |
|---|---|
| Altura do centro do sensor ao chão | ____ cm |
| Inclinação | ____ ° |
| Erro máximo até 100 cm (1.2) | ____ cm |
| Leituras ≥ 150 cm ou sem eco no chão livre (1.3) | ____ % |
| Início da zona cega (1.4) | ____ cm |
| Mínimo / máximo observado (1.5) | ____ / ____ cm |
| Desvio padrão parado a 40 cm (1.5) | ____ cm |
| Salto médio entre amostras (1.5) | ____ cm |
| Leituras sem eco em 30 s (1.5) | ____ |

Os números do 1.5 respondem a "por que vocês filtraram?" na apresentação.

---

## Bloco 2 · Motores, energia e Wi-Fi

Sketch: `coleta_wifi`. Celular na rede **CARRINHO-COLETA** (senha `carrinho123`), página em
**http://192.168.4.1**.

| | Teste | Como | Esperado | Se falhar |
|---|---|---|---|---|
| 2.1 | Página | abra a página e passe a mão na frente do sensor | a distância acompanha a mão | confira se o celular está na rede do carrinho e não no Wi-Fi de casa |
| 2.2 | Dirigir | rodas no ar, segure ▲ ▼ ◀ ▶ | obedece enquanto segura e para ao soltar | — |
| 2.3 | Fail-safe | segurando ▲, desligue o Wi-Fi do celular | os motores param em menos de 1 s | não colete sem isso funcionar |
| 2.4 | Velocidade real | no chão, cronometre 1 m em linha reta com velocidade 150, 200 e 250 | anote cm/s (tabela abaixo) | se não andar a 150, a velocidade mínima da matriz sobe para a menor que anda |
| 2.5 | Parada do Aproximar | roteiro **Aproximar** contra uma parede a partir de 150 cm, nas três velocidades | para com o sensor a **10 cm ou mais** da parede | parou mais perto: diminua a velocidade máxima ou suba `PARADA_CM` |
| 2.6 | Energia | 5 roteiros **Empurrar** seguidos contra o batente | o ESP32 não reinicia (a página não cai e o LED não apaga) | pilhas novas ou NiMH; velocidade 150 no Empurrar |
| 2.7 | Ruído dos motores | rodas no ar, parede a 40 cm: 30 s de **Gravar** com motor parado e 30 s com ▲ segurado | desvio padrão parecido nos dois (diferença ≤ 1 cm) | se o ruído subir muito: fios do sensor longe dos fios dos motores e capacitor de 100 nF nos motores |

**Registro do bloco 2**

| Velocidade | cm/s no chão | Parada do Aproximar (cm do sensor à parede) |
|---|---|---|
| 150 | | |
| 200 | | |
| 250 | | |

---

## Bloco 3 · Cadeia de dados

| | Teste | Como | Esperado |
|---|---|---|---|
| 3.1 | Cadência | **Gravar** por 60 s cronometrados, baixe o **Cru** e conte as linhas | 600 ± 2 |
| 3.2 | Ensaio | 2 rajadas de cada roteiro (Aproximar, Empurrar, Livre) e baixe o **CSV detalhado** | o arquivo abre e tem as colunas `rajada,tipo,obstaculo,vel,d0..d9,label` |
| 3.3 | Formas | `node dados/preparar.js <arquivo>` | os critérios de forma passam: `aproximando` descendo, `bloqueado` parado entre 8 e 35 cm, `livre` sem aproximação. Os de volume ainda falham, é esperado |
| 3.4 | Desfazer | grave uma rajada, aperte **Desfazer última rajada** | a contagem volta ao valor anterior |

> **Ponto de parada:** com 3.3 passando, a coleta completa pode começar.

---

## Bloco 4 · Modelo no Edge Impulse

Os critérios estão em [`plano-treino.md`](plano-treino.md#3-critérios-de-aceitação). Resumo:
Model testing **≥ 92%**, F1 **≥ 0,90** em cada classe, `bloqueado` → `aproximando` **≤ 5%** e duas
rodadas de treino com diferença **≤ 2 pontos**.

---

## Bloco 5 · Inferência na bancada

Carrinho apoiado num copo, rodas no ar, Monitor Serial com as três probabilidades.

| | Teste | Como | Esperado | Repetições |
|---|---|---|---|---|
| 5.1 | Livre | nada à frente | `livre` | 10 |
| 5.2 | Aproximando | mão ou caixa vindo devagar de 100 cm até 20 cm em ~2 s | `aproximando` durante o movimento | 10 |
| 5.3 | **O teste que importa** | pare a mão a 15 cm e **deixe parada** | vira `bloqueado` em até 1,5 s, na **mesma distância** em que antes era `aproximando` | 10 |
| 5.4 | Retirada | tire a mão | volta a `livre` | 10 |

Nesta etapa não se treina modelo novo: se falhar, anote o cenário e volte à coleta.

---

## Bloco 6 · Inferência no chão (o modelo ainda não comanda os motores)

Dirija pela página do celular e registre a classe que o modelo dá em cada cenário.

| Cenário | Classe esperada | Acertos (de 20) |
|---|---|---|
| Aproximação frontal: parede | `aproximando` | |
| Aproximação frontal: caixa | `aproximando` | |
| Aproximação frontal: cadeira | `aproximando` | |
| Aproximação em ângulo de 30° | `aproximando` | |
| Empurrando contra o batente | `bloqueado` | |
| Parado a 15 cm de um obstáculo | `bloqueado` | |
| Andando em espaço aberto | `livre` | |
| Girando em espaço aberto | `livre` | |
| Ré se afastando de um obstáculo | `livre` | |
| Andando num corredor, paralelo a uma parede | `livre` | |

Meta: **≥ 18 de 20** em cada linha, e `bloqueado` reconhecido em até 1,5 s depois de travar.

---

## Bloco 7 · Modelo comandando os motores

Política: `livre` anda em velocidade de cruzeiro, `aproximando` reduz a velocidade, `bloqueado`
manobra (gira até ficar livre; se continuar bloqueado, ré em arco e gira para o outro lado). Só
troque de ação com confiança > 0,60 e com 2 inferências seguidas iguais. Se o modelo falhar, ou
ficar com confiança baixa por mais de 1 s, o robô para.

| | Teste | Esperado |
|---|---|---|
| 7.1 | 3 min numa sala com obstáculos variados | nenhum travamento de mais de 3 s |
| 7.2 | 10 aproximações frontais em velocidade de cruzeiro | nenhuma batida forte: freia antes |
| 7.3 | Canto de 90° | sai sozinho em até 10 s, sem entrar no vai-e-vem |

Grave o vídeo da entrega aqui.
