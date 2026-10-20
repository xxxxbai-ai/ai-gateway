#pragma once
#include "common/contracts.h"
#include <vector>

namespace gateway {
// C负责：首版使用内存配置，后续在这里接入SQLite。
class ModelRepository {
public:
    ModelRepository();
    explicit ModelRepository(std::vector<ModelConfig> models);
    // 联调时从无密钥 JSON 读取配置；以后可由 C 替换成 SQLite 快照。
    static std::vector<ModelConfig> loadConfig(const std::string& path);
    std::vector<ModelConfig> listModels() const;
private:
    std::vector<ModelConfig> models_;
};
}
