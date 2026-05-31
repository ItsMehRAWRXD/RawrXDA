cmake_minimum_required(VERSION 3.20)

project(tokenizer_roundtrip_test LANGUAGES CXX)

add_executable(tokenizer_roundtrip_test
    tokenizer_roundtrip_test.cpp
)

target_include_directories(tokenizer_roundtrip_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../tokenizer
    ${CMAKE_CURRENT_LIST_DIR}/../../core
)

target_sources(tokenizer_roundtrip_test PRIVATE
    ../../tokenizer/tokenizer_base.cpp
    ../../tokenizer/bpe_tokenizer.cpp
    ../../tokenizer/sentencepiece_tokenizer.cpp
    ../../tokenizer/sentencepiece_protobuf.cpp
    ../../tokenizer/huggingface_tokenizer.cpp
    ../../tokenizer/tokenizer_factory.cpp
    ../../streaming_gguf_loader.cpp
    ../../gguf_loader.cpp
    ../../gguf_vocab_resolver.cpp
    ../../compression_interface.cpp
    ../../utils/Diagnostics.cpp
    codec_stubs.cpp
)

if(MSVC)
    target_compile_options(tokenizer_roundtrip_test PRIVATE /W4 /utf-8)
endif()

