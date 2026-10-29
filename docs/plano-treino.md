# Plano de treino no Edge Impulse

Do `treino.csv` e `teste.csv` até a biblioteca Arduino. Os parâmetros fixados pela professora
continuam **intocados**: Raw data + Classification, **20 ciclos**, **learning rate 0,005**, o resto
no padrão.

Como não se mexe no treino, o acerto sai de duas coisas: o dado (ver o
[plano de coleta](plano-coleta.md)) e uma avaliação que não engane.

---

## 1. Antes de abrir o Edge Impulse

```bash
node dados/preparar.js dados/<sessao>_detalhado.csv [outras sessões...]
```

Só siga com o checklist **todo em OK**. Ele gera `dados/treino.csv` e `dados/teste.csv`, no formato
`d0,...,d9,label`, com os rótulos por nome (`livre`, `aproximando`, `bloqueado`).

> **Por que dois arquivos, e não "split automático":** as janelas se sobrepõem em 80%. No corte
> aleatório do Edge Impulse, quase a mesma janela vai para o treino e para o teste, e a acurácia sai
> alta e mentirosa. O `preparar.js` manda **rajadas inteiras** para o teste, cobrindo as mesmas
> variações de obstáculo e velocidade do treino.

Use `--equilibrar` se o checklist reclamar de balanço. Ele corta janelas só do treino; o teste fica
intacto.

---

## 2. Passo a passo

### 2.1 Projeto

- `studio.edgeimpulse.com` → **Create new project**
- Nome: `tinyml-ultrassonico-2wd-grupoX` (vai no print da entrega)
- Type **Personal** · Setting **Public** (a professora pede o link público)

### 2.2 Upload, em duas vezes

**Data acquisition → Add data → Upload data:**

| Arquivo | Upload into category | Label |
|---|---|---|
| `treino.csv` | **Training** | vem da coluna `label` |
| `teste.csv` | **Testing** | vem da coluna `label` |

Não marque "Automatically split between training and testing".

### 2.3 CSV Wizard

| Tela | Resposta |
|---|---|
| Is this time-series data? | **Não**: cada linha já é uma janela de 1 s pronta |
| Value columns | **d0 … d9**, as dez |
| Label column | **`label`** |

> **Erros que já aconteceram no SMARS:** uma configuração de CSV salva de um upload anterior
> continua valendo (rode o Wizard de novo para sobrescrever); `label` marcado como metadado;
> `d0` usado como rótulo. Este último transforma o problema numa regressão disfarçada, que dá
> números plausíveis e não serve para nada.

**Confira na Data acquisition:** as três classes aparecem, e a proporção entre Training e Testing
fica perto da que o `preparar.js` mostrou (uns 80/20).

### 2.4 Create impulse

- **Add processing block → Raw data**
- **Add learning block → Classification**
- **Save impulse**

Os eixos do Raw data têm que ser **d0 … d9**, e o frame, de 10 valores. Se aparecer `label` como
eixo, o modelo receberia o próprio rótulo como entrada: acurácia ótima e projeto destruído.

### 2.5 Generate features

**Raw data → Generate features.** Antes de treinar, olhe o **Feature explorer**:

- nuvens separadas: pode treinar;
- `aproximando` e `bloqueado` misturados: o problema é a coleta. Volte à §5 deste plano em vez de
  treinar.

### 2.6 Classifier

| Campo | Valor |
|---|---|
| Number of training cycles | **20** |
| Learning rate | **0,005** |
| Demais | padrão |

**Save and train.** Anote a acurácia, o loss e a matriz de confusão. Este número é da
**validação**, tirada aleatoriamente de dentro do treino, então é otimista. Serve como sinal, não
como resultado.

### 2.7 Model testing: o número que vale

**Model testing → Classify all.** Aqui o modelo é avaliado no `teste.csv`, rajadas que ele nunca
viu. Anote a acurácia, a matriz de confusão e o **F1 de cada classe**, e tire o print.

### 2.8 Segunda rodada

Treine de novo, sem mudar nada, e repita o 2.7. É o que separa resultado de sorte na inicialização.

---

## 3. Critérios de aceitação

