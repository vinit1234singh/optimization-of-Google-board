#include "prediction.h"


bool contains(const std::unordered_map<std::string, std::vector<std::string>> &map, const std::string &key) {
    return !(map.find(key) == map.end());
}
std::string join(const std::vector<std::string> &v) {
    std::string joined_v;
    for (size_t i = 0; i < v.size(); ++i) {
        joined_v += v[i];
        if (i != v.size() - 1) {
            joined_v += " ";
        }
    }
    return joined_v;
}
std::unordered_map<std::string, double> file_to_probabilities_map(const std::string &filename) {
    std::fstream infile(filename);
    std::unordered_map<std::string, double> probabilities_map;

    std::string line;
    while (std::getline(infile, line)) {
        std::vector<std::string> result;
        boost::algorithm::split(result, line, boost::is_any_of(":"));
        probabilities_map[result[0]] = std::stod(result[result.size()-1]);
    }
    return probabilities_map;
}
void string_to_probabilities_map_parallel(oneapi::tbb::concurrent_hash_map<std::string, double> &probabilities_map,std::vector<std::string> &probabilities_split,size_t thread_num, size_t lines_per_thread) {
    for (size_t i = thread_num * lines_per_thread; i <= (thread_num + 1) * lines_per_thread; ++i) {
        std::vector<std::string> line;
        boost::algorithm::split(line, probabilities_split[i], boost::is_any_of(":"));
        oneapi::tbb::concurrent_hash_map<std::string, double>::accessor a;
        if (line.size() != 1) {
            probabilities_map.insert(a, line[0].data());
            a->second = std::stod(line[line.size()-1].data());
        }
    }
}
void string_to_next_words_map_parallel(oneapi::tbb::concurrent_hash_map<std::string, std::vector<std::string>> &words_map,std::vector<std::string> &words_split,size_t thread_num, size_t lines_per_thread) {
    auto from = thread_num * lines_per_thread;
    auto to = (thread_num + 1) * lines_per_thread;
    std::unordered_map<std::string, std::vector<std::string>> m;
    for (size_t i = from; i < to; ++i) {
        std::vector<std::string> line;
        boost::algorithm::split(line, words_split[i], boost::is_any_of(":"));
        tbb::concurrent_hash_map<std::string, std::vector<std::string>>::accessor a;
        m[line[0]].emplace_back(line[1]);
        words_map.insert(a, line[0].data());
        a->second.emplace_back(line[line.size()-1].data());
    }
}
std::unordered_map<std::string, std::vector<std::string>> file_to_next_words_map(const std::string &filename) {
    std::unordered_map<std::string, std::vector<std::string>> words_map;
    std::ifstream infile(filename);
    std::string line;
    while (std::getline(infile, line)) {
        std::vector<std::string> result;
        boost::algorithm::split(result, line, boost::is_any_of(":"));
        words_map[result[0]].emplace_back(result[1]);
    }
    return words_map;
}
std::vector<std::string> predict_next_word(const std::string &phrase, std::unordered_map<std::string, double> &prob_map,std::unordered_map<std::string, std::vector<std::string>> &next_words_map,size_t words_n) {
    auto normalized_phrase = boost::locale::fold_case(boost::locale::normalize(phrase));
    std::vector<std::string> predicted_words;
    if (contains(next_words_map, normalized_phrase)) {
        std::vector<double> words_probability(words_n);
        for (const auto &str: next_words_map[normalized_phrase]) {
            std::string current_str = normalized_phrase;
            current_str.append(" ").append(str);
            if (predicted_words.size() >= words_n) {
                double min_prob = *min_element(words_probability.begin(), words_probability.end());
                if (prob_map[current_str] > 0) {
                    auto index_of_min = std::find(words_probability.begin(), words_probability.end(), min_prob) - words_probability.begin();
                    words_probability[index_of_min] = prob_map[current_str];
                    if (str != "<unk>")
                    predicted_words[index_of_min] = str;
                }
            } else if (std::find(predicted_words.begin(), predicted_words.end(), str) == predicted_words.end()) {
                if (str != "<unk>") predicted_words.emplace_back(str);
            }
        }
    } else {
        return predicted_words;
    }
    return predicted_words;
}