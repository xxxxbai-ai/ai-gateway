#include "question_analyzer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

namespace gateway_contract {
namespace {

// 仅供规则引擎内部使用；对外只返回约定的英文任务名。
enum class Category { Code = 0, Math, Translation, Writing, General };

static const char* catName(Category c) {
    switch (c) {
        case Category::Code:        return "代码";
        case Category::Math:        return "数学计算";
        case Category::Translation: return "翻译";
        case Category::Writing:     return "写作";
        case Category::General:     return "通用";
    }
    return "未知";
}

// 仅把 ASCII 字母转小写, 不影响 UTF-8 中文 (中文是多字节, 首字节 > 0x80 会被跳过)
static std::string asciiLower(const std::string& s) {
    std::string r = s;
    for (char& c : r)
        if ((unsigned char)c < 0x80) c = (char)std::tolower((unsigned char)c);
    return r;
}

// ----------------------------------------------------------------------------
// 分类器
// ----------------------------------------------------------------------------
class QueryClassifier {
public:
    struct Result {
        Category category;
        double   confidence;
        int      scores[5];   // 索引顺序 = Category 枚举顺序
        std::array<std::vector<std::string>, 5> evidence;
    };

    QueryClassifier() { buildRules(); }

    Result classify(const std::string& raw) const {
        std::string q = asciiLower(raw);

        Result r{};
        for (int i = 0; i < 5; ++i)
            r.scores[i] = score(static_cast<Category>(i), q, r.evidence[i]);
        r.scores[(int)Category::General] += generalBase_;

        int best = 0, total = 0;
        for (int i = 0; i < 5; ++i) {
            total += r.scores[i];
            if (r.scores[i] > r.scores[best]) best = i;
        }
        r.category   = (Category)best;
        r.confidence = total > 0 ? (double)r.scores[best] / total : 0.0;
        return r;
    }

private:
    int generalBase_ = 1;   // 通用类固定基线分, 作兜底

    std::vector<std::pair<std::string, int>> kws_[5];   // 关键词 -> 权重
    std::vector<std::pair<std::regex,  int>> res_[5];   // 正则   -> 权重

    static int countMatches(const std::regex& re, const std::string& s) {
        int n = 0;
        for (std::sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) ++n;
        return n;
    }

    int score(Category c, const std::string& q, std::vector<std::string>& evidence) const {
        int idx = (int)c, s = 0;
        for (auto& kw : kws_[idx])
            if (q.find(kw.first) != std::string::npos) {
                s += kw.second;
                evidence.push_back("关键词“" + kw.first + "”");
            }
        for (auto& re : res_[idx]) {
            const int count = countMatches(re.first, q);
            s += re.second * count;
            if (count > 0) evidence.push_back("结构规则命中 " + std::to_string(count) + " 次");
        }
        return s;
    }

    void addKw(Category c, const char* w, int weight) { kws_[(int)c].push_back({w, weight}); }
    void addRe(Category c, const char* p, int weight) { res_[(int)c].push_back({std::regex(p), weight}); }

