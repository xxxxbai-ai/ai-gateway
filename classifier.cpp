// ============================================================================
//  AI 网关问题意图分类器 —— 将输入问题分到 5 类:
//    代码 / 数学计算 / 翻译 / 写作 / 通用
//  原理: 基于规则的关键词 + 正则 加权打分, 取最高分类别, 通用类作兜底。
//  编译: g++ -std=c++17 -O2 classifier.cpp -o classifier.exe
//  用法:
//    classifier.exe "问题文本"      -> 分类单个问题
//    classifier.exe --test          -> 跑内置自测用例
//    echo 问题 | classifier.exe     -> 从标准输入逐行分类 (UTF-8)
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <cctype>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

// ----------------------------------------------------------------------------
// 类别定义 (枚举顺序同时作为平局时的优先级: 代码 > 数学 > 翻译 > 写作 > 通用)
// ----------------------------------------------------------------------------
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
    };

    QueryClassifier() { buildRules(); }

    Result classify(const std::string& raw) const {
        std::string q = asciiLower(raw);

        Result r{};
        r.scores[(int)Category::Code]        = score(Category::Code, q);
        r.scores[(int)Category::Math]        = score(Category::Math, q);
        r.scores[(int)Category::Translation] = score(Category::Translation, q);
        r.scores[(int)Category::Writing]     = score(Category::Writing, q);
        r.scores[(int)Category::General]     = generalBase_ + score(Category::General, q);

        int best = 0, total = 0;
        for (int i = 0; i < 5; ++i) {
            total += r.scores[i];
            if (r.scores[i] > r.scores[best]) best = i;   // 严格大于 => 平局取枚举顺序靠前者
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

    int score(Category c, const std::string& q) const {
        int idx = (int)c, s = 0;
        for (auto& kw : kws_[idx])
            if (q.find(kw.first) != std::string::npos) s += kw.second;
        for (auto& re : res_[idx])
            s += re.second * countMatches(re.first, q);
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
        addKw(Category::Writing, "帮我写", 2);   // 中性动作, 靠宾语名词区分
        addKw(Category::Writing, "写", 1);
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

// ----------------------------------------------------------------------------
// 输出
// ----------------------------------------------------------------------------
static void printResult(const std::string& q, const QueryClassifier::Result& r) {
    std::cout << "[" << catName(r.category) << "] " << q << "\n";
    std::cout << "    评分: 代码=" << r.scores[0]
              << " 数学=" << r.scores[1]
              << " 翻译=" << r.scores[2]
              << " 写作=" << r.scores[3]
              << " 通用=" << r.scores[4]
              << " | 置信度=" << r.confidence << "\n";
}

// ----------------------------------------------------------------------------
// 自测
// ----------------------------------------------------------------------------
static int selfTest() {
    struct Case { const char* q; Category expect; };
    static const Case cases[] = {
        {"1+2*3等于多少",                 Category::Math},
        {"计算256的平方根",               Category::Math},
        {"解方程 x^2+2x+1=0",             Category::Math},
        {"sqrt(16)等于几",                Category::Math},
        {"100的二进制是多少",             Category::Math},
        {"帮我写一个Python爬虫",          Category::Code},
        {"C++里面指针和引用有什么区别",   Category::Code},
        {"这段代码报错了: NullPointerException", Category::Code},
        {"for (int i=0;i<n;i++) { }",     Category::Code},
        {"import numpy as np",            Category::Code},
        {"用Java实现单例模式",            Category::Code},
        {"把'你好'翻译成英文",            Category::Translation},
        {"translate this to French",      Category::Translation},
        {"这个单词怎么读",                Category::Translation},
        {"帮我写一篇关于环保的作文",      Category::Writing},
        {"帮我润色一下这段文字",          Category::Writing},
        {"写一个工作总结",                Category::Writing},
        {"什么是人工智能",                Category::General},
        {"为什么天空是蓝色的",            Category::General},
        {"介绍一下深度学习",              Category::General},
        {"今天天气怎么样",                Category::General},
    };

    QueryClassifier cls;
    int pass = 0, total = (int)(sizeof(cases) / sizeof(cases[0]));
    for (auto& c : cases) {
        auto r = cls.classify(c.q);
        bool ok = (r.category == c.expect);
        if (ok) ++pass;
        std::cout << (ok ? "[PASS] " : "[FAIL] ");
        printResult(c.q, r);
    }
    std::cout << "\n==== 自测结果: " << pass << "/" << total << " 通过 ====\n";
    return (pass == total) ? 0 : 1;
}

// Windows 下把 argv 从系统代码页(GBK)转成 UTF-8
static std::string acpToUtf8(const std::string& in) {
#ifdef _WIN32
    if (in.empty()) return in;
    int wlen = MultiByteToWideChar(CP_ACP, 0, in.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return in;
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, in.c_str(), -1, &w[0], wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return in;
    std::string u(ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &u[0], ulen, nullptr, nullptr);
    while (!u.empty() && u.back() == '\0') u.pop_back();
    return u;
#else
    return in;
#endif
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    QueryClassifier cls;

    if (argc >= 2 && std::string(argv[1]) == "--test") {
        return selfTest();
    }

    if (argc >= 2) {
        // 把多个参数拼成一句, 再分类
        std::ostringstream oss;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) oss << ' ';
            oss << acpToUtf8(argv[i]);
        }
        printResult(oss.str(), cls.classify(oss.str()));
        return 0;
    }

    // 从标准输入逐行分类 (UTF-8)
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        printResult(line, cls.classify(line));
    }
    return 0;
}
