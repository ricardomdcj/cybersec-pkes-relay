# Sessoes fora dos dados

## Invalidas (campanha rotulada como relayed, mas o carro conectou direto na chave)
Nao ha linhas CSVA/CSVB, o RTT ficou em ~89 ms (igual ao baseline direto) e a chave
registrou 50 HOP K sem passar pelos relays.

- 20261118-205109_relay-unicast-session
- 20261118-205332_relay-unicast-tx8-session
- 20261118-205917_relay-unicast-tx8-session

Causa: o Relay A registrou "connected to key", mas a chave nao confirmou essa conexao
(o link do lado do relay nao existia). O carro entao conectou direto na chave quando
ela ficou livre. O bring-up foi corrigido: a chave precisa registrar "vehicle connected"
perto do momento em que o Relay A conecta; senao o Relay A e reiniciado.

## Abortadas (sem campanha)
- aborted/20261118-195201_relay-session (Relay A nao conectou em 60 s)
- aborted/20261118-195355_relay-session (pulso de reset falhou: COM5 ocupada)
- aborted/20261118-205713_relay-unicast-session (falso aborto da checagem de estabilidade)