    void buildRules() {
        // ============================ 代码 ============================
        addKw(Category::Code, "代码", 3);      addKw(Category::Code, "程序", 2);
        addKw(Category::Code, "编程", 3);      addKw(Category::Code, "程序员", 2);
        addKw(Category::Code, "函数", 3);      addKw(Category::Code, "变量", 2);
        addKw(Category::Code, "类", 1);        addKw(Category::Code, "对象", 2);
        addKw(Category::Code, "接口", 2);      addKw(Category::Code, "模块", 1);
        addKw(Category::Code, "库", 1);        addKw(Category::Code, "框架", 2);
        addKw(Category::Code, "报错", 3);      addKw(Category::Code, "错误信息", 3);
        addKw(Category::Code, "异常", 2);      addKw(Category::Code, "bug", 3);
        addKw(Category::Code, "编译", 2);      addKw(Category::Code, "运行", 1);
        addKw(Category::Code, "调试", 3);      addKw(Category::Code, "断点", 3);
        addKw(Category::Code, "算法", 2);      addKw(Category::Code, "数据结构", 3);
        addKw(Category::Code, "复杂度", 3);    addKw(Category::Code, "递归", 3);
        addKw(Category::Code, "循环", 2);      addKw(Category::Code, "迭代", 2);
        addKw(Category::Code, "遍历", 2);      addKw(Category::Code, "字符串", 2);
        addKw(Category::Code, "数组", 2);      addKw(Category::Code, "指针", 3);
        addKw(Category::Code, "内存", 2);      addKw(Category::Code, "引用", 2);
        addKw(Category::Code, "多态", 3);      addKw(Category::Code, "继承", 2);
        addKw(Category::Code, "封装", 2);      addKw(Category::Code, "多线程", 3);
        addKw(Category::Code, "并发", 3);      addKw(Category::Code, "线程", 2);
        addKw(Category::Code, "进程", 2);      addKw(Category::Code, "死锁", 3);
        addKw(Category::Code, "数据库", 2);    addKw(Category::Code, "sql", 2);
        addKw(Category::Code, "索引", 2);      addKw(Category::Code, "事务", 2);
        addKw(Category::Code, "api", 2);       addKw(Category::Code, "restful", 2);
        addKw(Category::Code, "json", 2);      addKw(Category::Code, "xml", 2);
        addKw(Category::Code, "正则表达式", 3); addKw(Category::Code, "正则", 2);
        addKw(Category::Code, "爬虫", 3);      addKw(Category::Code, "前端", 2);
        addKw(Category::Code, "后端", 2);      addKw(Category::Code, "git", 2);
        addKw(Category::Code, "linux", 2);     addKw(Category::Code, "shell", 2);
        addKw(Category::Code, "脚本", 2);      addKw(Category::Code, "实现", 1);
        addKw(Category::Code, "如何实现", 2);   addKw(Category::Code, "怎么写", 2);
        addKw(Category::Code, "示例", 1);      addKw(Category::Code, "demo", 2);
        // 语言名
        addKw(Category::Code, "c++", 3);       addKw(Category::Code, "c语言", 3);
        addKw(Category::Code, "python", 3);    addKw(Category::Code, "java", 3);
        addKw(Category::Code, "javascript", 3); addKw(Category::Code, "typescript", 3);
        addKw(Category::Code, "golang", 2);    addKw(Category::Code, "rust", 3);
        addKw(Category::Code, "swift", 2);     addKw(Category::Code, "kotlin", 2);
        addKw(Category::Code, "php", 2);       addKw(Category::Code, "ruby", 2);
        addKw(Category::Code, "html", 2);      addKw(Category::Code, "css", 2);
        addKw(Category::Code, "汇编", 3);      addKw(Category::Code, "matlab", 2);
        // 代码结构特征 (正则)
        addRe(Category::Code, R"(#\s*include\s*[<"])", 4);
        addRe(Category::Code, R"(\b(import|from)\s+[A-Za-z_][A-Za-z0-9_]*)", 2);
        addRe(Category::Code, R"(\b(def|fn|func|function)\s+[A-Za-z_]\w*\s*\()", 3);
        addRe(Category::Code, R"(\b(class|struct|enum|interface|namespace|public|private|protected|static|const|void|int|char|float|double|bool|long|short|unsigned|return|if|else|for|while|switch|case|break|continue|new|delete|throw|try|catch|template|typedef)\b)", 2);
        addRe(Category::Code, R"([{};])", 1);
        addRe(Category::Code, R"(\b(c\+\+|c#|go|js|ts|php)\b)", 2);

