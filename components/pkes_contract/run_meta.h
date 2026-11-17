#pragma once

/*
 * Cabecalho de execucao (M-13): imprime a linha RUN_META no boot com o commit git da
 * fonte, o estado (+mod se houver mudancas nao commitadas) e a data do configure.
 * Serve para ligar cada captura ao codigo que a gerou (o manifesto da campanha, D-03,
 * le esta linha).
 *
 * O commit e lido pelo CMake na hora do configure. Depois de commitar, rode
 * `idf.py reconfigure` antes do build, senao a linha mostra o commit anterior.
 */
void run_meta_print(const char *papel);
