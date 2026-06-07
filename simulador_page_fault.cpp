#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Frame {
    int pid = -1;
    int page = -1;
    int last_used = -1;
};

struct Process {
    int id = -1;
    std::string name;
    int arrival_time = 0;
    std::vector<int> pages;
    std::size_t next_page_index = 0;
    int page_faults = 0;
    int completion_time = -1;
    int quantum_used = 0;
    int blocked_until = -1;
    std::unordered_map<int, int> page_to_frame;
};

struct EvictionInfo {
    bool happened = false;
    int evicted_pid = -1;
    int evicted_page = -1;
};

class MemoryManager {
public:
    explicit MemoryManager(int ram_size) : frames_(ram_size) {}

    bool has_page(const Process& process, int page) const {
        return process.page_to_frame.find(page) != process.page_to_frame.end();
    }

    void touch(Process& process, int page, int time) {
        const int frame_index = process.page_to_frame.at(page);
        frames_[frame_index].last_used = time;
    }

    EvictionInfo load_page(std::vector<Process>& processes, int pid, int page, int time) {
        EvictionInfo eviction;
        int target_frame = find_free_frame();

        if (target_frame == -1) {
            target_frame = find_lru_frame();
            eviction.happened = true;
            eviction.evicted_pid = frames_[target_frame].pid;
            eviction.evicted_page = frames_[target_frame].page;

            Process& victim = processes[eviction.evicted_pid];
            victim.page_to_frame.erase(eviction.evicted_page);
        }

        frames_[target_frame].pid = pid;
        frames_[target_frame].page = page;
        frames_[target_frame].last_used = time;
        processes[pid].page_to_frame[page] = target_frame;

        return eviction;
    }

private:
    std::vector<Frame> frames_;

    int find_free_frame() const {
        for (std::size_t i = 0; i < frames_.size(); ++i) {
            if (frames_[i].pid == -1) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int find_lru_frame() const {
        int frame_index = -1;
        int oldest_time = std::numeric_limits<int>::max();

        for (std::size_t i = 0; i < frames_.size(); ++i) {
            if (frames_[i].last_used < oldest_time) {
                oldest_time = frames_[i].last_used;
                frame_index = static_cast<int>(i);
            }
        }

        return frame_index;
    }
};

struct SimulationInput {
    int quantum = 0;
    int io_penalty = 0;
    int ram_size = 0;
    std::vector<Process> processes;
};

std::vector<int> parse_pages(const std::string& pages_text) {
    std::vector<int> pages;
    std::stringstream ss(pages_text);
    std::string token;

    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            pages.push_back(std::stoi(token));
        }
    }

    return pages;
}

