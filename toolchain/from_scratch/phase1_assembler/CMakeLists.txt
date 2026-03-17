cmake_minimum_required(VERSION 3.16)
project(rawrxd_phase1_assembler C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Main assembler executable
add_executable(rawrxd_asm
    main.c
    asm_lexer.c
    asm_parser.c
    x64_encoder.c
    coff_writer.c
)

# Encoder test utility
add_executable(rawrxd_enc_test
    enc_test.c
    x64_encoder.c
)
