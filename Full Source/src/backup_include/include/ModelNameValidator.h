/**
 * @file ModelNameValidator.h
 * @brief Permissive model name validation (stub for patchable build).
 */
#ifndef MODEL_NAME_VALIDATOR_H
#define MODEL_NAME_VALIDATOR_H

#include <string>

namespace RawrXD {

bool isValidModelName(const std::string& modelName);
std::string sanitizeModelName(const std::string& modelName);
std::string extractModelBaseName(const std::string& fullPath);

} // namespace RawrXD

#endif
