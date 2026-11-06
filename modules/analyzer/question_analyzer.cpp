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

bool containsAny(const std::string& question, std::initializer_list<const char*> words);

bool asciiWord(unsigned char c) {
    return c < 0x80 && (std::isalnum(c) || c == '_');
}

// 英文词按词边界匹配，避免把 trust/catalog 里的 rust/log 当成任务证据。
bool keywordMatch(const std::string& text, const std::string& keyword) {
    for (auto at = text.find(keyword); at != std::string::npos;
         at = text.find(keyword, at + 1)) {
        const auto end = at + keyword.size();
        const bool latin = static_cast<unsigned char>(keyword.front()) < 0x80;
        if (!latin) return true;
        const bool left = at == 0 || !asciiWord(static_cast<unsigned char>(text[at - 1]));
        const bool version = (keyword == "python" || keyword == "c++") &&
            end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]));
        const bool right = end == text.size() || !asciiWord(static_cast<unsigned char>(text[end])) || version;
        if (left && right) return true;
    }
    return false;
}

std::string normalize(const std::string& question) {
    auto result = asciiLower(question);
    for (const auto& space : {std::string("\xC2\xA0"), std::string("\xE3\x80\x80")}) {
        for (auto at = result.find(space); at != std::string::npos; at = result.find(space, at + 1))
            result.replace(at, space.size(), " ");
    }
    return result;
}

// 只屏蔽成对引号内的动作；被引用的“润色”不能盖过外层翻译指令。
std::string outsideQuotes(std::string text) {
    const std::pair<const char*, const char*> pairs[] = {
        {"\"", "\""}, {"“", "”"}, {"‘", "’"}, {"`", "`"}, {"'", "'"}, {"《", "》"}};
    for (const auto& pair : pairs) {
        const std::string open = pair.first, close = pair.second;
        std::size_t start = 0;
        while ((start = text.find(open, start)) != std::string::npos) {
            // don't / user's 中的撇号不开始引述。
            if (open == "'" && start > 0 && asciiWord(static_cast<unsigned char>(text[start - 1]))) {
                ++start;
                continue;
            }
            const auto end = text.find(close, start + open.size());
            if (end == std::string::npos) break;
            text.replace(start, end + close.size() - start, end + close.size() - start, ' ');
            start = end + close.size();
        }
    }
    return text;
}

std::string activeQuestion(std::string text) {
    // 仅识别这一组明确的局部否定；复杂的否定作用域仍属于已知局限。
    static const std::regex negated(
        R"(((不要|不用|无需|不需要|别|不必|不)(再|进行)?(翻译|润色|改写|扩写|缩写|写作))|(\b(do not|don't|no need to)\s+(translate|polish|rewrite|paraphrase)\b))");
    text = std::regex_replace(text, negated, " ");
    static const std::regex negative_code(
        R"((不要|不用|无需|不需要|别|不必)(再|进行)?(写|编写|实现|生成|提供|开发|使用|用)?(一个|个|一段|段|任何|相关的)?(代码|编程|程序|函数|算法|爬虫|python[0-9.]*|c\+\+|java)|(\b(do not|don't|no need to|avoid|without)\s+((write|writing|generate|provide|implement|use|using)\s+)?(any\s+|a\s+)?(code|coding|programming|program|function|python|c\+\+)\b))");
    text = std::regex_replace(text, negative_code, " ");
    static const std::regex negative_writing(
        R"((不要|不用|无需|不需要|别|不必)(再)?(写|撰写|起草)(一篇|一封|一个|个|篇|封)?(作文|文章|论文|邮件|故事|报告|总结))");
    return std::regex_replace(text, negative_writing, " ");
}

// Output language is a constraint, not automatically a translation task.
std::string withoutOutputLanguage(std::string text) {
    static const std::regex chinese(
        R"((用|以)(英语|英文|中文|汉语|日语|日文|法语|德语|西班牙语)(来)?\s*(?=回答|解释|说明|讲解|分析|介绍|证明|求解|写|编写|告诉|讲讲|给出|讨论|列出|推导|比较))");
    static const std::regex english(R"(\b(in|using)\s+(english|chinese|french|german|japanese|spanish)\b)");
    return std::regex_replace(std::regex_replace(text, chinese, " "), english, " ");
}

