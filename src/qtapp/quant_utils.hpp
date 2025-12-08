#pragma once
#include <QByteArray>
#include <QVector>
#include <QString>
#include <QPair>

// Quantization helpers used by inference engine and tests
QByteArray quantize_q8k(const QByteArray& raw);
QByteArray quantize_q4_0(const QByteArray& raw);
QByteArray quantize_generic_bits(const QByteArray& raw, int bits);
QByteArray to_f16(const QByteArray& raw);
QByteArray apply_quant(const QByteArray& raw, const QString& mode);

// New function: returns both quantized data and GGML type ID
QPair<QByteArray, int> apply_quant_with_type(const QByteArray& raw, const QString& mode);

// Unpacking helpers for tests
QVector<float> unpack_generic_bits(const QByteArray& packed, int bits);
QVector<float> unpack_f16(const QByteArray& packed);
