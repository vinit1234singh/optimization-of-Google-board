#include "processing.h"

using std::string;
using std::unordered_map;
using std::filesystem::path;

string get_first_n_words(int n, const std::string &text) {
    std::stringstream ss(text);
    std::string result;
    std::string word;
    int i = 0;
    while (i < n && ss >> word) {
        result += word;
        if (i != n - 1) {
            result += " ";
        }
        ++i;
    }
    return result;
}
void count_probabilities(unordered_map<string, double> &prob_map, const unordered_map<string, int> &phrase_map_n,unordered_map<string, int> &phrase_map_n_1, int n) {
    for (const auto &el: phrase_map_n) {
        auto first_n_1_words = get_first_n_words(n - 1, el.first);
        prob_map[el.first] = static_cast<double>(el.second) / static_cast<double>(phrase_map_n_1[first_n_1_words]);
    }
}
void make_ngrams(tbb::concurrent_hash_map<string, int> &map, std::vector<string> &w, int n) {
    for (size_t i = 0; i <= w.size() - n; i++) {
        string phrase;
        for (int j = 0; j < n; j++) {
            phrase += w[i + j];
            if (j != n - 1) {
                phrase += " ";
            }
        }
        tbb::concurrent_hash_map<string, int>::accessor a;
        map.insert(a, phrase);
        a->second++;
        a.release();
    }
}
void make_ngrams(unordered_map<string, int> &ph_map, std::vector<string> &w, int n) {
    for (size_t i = 0; i <= w.size() - n; i++) {
        string phrase;
        for (int j = 0; j < n; j++) {
            phrase += w[i + j];
            if (j != n - 1) {
                phrase += " ";
            }
        }
        ++ph_map[phrase];
    }
}
void count_ngrams(tbb::concurrent_hash_map<string, int> &g_map_n,tbb::concurrent_hash_map<string, int> &g_map_n_1,const string &line, std::unordered_map<std::string,int> dict_eng, int n) {
    using boost::locale::boundary::ssegment_index;
    string start = "<s>";
    string end = "</s>";

    ssegment_index splitter_one(boost::locale::boundary::sentence, line.begin(), line.end());
    splitter_one.rule(boost::locale::boundary::sentence_any);

    int unk = 0;
    int known = 0;

    for (ssegment_index::iterator i = splitter_one.begin(), e = splitter_one.end(); i != e; ++i) {
        string piece = i->str();
        ssegment_index splitter(boost::locale::boundary::word, piece.begin(), piece.end());
        splitter.rule(boost::locale::boundary::word_letters);
        std::vector<string> words;
        words.reserve(n - 1);
        for (int k = 0; k < n - 1; k++) {
            words.push_back(start);
        }
        for (ssegment_index::iterator j = splitter.begin(), k = splitter.end(); j != k; ++j) {
            string word = boost::locale::fold_case(boost::locale::normalize(j->str()));
            words.push_back(word);
        }
        if (words.size() > 1) {
            words.push_back(end);

            unordered_map<string, int> word_map;
            make_ngrams(word_map, words, 1);

            for (const auto &el: word_map) {
                if (dict_eng.find(el.first) == dict_eng.end() && el.first != "<s>" && el.first != "</s>") {
                    std::replace(words.begin(), words.end(), el.first, std::string("<unk>"));
                    unk++;
                } else {
                    known++;
                }
            }

            make_ngrams(g_map_n, words, n);
            make_ngrams(g_map_n_1, words, n - 1);
        }
    }
}

void index_string(safe_que<string> &queue, tbb::concurrent_hash_map<string, int> &g_map_n,tbb::concurrent_hash_map<string, int> &g_map_n_1,const std::unordered_map<std::string, int> &dict_eng, const string &ext, int n) {
    auto buff = static_cast<char *>(::operator new(11000000));

    for (;;) {
        unordered_map<string, int> local_map_n{};
        unordered_map<string, int> local_map_n_1{};
        auto element = queue.pop();
        auto line = element.first;
        if (line.empty()) {
            string empty_s;
            queue.push_end(empty_s, 1, "path");
            break;
        }
        if (element.second == ".zip" && ext.find(".zip") != string::npos) {
            struct archive *a;
            struct archive_entry *entry;
            int err_code;

            memset(buff, 0, 11000000);

            a = archive_read_new();
            archive_read_support_filter_all(a);
            archive_read_support_format_zip(a);

            err_code = archive_read_open_memory(a, line.data(), line.size());

            if (err_code != ARCHIVE_OK) {
                continue;
            }
            while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                if (archive_entry_size(entry) > 0 && archive_entry_size(entry) < 10000000 &&
                    static_cast<path>(archive_entry_pathname(entry)).extension() == ".txt") {
                    auto size = archive_entry_size(entry);
                    archive_read_data(a, buff, size);
                    buff[size] = '\0';
                    count_ngrams(g_map_n, g_map_n_1, buff, dict_eng, n);
                }
            }
            err_code = archive_read_free(a);
            if (err_code != ARCHIVE_OK) {
                continue;
            }
        } else if (element.second == ".txt" && ext.find(".txt") != string::npos) {
            count_ngrams(g_map_n, g_map_n_1, line, dict_eng, n);
        }
    }
    ::operator delete(buff);
}

std::unordered_map<std::string, int> file_to_dictionary(const std::string &fileName) {
    std::ifstream in(fileName);
    std::unordered_map<std::string, int> dict_eng;

    std::string str;
    while (in >> str) {
        if (!str.empty()) {
            dict_eng.insert({str, 1});
        }
    }
    in.close();
    return dict_eng;
}
