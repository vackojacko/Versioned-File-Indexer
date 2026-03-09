#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <string>
using namespace std;

// CHANGE 1: template utility for timing
template <typename F>
double measureSeconds(F&& f) {
    auto start = chrono::steady_clock::now();
    f();
    auto end = chrono::steady_clock::now();
    return chrono::duration<double>(end - start).count();
}

// CHANGE 2: second user-defined template (clamp)
template <typename T>
T clamp(T value, T lo, T hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

// ─────────────────────────────────────────────
// BufferedFileReader
// ─────────────────────────────────────────────
class BufferedFileReader {
private:
    ifstream file;
public:
    // CHANGE 3: explicit constructor
    explicit BufferedFileReader(const string& path) {
        file.open(path, ios::binary);
        if (!file) throw runtime_error("File open failure: " + path);
    }

    // CHANGE 4: non-copyable (RAII correctness)
    BufferedFileReader(const BufferedFileReader&) = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    bool readChunk(vector<char>& buffer, size_t& bytesRead) {
        file.read(buffer.data(), buffer.size());
        bytesRead = file.gcount();
        return bytesRead > 0;
    }
};

// ─────────────────────────────────────────────
// VersionedIndexer
// ─────────────────────────────────────────────
class VersionedIndexer {
private:
    unordered_map<string, size_t> frequency;
    string versionName;

public:
    // CHANGE 5: explicit constructor
    explicit VersionedIndexer(const string& name) : versionName(name) {}

    void addWord(const string& word) {
        if (!word.empty()) frequency[word]++;
    }

    size_t getWordCount(const string& word) const {
        auto it = frequency.find(word);
        if (it != frequency.end()) return it->second;
        return 0;
    }

    const unordered_map<string, size_t>& getFrequency() const {
        return frequency;
    }
};

// ─────────────────────────────────────────────
// Tokenizer
// ─────────────────────────────────────────────
class Tokenizer {
private:
    string currentWord;
    VersionedIndexer& idx;

public:
    // CHANGE 6: explicit constructor
    explicit Tokenizer(VersionedIndexer& indexer) : idx(indexer) {}

    void ProcessWord(const vector<char>& buffer, size_t bytesRead) {
        size_t index = 0;
        while (index < bytesRead) {
            char c = buffer[index];
            unsigned char uc = static_cast<unsigned char>(c);
            if (isalnum(uc))
                currentWord += static_cast<char>(tolower(uc));
            else {
                if (!currentWord.empty()) {
                    idx.addWord(currentWord);
                    currentWord.clear();
                }
            }
            index++;
        }
    }

    void flush() {
        if (!currentWord.empty()) {
            idx.addWord(currentWord);
            currentWord.clear();
        }
    }
};

// ─────────────────────────────────────────────
// VersionManager
// ─────────────────────────────────────────────
class VersionManager {
private:
    unordered_map<string, VersionedIndexer> versions;

public:
    void createVersion(const string& name) {
        if (versions.count(name))
            throw runtime_error("Version already exists: " + name);
        versions.emplace(name, VersionedIndexer(name));
    }

    VersionedIndexer& getVersion(const string& name) {
        return versions.at(name);
    }

    // CHANGE 7: const overload of getVersion (was accidentally placed OUTSIDE the class before)
    const VersionedIndexer& getVersion(const string& name) const {
        return versions.at(name);
    }
};

// ─────────────────────────────────────────────
// Query — abstract base class
// ─────────────────────────────────────────────
class Query {
public:
    virtual void execute(VersionManager& manager) = 0;
    // CHANGE 8: = default instead of {}
    virtual ~Query() = default;
};

// ─────────────────────────────────────────────
// WordQuery
// ─────────────────────────────────────────────
class WordQuery : public Query {
private:
    string version;
    string word;

    // CHANGE 9: normalize extracted into private static helper (no duplicate loop in execute)
    static string normalize(const string& w) {
        string out = w;
        for (char& c : out)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return out;
    }

public:
    WordQuery(const string& v, const string& w) : version(v), word(normalize(w)) {}

    void execute(VersionManager& manager) override {
        const VersionedIndexer& index = manager.getVersion(version);
        size_t ans = index.getWordCount(word);
        cout << "Version: " << version << "\n";
        cout << "Count: " << ans << "\n";
    }
};

// ─────────────────────────────────────────────
// DiffQuery
// ─────────────────────────────────────────────
class DiffQuery : public Query {
private:
    string version1;
    string version2;
    string word;

    // CHANGE 10: same normalize helper here too
    static string normalize(const string& w) {
        string out = w;
        for (char& c : out)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return out;
    }

public:
    DiffQuery(const string& v1, const string& v2, const string& w)
        : version1(v1), version2(v2), word(normalize(w)) {}

    void execute(VersionManager& manager) override {
        const VersionedIndexer& v1 = manager.getVersion(version1);
        const VersionedIndexer& v2 = manager.getVersion(version2);
        long long diff = static_cast<long long>(v2.getWordCount(word))
                       - static_cast<long long>(v1.getWordCount(word));
        cout << "Difference(v2-v1): " << diff << endl;
    }
};

// ─────────────────────────────────────────────
// TopKQuery
// ─────────────────────────────────────────────
class TopKQuery : public Query {
private:
    string version;
    size_t k;

public:
    TopKQuery(const string& v, size_t k) : version(v), k(k) {}

    void execute(VersionManager& manager) override {
        const VersionedIndexer& index = manager.getVersion(version);
        const unordered_map<string, size_t>& currMap = index.getFrequency();

        priority_queue<pair<size_t, string>,
                       vector<pair<size_t, string>>,
                       greater<pair<size_t, string>>> pq;

        for (const auto& it : currMap) {
            if (pq.size() < k) pq.push({it.second, it.first});
            else if (pq.top().first < it.second) {
                pq.pop();
                pq.push({it.second, it.first});
            }
        }

        vector<pair<size_t, string>> result;
        while (!pq.empty()) {
            result.push_back({pq.top().first, pq.top().second});
            pq.pop();
        }
        reverse(result.begin(), result.end());

        cout << "Top " << k << " words in version " << version << ":" << endl;
        for (const auto& it : result)
            cout << it.second << "   " << it.first << endl;
    }
};

// ─────────────────────────────────────────────
// buildIndex — overload #1 (single file)
// ─────────────────────────────────────────────
void buildIndex(const string& filePath, const string& versionName,
                size_t bufferSize, VersionManager& manager)
{
    manager.createVersion(versionName);
    VersionedIndexer& v = manager.getVersion(versionName);

    BufferedFileReader reader(filePath);
    Tokenizer token(v);
    vector<char> buffer(bufferSize);
    size_t bytesRead = 0;

    while (reader.readChunk(buffer, bytesRead))
        token.ProcessWord(buffer, bytesRead);

    token.flush();
}

// buildIndex — overload #2 (two files, function overloading)
void buildIndex(const string& file1, const string& file2,
                const string& version1, const string& version2,
                size_t bufferSize, VersionManager& manager)
{
    buildIndex(file1, version1, bufferSize, manager);
    buildIndex(file2, version2, bufferSize, manager);
}

// ─────────────────────────────────────────────
// CLI argument struct
// ─────────────────────────────────────────────
struct arguments {
    string file, file1, file2;
    string version, version1, version2;
    string queryType, word;
    size_t topK     = 0;
    size_t bufferKB = 512;
};

// CHANGE 11: CLI parsing extracted into its own function (main stays clean)
arguments parseArguments(int argc, char* argv[]) {
    arguments args;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--file") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --file");
            args.file = argv[++i];
        }
        else if (arg == "--buffer") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --buffer");
            args.bufferKB = stoul(argv[++i]);
        }
        else if (arg == "--file1") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --file1");
            args.file1 = argv[++i];
        }
        else if (arg == "--file2") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --file2");
            args.file2 = argv[++i];
        }
        else if (arg == "--version") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --version");
            args.version = argv[++i];
        }
        else if (arg == "--version1") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --version1");
            args.version1 = argv[++i];
        }
        else if (arg == "--version2") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --version2");
            args.version2 = argv[++i];
        }
        else if (arg == "--query") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --query");
            args.queryType = argv[++i];
        }
        else if (arg == "--word") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --word");
            args.word = argv[++i];
        }
        else if (arg == "--top") {
            if (i + 1 >= argc) throw runtime_error("Missing value for --top");
            args.topK = stoul(argv[++i]);
        }
        else {
            throw runtime_error("Unknown argument: " + arg);
        }
    }
    size_t clamped = ::clamp(args.bufferKB, (size_t)256, (size_t)1024);
    if (clamped != args.bufferKB)
        throw runtime_error("Buffer must be between 256 and 1024 KB");
    return args;
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    try {
        // CHANGE 11 (continued): main now just calls parseArguments
        arguments args = parseArguments(argc, argv);
        VersionManager manager;
        size_t bufferBytes = args.bufferKB * 1024;

        if (args.queryType == "word") {
            if (args.file.empty() || args.version.empty() || args.word.empty())
                throw runtime_error("Missing argument");

            double seconds = measureSeconds([&]() {
                buildIndex(args.file, args.version, bufferBytes, manager);
                unique_ptr<Query> q = make_unique<WordQuery>(args.version, args.word);
                q->execute(manager);
            });
            cout << "Buffer Size(KB): " << args.bufferKB << "\n";
            cout << "Execution Time(s): " << seconds << "\n";

        } else if (args.queryType == "diff") {
            if (args.file1.empty() || args.file2.empty() ||
                args.version1.empty() || args.version2.empty() || args.word.empty())
                throw runtime_error("Missing argument");

            double seconds = measureSeconds([&]() {
                buildIndex(args.file1, args.file2, args.version1, args.version2, bufferBytes, manager);
                unique_ptr<Query> q = make_unique<DiffQuery>(args.version1, args.version2, args.word);
                q->execute(manager);
            });
            cout << "Buffer Size(KB): " << args.bufferKB << "\n";
            cout << "Execution Time(s): " << seconds << "\n";

        } else if (args.queryType == "top") {
            if (args.file.empty() || args.version.empty() || args.topK == 0)
                throw runtime_error("Missing argument");

            double seconds = measureSeconds([&]() {
                buildIndex(args.file, args.version, bufferBytes, manager);
                unique_ptr<Query> q = make_unique<TopKQuery>(args.version, args.topK);
                cout << "Version: " << args.version << "\n";
                q->execute(manager);
            });
            cout << "Buffer Size(KB): " << args.bufferKB << "\n";
            cout << "Execution Time(s): " << seconds << "\n";

        } else {
            throw runtime_error("Invalid query type");
        }

    // CHANGE 12: errors go to cerr (Unix convention)
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
