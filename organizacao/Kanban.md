# Kanban – Projeto Robô Móvel

## Colunas do quadro

1. Backlog
2. A Fazer
3. Em Andamento
4. Em Teste
5. Em Correção
6. Concluído

## BACKLOG

| ID | Tarefa |
|---|---|
| T27 | Implementar e testar fail-safe do Bluetooth |
| T28 | Calibrar sensor HC-SR04 |
| T31 | Organizar cabos |
| T32 | Testar autonomia |
| T35 | Documentar circuito |
| T36 | Documentar software |
| T37 | Documentar montagem |
| T38 | Preparar apresentação |

## A FAZER

| ID | Tarefa |
|---|---|
| T01 | Validar componentes |
| T02 | Definir circuito definitivo |
| T03 | Definir layout físico |
| T04 | Construir base |
| T11 | Montar alimentação |

## EM ANDAMENTO

| Tarefa | Responsável |
|---|---|
| Planejamento do circuito | Integrante B |
| Planejamento da estrutura | Integrante C |
| Estrutura inicial do software Arduino | Integrante A |
| Plano de testes | Integrante D |

## EM TESTE

Inicialmente vazio. Conforme o desenvolvimento avançar, entram nesta coluna:

- Teste do motor esquerdo
- Teste do motor direito
- Teste do HC-SR04
- Teste do Bluetooth
- Teste de parada
- Teste integrado

## EM CORREÇÃO

Utilizada quando uma funcionalidade falha no teste.

Exemplos:
- Motor girando invertido
- Sensor instável
- Falha de comunicação Bluetooth
- Problema de alimentação
- Cabo interferindo na roda
- Comando de parada não respondendo corretamente

Depois da correção, a tarefa retorna para **Em Teste**.

## CONCLUÍDO

No início do projeto podem ser considerados concluídos:

- Requisitos definidos
- Arquitetura inicial definida
- Componentes dimensionados
- MVP definido
- Backlog criado

## Fluxo

```text
BACKLOG
   ↓
A FAZER
   ↓
EM ANDAMENTO
   ↓
EM TESTE ────────────┐
   ↓                  │
CONCLUÍDO       EM CORREÇÃO
                      │
                      └──→ EM TESTE
```

## Limite de WIP

| Coluna | Limite |
|---|---:|
| A Fazer | 6 |
| Em Andamento | 4 |
| Em Teste | 3 |
| Em Correção | 3 |
| Concluído | Sem limite |

Regra principal: cada integrante deve priorizar terminar ou desbloquear a atividade atual antes de iniciar uma nova.

## Organização sugerida em sprints

| Sprint | Objetivo | Resultado |
|---|---|---|
| Sprint 1 | Estrutura + eletrônica básica | Carrinho montado e motores ligados |
| Sprint 2 | Controle dos motores | Frente, ré, esquerda, direita e parar |
| Sprint 3 | Bluetooth | Carrinho controlado pelo celular |
| Sprint 4 | Sensor ultrassônico | Detecção e parada automática |
| Sprint 5 | Integração | MVP concluído |
| Sprint 6 | Testes e melhorias | Sistema estabilizado |
| Sprint 7 | Acabamento e documentação | Projeto concluído |