| Critério | Meta | Por quê |
|---|---|---|
| Acurácia no Model testing | **≥ 92%** | o número em situações novas |
| F1 de cada classe | **≥ 0,90** | a média esconde a classe pequena: no SMARS, 77% de acurácia escondiam F1 de 0,14 |
| Recall de `bloqueado` | **≥ 0,90** | é a classe que justifica o projeto |
| `bloqueado` → `aproximando` | **≤ 5%** | foi a confusão que derrubou a tentativa 1 |
| Diferença entre as duas rodadas | **≤ 2 pontos** | estabilidade |
| Validação × Model testing | diferença **≤ 5 pontos** | se a validação for muito maior, o modelo decorou as rajadas |

Passou em tudo: vá para o deployment (§6).

---

## 4. Registro dos treinos

| Rodada | Data | Dados (arquivos) | Janelas treino / teste | Validação | Model testing | F1 livre / aprox / bloq | Observação |
|---|---|---|---|---|---|---|---|
| 1 | | | | | | | |
| 2 | | | | | | | |

---

## 5. Se não passar: o que fazer

A regra do SMARS continua valendo: **corrija na coleta, não no treino nem filtrando o CSV**.

| Sintoma no Model testing | Causa provável | O que fazer |
|---|---|---|
| `bloqueado` virando `aproximando` | rajadas de Empurrar gravadas antes de estabilizar, ou Aproximar terminando parado | confira as formas no `preparar.js`; regrave Empurrar com o batente bem preso |
| `bloqueado` virando `livre` | zona cega: o sensor chegou a menos de 8 cm | afaste o batente; refaça o teste 1.4 |
| `livre` virando `aproximando` | eco do chão, ou "livre" gravado com obstáculo perto | refaça o teste 1.3 no piso da coleta; regrave Livre em mais espaço |
| `aproximando` virando `livre` | aproximações começando longe demais (sem eco) ou lentas demais | mais rajadas a 200 e 250, partindo de 100 e 60 cm |
| Erro concentrado num obstáculo | pouca variedade daquele obstáculo | mais rajadas dele, nos dois ângulos |
| Validação alta e Model testing baixo | o modelo decorou as rajadas do treino | mais variedade, e não mais repetições da mesma |
| Feature explorer misturado | rótulos inconsistentes | reveja o registro da sessão: alguém mexeu no sensor? mudou o piso? |

Depois de recoletar, rode o `preparar.js` com **todas** as sessões juntas e refaça o upload do zero,
apagando os dados antigos do projeto, para não misturar versões.

---

## 6. Deployment

### 6.1 TARGET (valores fixados pela professora)

| Campo | Valor |
|---|---|
| Target device | Custom |
| Processor Family | **ESP32** |
| Clock rate | **240 MHz** |
| Custom device name | **ESP32_sensor** |
| RAM | **1 MB** |
| ROM (Flash) | **2 MB** |
| Latency | **100 ms** |

O ESP32-S3 também roda a 240 MHz. Esses números servem para a estimativa de desempenho do Studio
e não mudam o modelo.

### 6.2 Biblioteca

**Deployment → Arduino library → Build.** Sai um `.zip`. Mantenha o **EON Compiler** ligado: no
SMARS ele custou ~17 KB de flash, contra ~279 KB do TensorFlow Lite Micro completo.

### 6.3 Na IDE

- **Sketch → Incluir biblioteca → Adicionar biblioteca .ZIP** com o arquivo baixado.
- **Não** tenha a `Chirale_TensorFLowLite` instalada: ela colide com a cópia do TFLM que vem no zip.
- Ferramentas: ESP32S3 Dev Module · Flash Mode **OPI 80MHz** · Flash Size **32MB** ·
  PSRAM **OPI PSRAM**.
- Erro `can't create ...arm_fill_f64.c.o`: o caminho de build é longo demais. Compile com
  `--build-path C:/eib` ou ative caminhos longos no Windows.

### 6.4 Inferência

O sketch de inferência vai usar **a mesma `struct Pipeline`** do `coleta_wifi.ino`, copiada sem
mudanças, e `run_classifier()`, como no `tinyml_inferencia` do SMARS. Ele imprime as três
probabilidades, para o bloco 5 dos testes. Com o `.zip` em mãos, ele é escrito e compilado contra a
biblioteca real.