bool programmingAction(const std::string& text) {
    static const std::regex direct(
        R"((写|编写|实现|开发|设计)(一个|个|一段|段)?(函数|程序|代码|算法|爬虫|脚本|软件|网站|网页|应用)|\b(implement|debug)\b)");
    if (std::regex_search(text, direct)) return true;
    // Admit modifiers such as "开发一个翻译软件", without requiring an exact sentence.
    for (const auto* verb : {"编写", "实现", "开发"}) {
        const auto at = text.find(verb);
        if (at != std::string::npos && containsAny(text.substr(at, 120),
                {"函数", "程序", "代码", "算法", "爬虫", "脚本", "软件", "网站", "应用"})) return true;
    }
    return false;
}

bool translationTopic(const std::string& text) {
    static const std::regex nouns(
        R"(((“|\"|‘|')?翻译(”|\"|’|')?(这个|这份|这门)?(软件|工具|行业|职业|工作|专业|平台))|人工翻译|\b(machine translation|translation\s+(software|tools?|career|profession|industry))\b|\b(what is|explain|describe)\s+(machine\s+)?translation\b)");
    return std::regex_search(text, nouns);
}

std::string withoutTranslationTopic(const std::string& text) {
    static const std::regex nouns(
        R"(((“|\"|‘|')?翻译(”|\"|’|')?(这个|这份|这门)?(软件|工具|行业|职业|工作|专业|平台))|人工翻译|\b(machine translation|translation\s+(software|tools?|career|profession|industry))\b|\btranslation\b)");
    return std::regex_replace(text, nouns, " ");
}

struct Context {
    bool code = false, math = false, web_fault = false;
    bool python_animal = false, rust_material = false, everyday = false;
    bool ambiguous = false;
};