bool parse_input(std::istream& input, SimulationInput& data) {
    if (!(input >> data.quantum >> data.io_penalty >> data.ram_size)) {
        return false;
    }

    int arrival = 0;
    std::string name;
    std::string pages_text;
    int pid = 0;

    while (input >> arrival >> name >> pages_text) {
        Process process;
        process.id = pid++;
        process.name = std::move(name);
        process.arrival_time = arrival;
        process.pages = parse_pages(pages_text);
        data.processes.push_back(std::move(process));
    }

    std::sort(data.processes.begin(), data.processes.end(), [](const Process& a, const Process& b) {
        if (a.arrival_time != b.arrival_time) {
            return a.arrival_time < b.arrival_time;
        }
        return a.id < b.id;
    });

    for (std::size_t i = 0; i < data.processes.size(); ++i) {
        data.processes[i].id = static_cast<int>(i);
    }

    return data.quantum > 0 && data.ram_size > 0 && !data.processes.empty();
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    SimulationInput input_data;
    std::ifstream file;

    if (argc > 1) {
        file.open(argv[1]);
        if (!file.is_open() || !parse_input(file, input_data)) {
            std::cerr << "Erro: nao foi possivel ler o arquivo de entrada.\n";
            return 1;
        }
    } else {
        if (!parse_input(std::cin, input_data)) {
            std::cerr << "Erro: entrada invalida.\n";
            return 1;
        }
    }

    MemoryManager memory(input_data.ram_size);
    std::vector<Process> processes = input_data.processes;
    std::deque<int> ready_queue;
    std::vector<int> blocked;
    std::vector<std::string> logs;

    int time = 0;
    int next_arrival = 0;
    int finished = 0;
    int running_pid = -1;

    auto log = [&](const std::string& message) {
        logs.push_back("[Tempo " + std::to_string(time) + "] " + message);
    };

    while (finished < static_cast<int>(processes.size())) {
        while (next_arrival < static_cast<int>(processes.size()) &&
               processes[next_arrival].arrival_time <= time) {
            ready_queue.push_back(next_arrival);
            log("Processo " + processes[next_arrival].name + " chegou e entrou na fila de prontos");
            ++next_arrival;
        }

        std::vector<int> still_blocked;
        for (int pid : blocked) {
            if (processes[pid].blocked_until <= time) {
                ready_queue.push_back(pid);
                log("Processo " + processes[pid].name + " saiu de bloqueado e voltou para prontos");
            } else {
                still_blocked.push_back(pid);
            }
        }
        blocked.swap(still_blocked);

        if (running_pid == -1 && !ready_queue.empty()) {
            running_pid = ready_queue.front();
            ready_queue.pop_front();
            processes[running_pid].quantum_used = 0;
            log("Escalonador RR selecionou " + processes[running_pid].name + " para CPU");
        }

        if (running_pid != -1) {
            Process& p = processes[running_pid];
            const int requested_page = p.pages[p.next_page_index];

            if (!memory.has_page(p, requested_page)) {
                ++p.page_faults;
                log(p.name + " sofreu Page Fault na pagina " + std::to_string(requested_page));

                const EvictionInfo eviction = memory.load_page(processes, running_pid, requested_page, time);
                if (eviction.happened) {
                    log("LRU removeu pagina " + std::to_string(eviction.evicted_page) +
                        " de " + processes[eviction.evicted_pid].name + " para carregar pagina " +
                        std::to_string(requested_page) + " de " + p.name);
                } else {
                    log("Pagina " + std::to_string(requested_page) + " de " + p.name + " carregada na RAM");
                }

                p.blocked_until = time + input_data.io_penalty;
                blocked.push_back(running_pid);
                log(p.name + " movido para bloqueado ate o tempo " + std::to_string(p.blocked_until));
                running_pid = -1;
            } else {
                memory.touch(p, requested_page, time);
                ++p.next_page_index;
                ++p.quantum_used;
                log(p.name + " executou acesso da pagina " + std::to_string(requested_page) + " (RAM hit)");

                if (p.next_page_index >= p.pages.size()) {
                    p.completion_time = time + 1;
                    ++finished;
                    log("Processo " + p.name + " finalizou");
                    running_pid = -1;
                } else if (p.quantum_used >= input_data.quantum) {
                    ready_queue.push_back(running_pid);
                    log("Quantum expirou para " + p.name + ", processo preemptado para o fim da fila");
                    running_pid = -1;
                }
            }
        } else {
            log("CPU ociosa");
        }

        ++time;
    }

    std::cout << "=== Relatorio Final ===\n";
    std::cout << "Quantum: " << input_data.quantum << "\n";
    std::cout << "Penalidade IO: " << input_data.io_penalty << "\n";
    std::cout << "Tamanho RAM: " << input_data.ram_size << "\n\n";

    std::cout << "Tempo de retorno por processo:\n";
    for (const Process& p : processes) {
        std::cout << "- " << p.name << ": " << (p.completion_time - p.arrival_time) << "\n";
    }

    std::cout << "\nTotal de page faults por processo:\n";
    for (const Process& p : processes) {
        std::cout << "- " << p.name << ": " << p.page_faults << "\n";
    }

    std::cout << "\nLog de execucao:\n";
    for (const std::string& entry : logs) {
        std::cout << entry << "\n";
    }

    return 0;
}
