# Backlog do Projeto – Robô Móvel

## Papéis considerados
Como os nomes da equipe não foram informados, foram definidos quatro responsáveis genéricos:

- **Integrante A:** Software / Arduino
- **Integrante B:** Eletrônica
- **Integrante C:** Mecânica / Estrutura
- **Integrante D:** Testes / Documentação

## Backlog

| ID | Tarefa | Responsável | Prioridade | Dependência | Critério de aceite |
|---|---|---|---|---|---|
| T01 | Validar componentes disponíveis | Todos | Alta | — | Componentes conferidos |
| T02 | Definir arquitetura elétrica definitiva | B | Alta | T01 | Diagrama final aprovado |
| T03 | Definir posição dos componentes | C | Alta | T01 | Layout físico definido |
| T04 | Construir base do carrinho | C | Alta | T03 | Estrutura montada |
| T05 | Fixar motores | C | Alta | T04 | Motores firmes e alinhados |
| T06 | Fixar rodas | C | Alta | T05 | Rodas girando livremente |
| T07 | Fixar suporte da bateria | C | Alta | T04 | Bateria segura |
| T08 | Fixar Arduino | C | Alta | T04 | Arduino encaixado |
| T09 | Fixar L298N | C | Alta | T04 | Ponte H encaixada |
| T10 | Fixar HC-SR04 | C | Alta | T04 | Sensor voltado para frente |
| T11 | Montar circuito de alimentação | B | Alta | T02 | Tensões conferidas |
| T12 | Conectar L298N aos motores | B | Alta | T05, T11 | Dois motores conectados |
| T13 | Conectar Arduino à L298N | B | Alta | T11 | IN1-IN4/ENA/ENB conectados |
| T14 | Testar Motor 1 | A+B | Alta | T12,T13 | Frente e ré funcionando |
| T15 | Testar Motor 2 | A+B | Alta | T12,T13 | Frente e ré funcionando |
| T16 | Programar função frente() | A | Alta | T14,T15 | Robô avança |
| T17 | Programar função re() | A | Alta | T14,T15 | Robô recua |
| T18 | Programar função esquerda() | A | Alta | T14,T15 | Robô vira à esquerda |
| T19 | Programar função direita() | A | Alta | T14,T15 | Robô vira à direita |
| T20 | Programar função parar() | A | Alta | T14,T15 | Motores param |
| T21 | Conectar HC-SR04 | B | Alta | T11 | Sensor conectado |
| T22 | Programar leitura de distância | A | Alta | T21 | Distância medida corretamente |
| T23 | Implementar parada por obstáculo | A | Alta | T20,T22 | Avanço bloqueado |
| T24 | Conectar HC-05/HC-06 | B | Alta | T11 | Comunicação serial funcionando |
| T25 | Programar leitura Bluetooth | A | Alta | T24 | Arduino recebe comandos |
| T26 | Mapear comandos Bluetooth | A | Alta | T16-T20,T25 | Todos movimentos controláveis |
| T27 | Testar perda de conexão Bluetooth | A+D | Média | T26 | Robô entra em estado seguro |
| T28 | Calibrar distância do HC-SR04 | A+D | Média | T23 | Limite seguro definido |
| T29 | Testar movimentação integrada | D | Alta | T26 | Movimentos funcionando |
| T30 | Testar detecção durante movimento | D | Alta | T23,T26 | Robô para diante de obstáculo |
| T31 | Organizar cabos | B+C | Média | T30 | Nenhum cabo interfere nas rodas |
| T32 | Testar duração da bateria | D | Média | T30 | Autonomia registrada |
| T33 | Testar robô em percurso completo | Todos | Alta | T30,T31 | Percurso concluído |
| T34 | Corrigir bugs encontrados | A+B+C | Alta | T33 | Sem falhas críticas |
| T35 | Documentar circuito | B+D | Média | T34 | Diagrama atualizado |
| T36 | Documentar software | A+D | Média | T34 | Código e comandos descritos |
| T37 | Registrar montagem física | C+D | Média | T34 | Estrutura documentada |
| T38 | Preparar demonstração final | Todos | Alta | T35-T37 | Robô pronto para apresentação |
