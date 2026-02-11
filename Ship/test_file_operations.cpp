#include "RawrXD_FileOperations.hpp"
#include <iostream>

int main() {
    RawrXD::FileManager fm;

    std::wstring path = L"test_file_ops.txt";
    std::wstring content = L"Hello RawrXD FileManager";

    auto writeRes = fm.writeFile(path, content, true);
    if (!writeRes.success) {
        std::wcout << L"writeFile failed: " << writeRes.errorMessage << L"\n";
        return 1;
    }

    std::wstring readContent;
    RawrXD::Encoding enc;
    if (!fm.readFile(path, readContent, &enc)) {
        std::wcout << L"readFile failed\n";
        return 1;
    }

    if (readContent != content) {
        std::wcout << L"content mismatch\n";
        return 1;
    }

    auto renameRes = fm.renameFile(path, L"test_file_ops_renamed.txt");
    if (!renameRes.success) {
        std::wcout << L"renameFile failed: " << renameRes.errorMessage << L"\n";
        return 1;
    }

    auto copyRes = fm.copyFile(L"test_file_ops_renamed.txt", L"test_file_ops_copy.txt", true);
    if (!copyRes.success) {
        std::wcout << L"copyFile failed: " << copyRes.errorMessage << L"\n";
        return 1;
    }

    auto del1 = fm.deleteFile(L"test_file_ops_copy.txt", false);
    auto del2 = fm.deleteFile(L"test_file_ops_renamed.txt", false);

    if (!del1.success || !del2.success) {
        std::wcout << L"deleteFile failed\n";
        return 1;
    }

    std::wcout << L"File operations test OK\n";
    return 0;
}
