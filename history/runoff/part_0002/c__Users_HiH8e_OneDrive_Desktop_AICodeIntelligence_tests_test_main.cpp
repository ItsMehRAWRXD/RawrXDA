#include "indexer.hpp"
#include <iostream>
#include <cstdlib>

static int expect_true(bool v, const char* msg) {
    if (!v) {
        std::cerr << "FAIL: " << msg << "\n";
        return 1;
    }
    return 0;
}

int main() {
    int rc = 0;
    Indexer ix;
    IndexOptions opts;
    opts.exts = { ".c", ".py" };
    opts.include = { "*tests*" };
    opts.threads = 2;
    Index idx = ix.index_root("..", opts); // project root is parent of build dir

    // find foo symbol
    auto syms = idx.find_symbols("foo");
    rc |= expect_true(!syms.empty(), "expected symbol 'foo'");

    // python class detected
    auto pysyms = idx.find_symbols("MyClass");
    rc |= expect_true(!pysyms.empty(), "expected class 'MyClass'");

    // references to foo
    auto refs = idx.find_references("foo");
    rc |= expect_true(!refs.empty(), "expected references to 'foo'");

    // security findings exist for test data
    bool has_cl = false, has_py = false;
    for (const auto& f : idx.findings()) {
        if (f.id == "CL001") has_cl = true;
        if (f.id == "PY001" || f.id == "PY003") has_py = true;
    }
    rc |= expect_true(has_cl, "expected CL001 finding (strcpy)");
    rc |= expect_true(has_py, "expected python finding (eval or subprocess shell=True)");

    // ensure default excludes prevent node_modules files
    bool found_nm = false;
    for (const auto& mkv : idx.metrics_by_file()) {
        if (mkv.first.find("node_modules") != std::string::npos) { found_nm = true; break; }
    }
    rc |= expect_true(!found_nm, "default excludes should ignore node_modules");

    if (rc == 0) std::cout << "PASS\n";
    return rc;
}
