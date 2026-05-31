#include "engine_revolver.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}
}  // namespace

int main()
{
    using RawrXD::EngineRevolver;

    auto& r = EngineRevolver::instance();
    r.reset();

    if (auto e = r.setRing(std::vector<std::string>{"eng_a", "eng_b"}); !e)
        return fail("setRing");

    auto s1 = r.advance();
    if (!s1)
        return fail("advance 1");
    if (s1->id != "eng_a" || s1->index != 0 || s1->generation != 0)
        return fail("first step");

    auto s2 = r.advance();
    if (!s2 || s2->id != "eng_b" || s2->index != 1 || s2->generation != 1)
        return fail("second step");

    auto s3 = r.advance();
    if (!s3 || s3->id != "eng_a" || s3->index != 0 || s3->generation != 2)
        return fail("wrap");

    auto pk = r.peek();
    if (pk.id != "eng_b" || pk.generation != 3)
        return fail("peek");

    std::fprintf(stderr, "OK test_engine_revolver\n");
    return 0;
}
