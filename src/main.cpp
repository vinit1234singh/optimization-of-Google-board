
#include <iostream>
#include <unordered_map>
#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <iterator>

#include "../indexing/myqueue.hpp"
#include "../parser/parser.h"
#include "../indexing/processing.h"
#include "../prediction/prediction.h"
#include "oneapi/tbb/concurrent_hash_map.h"
#include "main.h"


using std::vector;
using std::string;
using std::cin;
using std::cout;
using std::endl;
using std::ref;
using std::unordered_map;
using std::filesystem::path;


void write_to_file(const std::unordered_map<std::string, double> &m, std::ofstream &prob, std::ofstream &next_words, int n) {
    for (auto &p: m) {
        prob << p.first << ":" << p.second << std::endl;
        next_words << get_first_n_words(n - 1, p.first) << ":" << p.first.substr(p.first.find_last_of(' ') + 1)<< std::endl;
    }
}

int main(int argc, char *argv[]) {
    boost::locale::localization_backend_manager lbm = boost::locale::localization_backend_manager::global();
    lbm.select("icu");
    boost::locale::generator gen;
    std::locale::global(gen("en_US.UTF-8"));
    enum OUT_CODES {
        ALL_GOOD,
        WRONG_AMOUNT_OF_ARGUMENTS,
        INVALID_ARGUMENTS,
        FAILED_TO_OPEN_CONFIG_FILE,
        ERROR_IN_CONFIG_FILE
    };
    if (argc < 1) {
        std::cerr << "ERROR: wrong amount of arguments" << endl;
        return WRONG_AMOUNT_OF_ARGUMENTS;
    }
    string filename;
    if (argc >= 2) {
        filename = argv[1];
    } else if (argc == 1) {
        filename = "/home/chintu/Project/Ngram/index.cfg";
    } else {
        std::cerr << "ERROR wrong amount of arguments" << endl;
        return WRONG_AMOUNT_OF_ARGUMENTS;
    }
    std::ifstream cf(filename);
    if (!cf.is_open()) {
        std::cerr << "ERROR failed to open config file" << endl;
        return FAILED_TO_OPEN_CONFIG_FILE;
    }
    params parsed_cfg;
    try {
        parsed_cfg = fill_params(cf);
    }
    catch (std::exception &ex) {
        std::cerr << "ERROR " << ex.what() << endl;
        return ERROR_IN_CONFIG_FILE;
    }
    std::unordered_map<std::string, int> dict_eng = file_to_dictionary(parsed_cfg.dictionary);
    if (parsed_cfg.option == 0) {
        std::string greetings = "PDC project\nTraining mode:\n\n";
        cout << greetings << endl;
        safe_que<path> files_que(parsed_cfg.files_queue_s);
        safe_que<string> string_que(parsed_cfg.strings_queue_s);
        safe_que<unordered_map<string, int>> merge_q_n(parsed_cfg.merge_queue_s, true);
        safe_que<unordered_map<string, int>> merge_q_n_1(parsed_cfg.merge_queue_s, true);
        tbb::concurrent_hash_map<string, int> g_map_n;
        tbb::concurrent_hash_map<string, int> g_map_n_1;
        path empty_p;
        string empty_s;
        auto full_time_start = get_current_time_fenced();
        auto find_time_start = get_current_time_fenced();

        for (const auto &file: std::filesystem::recursive_directory_iterator(parsed_cfg.indir)) {
            files_que.push_end(file.path(), 1, "path");
        }
        files_que.push_end(empty_p, 1, "path");
        auto find_time = get_current_time_fenced() - find_time_start;
        std::vector<std::thread> main_flows(parsed_cfg.index_threads);

        for (int i = 0; i < parsed_cfg.index_threads; ++i) {
            main_flows.emplace_back(index_string, ref(string_que), ref(g_map_n), ref(g_map_n_1), ref(dict_eng),std::ref(parsed_cfg.extensions), parsed_cfg.ngram_par);
        }
        auto read_time_start = get_current_time_fenced();
        while (files_que.get_size() > 0) {
            auto file_p = files_que.pop().first;
            if (file_p.empty()) {
                string_que.push_end(empty_s, 1, "Error");
                break;
            }
            if (file_p.extension() == ".DS_Store") {
                continue;
            }
            std::ifstream raw_file(file_p, std::ios::binary);
            std::ostringstream buffer_ss;
            buffer_ss << raw_file.rdbuf();
            std::string buffer{buffer_ss.str()};
            if (buffer.empty() || raw_file.tellg() > 10485760) {
                continue;
            }
            string_que.push_end(buffer, buffer.size(), file_p.extension().string());
        }
        auto read_time = get_current_time_fenced() - read_time_start;
        for (auto &t: main_flows) {
            if (t.joinable()) {
                t.join();
            }
        }
        unordered_map<string, int> f_map_n(g_map_n.begin(), g_map_n.end());
        unordered_map<string, int> f_map_n_1(g_map_n_1.begin(), g_map_n_1.end());
        auto full_time = get_current_time_fenced() - full_time_start;
        auto write_time_start = get_current_time_fenced();
        unordered_map<string, double> prob_map;
        count_probabilities(prob_map, f_map_n, f_map_n_1, parsed_cfg.ngram_par);
        std::ofstream of_prob(parsed_cfg.out_prob);
        std::ofstream of_next_words(parsed_cfg.out_ngram);
        write_to_file(prob_map, of_prob, of_next_words, parsed_cfg.ngram_par);
        auto write_time = get_current_time_fenced() - write_time_start;
        cout << "Training Done!" << endl;
        cout << "---------------------------------------" << endl;
        cout << "The total time taken: " << to_us(full_time) << endl;
        cout << "\t  Read time:   " << to_us(read_time) << endl;
        cout << "\t  Find time:   " << to_us(find_time) << endl;
        cout << "\t  Write time:  " << to_us(write_time) << endl;
        cout << "Completed!!" << endl;
    } else if (parsed_cfg.option == 1) {
        std::string greetings = "PDC project\nPrediction mode:\n\n";
        cout << greetings << endl;

        cout << "Predicting:" << endl;
        std::unordered_map<std::string, double> prob_map;
        std::unordered_map<std::string, std::vector<std::string>> next_words_map;

        auto prediction_time_start = get_current_time_fenced();
        if (parsed_cfg.pred_threads == 0) {
        } else {
            prob_map = file_to_probabilities_map(parsed_cfg.out_prob);
            next_words_map = file_to_next_words_map(parsed_cfg.out_ngram);

            std::ifstream out_prob(parsed_cfg.out_prob, std::ios::binary);
            std::ostringstream buffer_ss;
            buffer_ss << out_prob.rdbuf();
            std::string probabilities{buffer_ss.str()};

            std::ifstream out_ngram(parsed_cfg.out_ngram, std::ios::binary);
            std::ostringstream buffer_s;
            buffer_s << out_ngram.rdbuf();
            std::string ngram{buffer_s.str()};

            std::vector<std::string> probabilities_split;
            std::vector<std::string> ngram_split;
            boost::algorithm::split(probabilities_split, probabilities, boost::is_any_of("\n"));
            boost::algorithm::split(ngram_split, ngram, boost::is_any_of("\n"));
            ngram_split.pop_back();
            probabilities_split.pop_back();

            size_t n = ngram_split.size();
            size_t lines_per_thread = std::floor(n / parsed_cfg.pred_threads);
            oneapi::tbb::concurrent_hash_map<std::string, std::vector<std::string>> words_maps;
            oneapi::tbb::concurrent_hash_map<std::string, double> probability_maps;


            std::vector<std::thread> processing_flows(parsed_cfg.pred_threads);

            for (size_t i = 0; i < parsed_cfg.pred_threads; ++i) {
                processing_flows.emplace_back(string_to_next_words_map_parallel, ref(words_maps), ref(ngram_split), i,lines_per_thread);
            }

            for (size_t i = 0; i < parsed_cfg.pred_threads; ++i) {
                processing_flows.emplace_back(string_to_probabilities_map_parallel, ref(probability_maps),ref(probabilities_split), i, lines_per_thread);
            }

            for (auto &th: processing_flows) {
                if (th.joinable()) {
                    th.join();
                }
            }

        }
        auto prediction_time = get_current_time_fenced() - prediction_time_start;
        cout << "-------------------------------" << endl;
        cout << "The time taken for processing: " << to_us(prediction_time) << endl;
        cout << "--------------------------------" << endl;
        cout << "\nPrediction:" << endl;

        std::string end_punctuation = ".!?";
        std::string continue_punctuation = ",:;\"'";
        int n = parsed_cfg.ngram_par - 1;
        std::vector<std::string> last_n_inputs;
        while (n--) {
            last_n_inputs.emplace_back("<s>");
        }
        std::string current_input = join(last_n_inputs);
        while (true) {
            auto predicted_words = predict_next_word(join(last_n_inputs), prob_map, next_words_map,parsed_cfg.word_num);
            if (predicted_words.empty() && last_n_inputs != std::vector<std::string>{"<s>", "<s>"}) {
                if (last_n_inputs[last_n_inputs.size() - 2] != "<s>") {
                    std::fill(last_n_inputs.begin(), last_n_inputs.end() - 1, "<s>");
                }
                continue;
            }
            for (const auto &el: predicted_words) {
                cout << el << " ";
            }
            cout << endl;
            last_n_inputs.erase(last_n_inputs.begin());
            cout << "->\t";
            cin >> current_input;
            boost::trim(current_input);
            if (current_input == "///") {
                break;
            }
            if (end_punctuation.find(current_input) != std::string::npos) {
                last_n_inputs.emplace_back("</s>");
                last_n_inputs.erase(last_n_inputs.begin());
                last_n_inputs.emplace_back("<s>");
                continue;
            } else if (continue_punctuation.find(current_input) != std::string::npos) {
                continue;
            }
            if (dict_eng.find(current_input) == dict_eng.end()) {
                current_input = "<unk>";
            }
            last_n_inputs.emplace_back(current_input);
        }
        cout << "Finish!" << endl;
    } else {
        std::cerr << "Wrong mode: need 0 for 'train' or 1 for 'predict'" << endl;
        return INVALID_ARGUMENTS;
    }
    return ALL_GOOD;
}
