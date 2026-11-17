#pragma once

/*
 * Um unico projeto compila os dois relays; o papel e escolhido por um define.
 *
 *   RELAY_ROLE_A -> central, fica perto da chave (key/)
 *   RELAY_ROLE_B -> peripheral, fica perto do veiculo
 *
 * Troque RELAY_ROLE abaixo antes de compilar cada placa. Agora os dois papeis rodam na
 * MESMA CYD (ESP32 classico); as duas placas se distinguem so pelo papel gravado:
 *   Relay A:  idf.py set-target esp32  -DRELAY_ROLE=1
 *   Relay B:  idf.py set-target esp32  -DRELAY_ROLE=2
 *
 * Alternativa sem editar o header: idf.py build -DRELAY_ROLE=1  (ou =2).
 */

#define RELAY_ROLE_A 1
#define RELAY_ROLE_B 2

#ifndef RELAY_ROLE
#define RELAY_ROLE RELAY_ROLE_A
#endif

/* Autoteste do enlace: A e B trocam frames sem BLE, so para validar o tunel
 * ESP-NOW e medir seu overhead (Milestone 1). Com BLE pronto (Milestone 2), ponha 0
 * para rodar a cadeia real celular -> A -> B -> veiculo.
 * Overridable na linha de comando: idf.py build -DRELAY_LINK_SELFTEST=0 */
#ifndef RELAY_LINK_SELFTEST
#define RELAY_LINK_SELFTEST 1
#endif

/* MAC do outro relay (M-06b), no formato {0xAA,0xBB,...}. Cada placa precisa do MAC da
 * OUTRA: o Relay A coloca aqui o MAC do B, e o Relay B o MAC do A. Os MACs saem no log de
 * boot ("MAC desta placa"). Enquanto estiver zerado, o enlace usa broadcast como antes.
 * Broadcast nao tem ACK da camada MAC, entao a perda do tunel nao aparece na contagem. */
#ifndef RELAY_PEER_MAC
#define RELAY_PEER_MAC {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#endif
