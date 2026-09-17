# CAD e fabricação

## Origem do chassi

O chassi é baseado no projeto open-source **SMARS** (Screwless Modular Assemblable Robotic System), de
Kevin Thomas, publicado no Thingiverse sob licença Creative Commons. Escolhemos o SMARS porque o conjunto
já prevê o berço dos motores N20, o compartimento da bateria 9 V e a capa do sensor HC-SR04 — exatamente
os componentes definidos no nosso planejamento. Todas as peças foram fatiadas e impressas pela equipe;
as **rodas foram redesenhadas** pela equipe (ver abaixo), pois a proposta original com esteiras foi descartada.

## Arquivos

| Arquivo | O que é |
|---|---|
| `stl/robo_arduino.3mf` | Projeto de fatiamento completo (Bambu Studio 2.3, perfil Sovol SV07, camada 0,2 mm) com **9 placas**: esteiras, chassi, rodas originais, suporte da bateria 9 V, suporte de sensor IR, conector, capa do HC-SR04 (3 peças) |
| `stl/rodas/roda_motor_canal.stl` | **Versão final** – roda motora Ø33 mm com canal para elástico, autossustentável |
| `stl/rodas/roda_livre_canal.stl` | Versão final – roda livre Ø32 mm com canal |
| `stl/rodas/roda_motor_lisa.stl`, `roda_livre_lisa.stl` | v1/v3 – rodas lisas |
| `stl/rodas/roda_motor_ranhurada.stl`, `roda_livre_ranhurada.stl` | v2 – 24 ranhuras transversais |
| `scripts/remodel.js` | Script (Node.js, sem dependências) que gera as rodas a partir da roda original do SMARS: remove os dentes de engate da esteira e reconstrói a banda de rodagem (lisa / ranhurada / com canal) preservando o encaixe interno do motor e do eixo |
| `scripts/manifold_check.js` | Verificador de malha (arestas abertas / não-manifold) usado para validar cada STL antes de imprimir |
| `scripts/stl_render.js`, `scripts/split_wheels.js` | Render PNG dos STLs e separação das rodas em arquivos individuais |

Placas de impressão (miniaturas geradas pelo fatiador): `../img/fabricacao/placa-1.png` … `placa-9.png`.

## Evolução das rodas

| Versão | Arquivo | Diâmetro motora / livre | Resultado |
|---|---|---|---|
| v0 – original SMARS | no 3MF (`master_wheel`, `slave_wheel_SL`) | Ø32,7 com 8 dentes | Só funciona com esteira. A esteira travava o motor → descartada |
| v1 – lisa | `roda_*_lisa.stl` | Ø32 / Ø32 | Roda livre, mas PLA liso patina no piso |
| v2 – ranhurada | `roda_*_ranhurada.stl` | Ø32 / Ø32 | 24 ranhuras; ainda girava em falso: com diâmetros iguais as motoras ficavam sem contato com o chão |
| v3 – motora maior | (aplicado a v1 e v2; arquivos do repo já estão assim) | **Ø33 / Ø32** | Motora 0,5 mm mais alta garante contato sempre; faltava atrito |
| **v4 – canal para elástico** | `roda_*_canal.stl` | Ø33 / Ø32 (+ elástico) | **Final.** Elástico de dinheiro (2 voltas) nas motoras; tração resolvida |

Detalhes de cada decisão estão na linha do tempo do README principal.

## Como regenerar as rodas

```bash
node scripts/remodel.js roda.stl rodas_canal.stl canal        # variantes: (vazio)=lisa | ranhura | canal
node scripts/split_wheels.js rodas_canal.stl roda_motor_canal.stl roda_livre_canal.stl
node scripts/manifold_check.js roda_motor_canal.stl
```

`roda.stl` é a exportação das 4 rodas originais do 3MF (placa 3). Parâmetros (diâmetros, profundidade do
canal, número de ranhuras) estão no topo de `remodel.js`.

## Impressão

- Impressora Sovol SV07, bico 0,4 mm, camada 0,2 mm, PLA.
- Rodas: imprimir **deitadas, como o arquivo abre** (cavidade do motor para baixo). O canal do elástico tem a
  parede superior em rampa de 45° justamente para não precisar de suporte nessa orientação.
- Elástico: só nas rodas motoras. As rodas livres precisam deslizar de lado para o carrinho girar no lugar.