        // ============================ 数学 ============================
        addKw(Category::Math, "计算", 2);       addKw(Category::Math, "等于", 2);
        addKw(Category::Math, "多少", 1);       addKw(Category::Math, "方程", 3);
        addKw(Category::Math, "解方程", 3);     addKw(Category::Math, "求解", 2);
        addKw(Category::Math, "求", 1);         addKw(Category::Math, "算", 1);
        addKw(Category::Math, "乘以", 2);       addKw(Category::Math, "除以", 2);
        addKw(Category::Math, "乘", 2);         addKw(Category::Math, "除", 2);
        addKw(Category::Math, "平方", 2);       addKw(Category::Math, "立方", 2);
        addKw(Category::Math, "次方", 2);       addKw(Category::Math, "幂", 2);
        addKw(Category::Math, "平方根", 3);     addKw(Category::Math, "开方", 2);
        addKw(Category::Math, "根号", 3);       addKw(Category::Math, "阶乘", 3);
        addKw(Category::Math, "求和", 3);       addKw(Category::Math, "积分", 3);
        addKw(Category::Math, "微分", 3);       addKw(Category::Math, "导数", 3);
        addKw(Category::Math, "数列", 2);       addKw(Category::Math, "等差数列", 3);
        addKw(Category::Math, "等比数列", 3);   addKw(Category::Math, "最大公约数", 3);
        addKw(Category::Math, "最小公倍数", 3); addKw(Category::Math, "质数", 3);
        addKw(Category::Math, "素数", 3);       addKw(Category::Math, "合数", 2);
        addKw(Category::Math, "进制", 2);       addKw(Category::Math, "二进制", 3);
        addKw(Category::Math, "十六进制", 3);   addKw(Category::Math, "圆周率", 2);
        addKw(Category::Math, "π", 2);          addKw(Category::Math, "百分比", 2);
        addKw(Category::Math, "百分数", 2);     addKw(Category::Math, "约分", 2);
        addKw(Category::Math, "通分", 2);       addKw(Category::Math, "化简", 2);
        addKw(Category::Math, "换算", 2);       addKw(Category::Math, "单位换算", 2);
        addKw(Category::Math, "摄氏度", 2);     addKw(Category::Math, "华氏", 2);
        addKw(Category::Math, "sin", 2);        addKw(Category::Math, "cos", 2);
        addKw(Category::Math, "tan", 2);        addKw(Category::Math, "log", 2);
        addKw(Category::Math, "sqrt", 2);       addKw(Category::Math, "ln", 1);
        addKw(Category::Math, "abs", 1);        addKw(Category::Math, "exp", 1);
        addKw(Category::Math, "mod", 1);        addKw(Category::Math, "斐波那契", 3);
        addKw(Category::Math, "矩阵", 3);       addKw(Category::Math, "行列式", 3);
        addKw(Category::Math, "向量", 2);       addKw(Category::Math, "概率", 2);
        addKw(Category::Math, "排列组合", 3);   addKw(Category::Math, "组合数", 3);
        addKw(Category::Math, "排列数", 3);     addKw(Category::Math, "几何", 2);
        // 数学结构特征 (正则)
        addRe(Category::Math, R"(\d+\s*[+\-*/%^]\s*\d+)", 3);            // 1+2, 3*5, 2^10
        addRe(Category::Math, R"([-+]?\d+(\.\d+)?\s*[x×X]\s*[-+]?\d+(\.\d+)?)", 2); // 3x4
        addRe(Category::Math, R"(\d+\s*=\s*)", 2);                       // 含等号
        addRe(Category::Math, R"(\d+\s*[<>]=?\s*\d+)", 1);               // 比较
        addRe(Category::Math, R"(\b(sin|cos|tan|sqrt|log|ln|pow|abs|exp|factorial)\s*\()", 2);
        addRe(Category::Math, R"(\d+(\.\d+)?\s*(%|％|percent))", 1);     // 百分比
        addRe(Category::Math, R"(^[-+]?[0-9\s+\-*/%^().]+$)", 3);        // 整句是算式

