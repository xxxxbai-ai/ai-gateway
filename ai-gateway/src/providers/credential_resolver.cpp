#include "providers/credential_resolver.h"
#include <cstdlib>
#include <stdexcept>

namespace gateway {
std::string CredentialResolver::resolve(const std::string& reference) {
    if (reference.empty()) return {};
    if (reference.rfind("env:", 0) != 0 || reference.size() == 4)
        throw std::runtime_error("凭证引用格式应为 env:环境变量名");
    const char* value = std::getenv(reference.substr(4).c_str());
    if (!value || !*value) throw std::runtime_error("凭证环境变量未配置");
    const std::string secret(value);
    if (secret.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("凭证包含非法换行");
    return secret;
}
}