Context contextOf(const std::string& text) {
    Context result;
    const bool explicit_code = programmingAction(text) || containsAny(text,
        {"代码", "编程", "报错", "编译", "调试", "指针", "多态", "数据库", "数据结构", "爬虫", "脚本"}) ||
        keywordMatch(text, "sql") || keywordMatch(text, "api") || keywordMatch(text, "json");
    result.python_animal = keywordMatch(text, "python") && !explicit_code && containsAny(text,
        {"动物", "蟒蛇", "爬行动物", "有毒", "饲养", "animal", "snake", "reptile"});
    result.rust_material = keywordMatch(text, "rust") && !explicit_code && containsAny(text,
        {"铁锈", "生锈", "除锈", "腐蚀", "metal", "iron", "rust on"});
    result.web_fault = containsAny(text, {"网页", "网站", "浏览器", "页面", "表单"}) &&
        containsAny(text, {"白屏", "空白", "无响应", "报错", "提交后", "点击后", "加载失败"});
    static const std::regex syntax(
        R"(#\s*include\s*[<"]|\bimport\s+[a-z_]\w*|\bfrom\s+[a-z_][a-z_0-9.]*\s+import\b|\b(def|fn|func|function)\s+[a-z_]\w*\s*\(|\b(for|while|if|switch)\s*\(|\b(class|struct|enum)\s+[a-z_]\w*\s*[:{]|\b(int|void|bool|auto)\s+[a-z_]\w*\s*[(=;])");
    static const std::regex fragment(
        R"(\bstd::|\b(return|throw)\s+[^;\n]+;|\b(break|continue)\s*;)");
    bool language = false;
    for (const auto* name : {"c++", "c语言", "python", "java", "javascript", "typescript", "golang", "rust",
                             "kotlin", "swift", "php", "ruby", "html", "css", "matlab"}) {
        if ((std::string(name) == "python" && result.python_animal) ||
            (std::string(name) == "rust" && result.rust_material)) continue;
        language = language || keywordMatch(text, name);
    }
    result.code = explicit_code || language || result.web_fault ||
        std::regex_search(text, syntax) || std::regex_search(text, fragment);
    result.math = containsAny(text, {"数学", "微积分", "定积分", "不定积分", "求积分", "积分计算", "方程", "导数",
        "极限", "定义域", "值域", "单调性", "函数值", "二次函数", "一次函数", "三角形", "矩阵", "几何", "∫"});
    for (const auto* word : {"mathematics", "math", "calculus", "equation", "triangle", "circle", "radius",
                             "rectangle", "derivative", "integral", "quadratic", "polynomial"})
        result.math = result.math || keywordMatch(text, word);
    result.everyday = result.python_animal || result.rust_material ||
        (text.find("积分") != std::string::npos && containsAny(text, {"信用卡", "会员", "兑换", "商城", "签到", "里程", "奖励"})) ||
        (text.find("继承") != std::string::npos && containsAny(text, {"遗产", "遗嘱", "房产", "财产", "父母"})) ||
        (text.find("对象") != std::string::npos && containsAny(text,
            {"生气", "恋爱", "女友", "男友", "哄", "相亲", "结婚", "伴侣", "不高兴", "开心", "情绪"}));
    result.ambiguous = containsAny(text, {"函数", "变量", "对象", "继承", "积分", "框架", "模块", "引用", "异常"});
    return result;
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
        const auto context = contextOf(q);
        for (int i = 0; i < 5; ++i)
            r.scores[i] = score(static_cast<Category>(i), q, r.evidence[i], context);
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

    int score(Category c, const std::string& q, std::vector<std::string>& evidence, const Context& context) const {
        int idx = (int)c, s = 0;
        for (auto& kw : kws_[idx]) {
            if (c == Category::Code && !context.code && containsAny(kw.first,
                {"函数", "变量", "对象", "接口", "模块", "框架", "异常", "运行", "循环", "迭代", "引用", "继承", "封装", "实现", "怎么写", "示例", "demo"})) continue;
            if (c == Category::Code && ((kw.first == "python" && context.python_animal) ||
                (kw.first == "rust" && context.rust_material))) continue;
            if (c == Category::Code && (kw.first == "网页" || kw.first == "白屏") && !context.web_fault) continue;
            if (c == Category::Math && (kw.first == "函数" || kw.first == "area") && !context.math) continue;
            if (c == Category::Math && kw.first == "积分" && !context.math &&
                !containsAny(q, {"计算", "求解", "化简", "求", "算一下"})) continue;
            if (keywordMatch(q, kw.first)) {
                s += kw.second;
                evidence.push_back("关键词“" + kw.first + "”");
            }
        }
        if (c == Category::Code && !context.code) return s;
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
        addKw(Category::Code, "对象", 2);
        addKw(Category::Code, "接口", 2);      addKw(Category::Code, "模块", 1);
        addKw(Category::Code, "框架", 2);
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
        addKw(Category::Code, "网页", 1);      addKw(Category::Code, "白屏", 3);
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
        addKw(Category::Math, "乘以", 2);       addKw(Category::Math, "除以", 2);
        addKw(Category::Math, "乘法", 2);       addKw(Category::Math, "除法", 2);
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
        addKw(Category::Math, "数学", 3);       addKw(Category::Math, "函数", 2);
        addKw(Category::Math, "三角形", 3);     addKw(Category::Math, "边长", 2);
        addKw(Category::Math, "夹角", 2);       addKw(Category::Math, "半径", 2);
        addKw(Category::Math, "面积", 2);       addKw(Category::Math, "周长", 2);
        addKw(Category::Math, "定义域", 3);     addKw(Category::Math, "值域", 3);
        addKw(Category::Math, "极限", 3);       addKw(Category::Math, "∫", 3);
        for (const auto* word : {"equation", "triangle", "circle", "radius", "rectangle", "derivative",
                                 "integral", "quadratic", "polynomial", "probability", "factorial", "gcd"})
            addKw(Category::Math, word, 3);
        addKw(Category::Math, "area", 2);
        // 数学结构特征 (正则)
        addRe(Category::Math, R"(\d+\s*[+\-*/%^]\s*\d+)", 3);            // 1+2, 3*5, 2^10
        addRe(Category::Math, R"([-+]?\d+(\.\d+)?\s*[x×X]\s*[-+]?\d+(\.\d+)?)", 2); // 3x4
        addRe(Category::Math, R"(\d+\s*=\s*)", 2);                       // 含等号
        addRe(Category::Math, R"(\d+\s*[<>]=?\s*\d+)", 1);               // 比较
        addRe(Category::Math, R"(\b(sin|cos|tan|sqrt|log|ln|pow|abs|exp|factorial)\s*\()", 2);
        addRe(Category::Math, R"(\d+(\.\d+)?\s*(%|％|percent))", 1);     // 百分比
        addRe(Category::Math, R"(^[-+]?(?=[0-9\s+\-*/%^().]*[0-9])[0-9\s+\-*/%^().]+$)", 3); // 算式须含数字

        // ============================ 翻译 ============================
        addKw(Category::Translation, "翻译", 4);
        addKw(Category::Translation, "translate", 4);
        addKw(Category::Translation, "translation", 3);
        addKw(Category::Translation, "译", 1);
        addKw(Category::Translation, "翻译成", 4);
        addKw(Category::Translation, "译成", 3);
        addKw(Category::Translation, "译为", 3);
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
        addKw(Category::Writing, "请假信", 3);
        addKw(Category::Writing, "申请书", 3);
        addKw(Category::Writing, "推荐信", 3);
        addKw(Category::Writing, "道歉信", 3);
        addKw(Category::Writing, "短文", 3);
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
        addKw(Category::Writing, "email", 3);
        addKw(Category::Writing, "essay", 3);
        addKw(Category::Writing, "letter", 3);
        addKw(Category::Writing, "poem", 3);
        addKw(Category::Writing, "polish", 3);
        addKw(Category::Writing, "proofread", 3);
        addKw(Category::Writing, "paraphrase", 3);

        // ============================ 通用 ============================
        addKw(Category::General, "什么是", 1);
        addKw(Category::General, "是什么", 1);
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
        addKw(Category::General, "why", 1);
        addKw(Category::General, "what is", 1);
        addKw(Category::General, "explain", 1);
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
    if (question.size() > 8192)
        return {"general", "输入较长，规则分析无法可靠确定主任务，回退到通用问答", true};
    const auto normalized = normalize(question);
    if (normalized.find_first_not_of(" \t\r\n\f\v") == std::string::npos)
        return {"general", "问题为空或仅含空白，无法判断任务，回退到通用问答", true};

    // C++11 起局部静态对象初始化是线程安全的；后续分类只读取规则表。
    static const QueryClassifier classifier;
    auto active = withoutOutputLanguage(activeQuestion(normalized));
    if (active.find_first_not_of(" \t\r\n\f\v") == std::string::npos)
        return {"general", "未命中有效任务：否定表达后无可分类内容，回退到通用问答", true};
    auto actions = withoutOutputLanguage(activeQuestion(outsideQuotes(normalized)));
    const bool topic = translationTopic(active);
    static const std::regex transform_direction(R"(译成|译为|翻成|中译英|英译中|汉译英|英译汉|\btranslate\b)");
    const bool direction = std::regex_search(actions, transform_direction);
    if (topic && !direction) { active = withoutTranslationTopic(active); actions = withoutTranslationTopic(actions); }
    const auto result = classifier.classify(active);
    static const std::regex translation_action(R"(\btranslate\b|\btranslation\s+(of|for)\b)");
    const bool translate = containsAny(actions,
        {"翻译", "译成", "译为", "翻成", "中译英", "英译中", "汉译英", "英译汉"}) ||
        std::regex_search(actions, translation_action);
    static const std::regex english_writing(R"(\b(polish|proofread|paraphrase)\b|\b(write|draft|compose)\b.*\b(email|essay|letter|poem|story|article|report)\b)");
    const bool writing_object = containsAny(actions,
        {"作文", "文章", "请假信", "请假条", "申请书", "推荐信", "道歉信", "邮件", "文案", "演讲稿", "短文", "故事", "总结"});
    static const std::regex polite_rewrite(
        R"((说|改|表达|写|措辞|语气).*(礼貌|委婉|自然|简洁|正式|流畅)|\b(make|rewrite|rephrase)\b.*\b(polite|natural|concise|formal|clear)\b)");
    const bool prose_target = containsAny(actions,
        {"句子", "这句", "句话", "段话", "文字", "措辞", "语气", "注释"}) ||
        keywordMatch(actions, "sentence") || keywordMatch(actions, "paragraph");
    const bool rewrite_style = std::regex_search(actions, polite_rewrite) &&
        (prose_target || !contextOf(actions).code);
    const bool write = containsAny(actions,
        {"润色", "写作", "写一篇", "起草", "草拟", "撰写", "改写", "扩写", "缩写"}) ||
        (writing_object && actions.find("写") != std::string::npos) ||
        std::regex_search(actions, english_writing) || rewrite_style;

    if (translate && write)
        return {"general", "同时发现翻译与写作意图，主任务不明确，回退到通用问答", true};
    if (translate)
        return {"translation", "优先按明确的翻译意图判断；" + describe(result, Category::Translation), false};
    if (write)
        return {"writing", "优先按明确的写作意图判断；" + describe(result, Category::Writing), false};

    if (programmingAction(actions))
        return {"programming", "优先按明确的编程意图判断；" + describe(result, Category::Code), false};

    const int best_specialized = *std::max_element(result.scores, result.scores + 4);
    const auto context = contextOf(active);
    if (best_specialized == 0 && (topic || context.everyday))
        return {"general", "识别为日常或话题讨论；未发现需要执行翻译、编程或计算的明确要求", false};
    if (best_specialized == 0 && context.ambiguous && !context.code && !context.math)
        return {"general", "只有多义词线索，缺少明确领域上下文，回退到通用问答", true};
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