        // ============================ 翻译 ============================
        addKw(Category::Translation, "翻译", 4);
        addKw(Category::Translation, "translate", 4);
        addKw(Category::Translation, "translation", 3);
        addKw(Category::Translation, "译", 1);
        addKw(Category::Translation, "翻译成", 4);
        addKw(Category::Translation, "译成", 3);
        addKw(Category::Translation, "翻成", 3);
        addKw(Category::Translation, "中译英", 3);
        addKw(Category::Translation, "英译中", 3);
        addKw(Category::Translation, "汉译英", 3);
        addKw(Category::Translation, "英译汉", 3);
        addKw(Category::Translation, "什么意思", 2);
        addKw(Category::Translation, "是什么意思", 2);
        addKw(Category::Translation, "怎么读", 2);
        addKw(Category::Translation, "怎么念", 2);
        addKw(Category::Translation, "发音", 2);
        addKw(Category::Translation, "怎么说", 2);
        addKw(Category::Translation, "怎么讲", 2);
        addKw(Category::Translation, "用英语", 3);
        addKw(Category::Translation, "用英文", 3);
        addKw(Category::Translation, "用日语", 3);
        addKw(Category::Translation, "拼写", 2);
        // 语言名(作翻译对象)
        addKw(Category::Translation, "英文", 2);
        addKw(Category::Translation, "英语", 2);
        addKw(Category::Translation, "汉语", 2);
        addKw(Category::Translation, "日语", 2);
        addKw(Category::Translation, "韩语", 2);
        addKw(Category::Translation, "法语", 2);
        addKw(Category::Translation, "德语", 2);
        addKw(Category::Translation, "西班牙语", 2);
        addKw(Category::Translation, "俄语", 2);
        addKw(Category::Translation, "意大利语", 2);
        addKw(Category::Translation, "葡萄牙语", 2);
        addRe(Category::Translation, R"(\btranslate\b.*\bto\b)", 3);

        // ============================ 写作 ============================
        addKw(Category::Writing, "写作", 3);
        addKw(Category::Writing, "写一篇", 3);
        // “帮我写/写”是中性动作，不独立加分，依靠函数/作文等宾语判断。
        addKw(Category::Writing, "文章", 3);
        addKw(Category::Writing, "作文", 3);
        addKw(Category::Writing, "论文", 3);
        addKw(Category::Writing, "报告", 2);
        addKw(Category::Writing, "邮件", 2);
        addKw(Category::Writing, "文案", 3);
        addKw(Category::Writing, "演讲稿", 3);
        addKw(Category::Writing, "通知", 2);
        addKw(Category::Writing, "请假条", 3);
        addKw(Category::Writing, "简历", 2);
        addKw(Category::Writing, "自我介绍", 2);
        addKw(Category::Writing, "情书", 3);
        addKw(Category::Writing, "总结", 2);
        addKw(Category::Writing, "摘要", 2);
        addKw(Category::Writing, "读后感", 3);
        addKw(Category::Writing, "观后感", 3);
        addKw(Category::Writing, "周报", 2);
        addKw(Category::Writing, "日报", 2);
        addKw(Category::Writing, "工作汇报", 3);
        addKw(Category::Writing, "润色", 3);
        addKw(Category::Writing, "改写", 3);
        addKw(Category::Writing, "扩写", 3);
        addKw(Category::Writing, "缩写", 3);
        addKw(Category::Writing, "美化", 2);
        addKw(Category::Writing, "优化表达", 3);
        addKw(Category::Writing, "精简", 2);
        addKw(Category::Writing, "标题", 2);
        addKw(Category::Writing, "大纲", 3);
        addKw(Category::Writing, "开头怎么写", 3);
        addKw(Category::Writing, "故事", 2);
        addKw(Category::Writing, "小说", 2);
        addKw(Category::Writing, "诗歌", 3);
        addKw(Category::Writing, "散文", 3);
        addKw(Category::Writing, "对联", 3);
        addKw(Category::Writing, "顺口溜", 3);
        addKw(Category::Writing, "起草", 3);
        addKw(Category::Writing, "草拟", 3);
        addKw(Category::Writing, "撰写", 3);
        addKw(Category::Writing, "创作", 3);
        addKw(Category::Writing, "广告语", 3);
        addKw(Category::Writing, "标语", 3);
        addKw(Category::Writing, "口号", 2);

