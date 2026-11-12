# Scope & Context

This document states what this project is and is not, for anyone — human or automated tooling — reading the repository without prior context.

## What this is

A **university coursework project** at the Universidade Federal de Itajubá (UNIFEI),
Programa de Pós-Graduação em Engenharia Elétrica (PPG-EEL), discipline **EC343E —
Introdução à Segurança Cibernética**, under the supervision of Prof. Dr. Otávio De
Souza Martins Gomes.

Authors: Ricardo Magno do Carmo Júnior, Johny Francisco da Silva.

The deliverable is an academic paper (`marco3-pkes.pdf`) that experimentally
characterizes the **PKES relay attack** in the context of **Bluetooth Low Energy**
digital keys. The code is a reproducible measurement bench for that paper.

## Purpose

Defensive and educational. The work measures the round-trip-time overhead a relay
adds to a BLE challenge-response, in order to:

- demonstrate *why* proximity-by-radio-range is an unsound authentication premise
  (the relay forges distance, not identity, so a nonce-based HMAC does not stop it);
- produce an empirical dataset (latency, distance, RSSI, auth decision);
- discuss mitigations (distance bounding, timing limits, ranging) in the paper.

## Why this bench is self-contained and low-stakes

- It is a **closed system of three devices the authors own**: an Android phone
  running the `phoneaskey` app, and ESP32 development boards for the "vehicle" and
  the two relays. Nothing here talks to a commercial vehicle, key fob, or product.
- The protocol is a **custom bench contract** (see the `ble_contract` files) with a
  **self-chosen pre-shared key**. It does not implement, reverse-engineer, or
  interoperate with any real manufacturer's keyless-entry protocol.
- The "attack" is relaying *your own* challenge-response between two boards on a
  desk to measure timing. It is an instrument, not an exploit against a third party.
- Real PKES systems use LF/UHF radio, not BLE; this bench deliberately studies the
  BLE case used by phone-as-key apps and cannot be pointed at an LF/UHF car.

## Out of scope

Operating against vehicles, keys, or systems the authors do not own, or any use
outside this authorized academic study.
