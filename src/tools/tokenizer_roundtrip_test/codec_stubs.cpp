#include <vector>

namespace codec
{

std::vector<unsigned char> deflate(const std::vector<unsigned char>& in, bool* ok)
{
    if (ok)
        *ok = false;
    return in;
}

std::vector<unsigned char> inflate(const std::vector<unsigned char>& in, bool* ok)
{
    if (ok)
        *ok = false;
    return in;
}

}  // namespace codec
