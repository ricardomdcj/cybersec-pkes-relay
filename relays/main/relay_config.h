#pragma once

/*
 * Um unico projeto compila os dois relays; o papel e escolhido por um define.
 *
 *   RELAY_ROLE_A -> central, fica perto do celular (a chave)
 *   RELAY_ROLE_B -> peripheral, fica perto do veiculo
 *
 * Troque RELAY_ROLE abaixo antes de compilar cada placa. Como A e B rodam em
 * chips diferentes (A = ESP32-S3, B = ESP32-C3), lembre de casar o set-target:
 *   Relay A:  idf.py set-target esp32s3  (RELAY_ROLE_A)
 *   Relay B:  idf.py set-target esp32c3  (RELAY_ROLE_B)
 *
 * Alternativa sem editar o header: idf.py build -DRELAY_ROLE=1  (ou =2).
 */

#define RELAY_ROLE_A 1
#define RELAY_ROLE_B 2

#ifndef RELAY_ROLE
#define RELAY_ROLE RELAY_ROLE_A
#endif

/* Autoteste do enlace: A e B trocam frames sem BLE, so para validar o tunel
 * ESP-NOW e medir seu overhead. Deixe 1 no esboco; volte para 0 quando o BLE entrar. */
#define RELAY_LINK_SELFTEST 1