        // ============================ 通用 ============================
        addKw(Category::General, "什么是", 1);
        addKw(Category::General, "为什么", 1);
        addKw(Category::General, "怎么", 1);
        addKw(Category::General, "如何", 1);
        addKw(Category::General, "介绍", 1);
        addKw(Category::General, "解释", 1);
        addKw(Category::General, "区别", 1);
        addKw(Category::General, "对比", 1);
        addKw(Category::General, "推荐", 1);
        addKw(Category::General, "建议", 1);
        addKw(Category::General, "历史", 1);
        addKw(Category::General, "定义", 1);
        addKw(Category::General, "原理", 1);
        addKw(Category::General, "原因", 1);
        addKw(Category::General, "含义", 1);
        addKw(Category::General, "知识", 1);
        addKw(Category::General, "百科", 1);
        addKw(Category::General, "请问", 1);
        addKw(Category::General, "怎么样", 1);
        addKw(Category::General, "哪些", 1);
        addKw(Category::General, "哪个", 1);
    }
};

const char* taskId(Category category) {
    switch (category) {
    case Category::Code: return "programming";
    case Category::Math: return "math";
    case Category::Translation: return "translation";
    case Category::Writing: return "writing";
    case Category::General: return "general";
    }
    return "general";
}

std::string describe(const QueryClassifier::Result& result, Category category) {
    const auto index = static_cast<std::size_t>(category);
    std::ostringstream reason;
    reason << "识别为" << catName(category) << "；命中";
    const auto& evidence = result.evidence[index];
    if (evidence.empty()) return std::string("识别为") + catName(category) + "；按明确动作判断";
    for (std::size_t i = 0; i < std::min<std::size_t>(evidence.size(), 3); ++i) {
        if (i != 0) reason << "、";
        reason << evidence[i];
    }
    reason << "；规则得分 " << result.scores[index];
    return reason.str();
}

bool containsAny(const std::string& question,
                 std::initializer_list<const char*> words) {
    for (const auto* word : words)
        if (question.find(word) != std::string::npos) return true;
    return false;
}

} // namespace

AnalysisResult analyzeQuestion(const std::string& question) {
    if (question.find_first_not_of(" \t\r\n\f\v") == std::string::npos)
        return {"general", "问题为空或仅含空白，无法判断任务，回退到通用问答", true};

    // C++11 起局部静态对象初始化是线程安全的；后续分类只读取规则表。
    static const QueryClassifier classifier;
    const auto result = classifier.classify(question);
    const auto normalized = asciiLower(question);
    static const std::regex translation_action(R"(\b(translate|translation)\b)");
    const bool translate = containsAny(normalized,
        {"翻译", "译成", "翻成", "中译英", "英译中", "汉译英", "英译汉"}) ||
        std::regex_search(normalized, translation_action);
    const bool write = containsAny(normalized,
        {"润色", "写作", "写一篇", "起草", "草拟", "撰写", "改写", "扩写", "缩写"});

    if (translate && write)
        return {"general", "同时发现翻译与写作意图，主任务不明确，回退到通用问答", true};
    if (translate)
        return {"translation", "优先按明确的翻译意图判断；" + describe(result, Category::Translation), false};
    if (write)
        return {"writing", "优先按明确的写作意图判断；" + describe(result, Category::Writing), false};

    const int best_specialized = *std::max_element(result.scores, result.scores + 4);
    if (best_specialized == 0) {
        if (!result.evidence[4].empty())
            return {"general", describe(result, Category::General), false};
        return {"general", "未命中专门任务规则，也无明确通用问答线索，回退到通用问答", true};
    }

    if (result.category == Category::General)
        return {"general", "专门任务证据未超过通用问答得分，主任务不明确，回退到通用问答", true};

    int tied = 0;
    for (int i = 0; i < 4; ++i)
        if (result.scores[i] == best_specialized) ++tied;
    if (tied > 1)
        return {"general", "多个专门任务最高分相同，主任务不明确，回退到通用问答", true};

    return {taskId(result.category), describe(result, result.category), false};
}

} // namespace gateway_contract
