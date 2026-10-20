#pragma once
#include <string>

namespace gateway {
class CredentialResolver {
public:
    // 凭证只从网关进程环境读取，不写配置、源码或日志。
    static std::string resolve(const std::string& reference);
};
}
