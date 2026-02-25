#ifndef JWT_VALIDATOR_H
#define JWT_VALIDATOR_H

// C++20, no Qt. JWT validator (HS256 + RS256). Claims as string map.

#include <string>
#include <map>

class JWTValidator
{
public:
    JWTValidator() = default;
    ~JWTValidator() = default;

    void setHS256Secret(const std::string& secret);
    void setRS256PublicKey(const std::string& publicKey);
    bool validateToken(const std::string& token);

    /** Claims after validateToken; values as string (or JSON string for nested). */
    std::map<std::string, std::string> getClaims() const { return m_claims; }

private:
    bool validateHS256(const std::string& token);
    bool validateRS256(const std::string& token);

    std::string m_hs256Secret;
    std::string m_rs256PublicKey;
    std::map<std::string, std::string> m_claims;
};

#endif // JWT_VALIDATOR_H
