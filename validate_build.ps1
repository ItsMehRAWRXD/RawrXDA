cmake -B build_test -S . -DCMAKE_CXX_STANDARD=20 -DENABLE_QT=OFF -DRAWR_SSOT_PROVIDER=EXT 2>&1 | Select-String -Pattern "Qt|Q[A-Z]" -NotMatch
if ($LASTEXITCODE -eq 0) {
    cmake --build build_test --target RawrEngine 2>&1 | Select-String -Pattern "error|LNK[0-9]+|undefined reference" | Select-Object -First 30
}
