#pragma once
#include <QByteArray>

// Deflate (compress) using MASM-accelerated brutal algorithm
QByteArray deflate_brutal_masm(const QByteArray& data);

// Inflate (decompress) using MASM-accelerated brutal algorithm
QByteArray inflate_brutal_masm(const QByteArray& data);
