#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sys/resource.h>
#include <sys/time.h>

using namespace std;
using namespace chrono;

// Заглушки для "пинга"
long measure_tcp_ping() {
    auto start = high_resolution_clock::now();
    this_thread::sleep_for(milliseconds(20)); // Имитация задержки
    auto end = high_resolution_clock::now();
    return duration_cast<milliseconds>(end - start).count();
}

long measure_grpc_ping() {
    auto start = high_resolution_clock::now();
    this_thread::sleep_for(milliseconds(12)); // Имитация меньшей задержки
    auto end = high_resolution_clock::now();
    return duration_cast<milliseconds>(end - start).count();
}

// Чтение информации о памяти из /proc/self/status
size_t get_memory_kb() {
    ifstream status("/proc/self/status");
    string line;
    while (getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            size_t kb;
            sscanf(line.c_str(), "VmRSS: %zu kB", &kb);
            return kb;
        }
    }
    return 0;
}

// Генерация графика Gnuplot
void generate_gnuplot_image() {
    FILE* gnuplot = popen("gnuplot -persistent", "w");
    if (gnuplot) {
        fprintf(gnuplot, "set terminal png size 800,600\n");
        fprintf(gnuplot, "set output 'results/benchmark.png'\n");
        fprintf(gnuplot, "set title 'Latency Comparison (ms)'\n");
        fprintf(gnuplot, "set style data histograms\n");
        fprintf(gnuplot, "set style fill solid 1.0 border -1\n");
        fprintf(gnuplot, "set xlabel 'Protocol'\n");
        fprintf(gnuplot, "set ylabel 'Latency (ms)'\n");
        fprintf(gnuplot, "plot 'results/latency.dat' using 2:xtic(1) title ''\n");
        pclose(gnuplot);
    }
}

// ASCII-граф
void print_ascii_chart(const string& label, long value) {
    cout << label << " [";
    for (int i = 0; i < value; ++i) cout << "=";
    cout << "] " << value << " ms" << endl;
}

int main() {
    cout << "[Benchmark] Starting latency tests..." << endl;

    long tcp_ping = measure_tcp_ping();
    long grpc_ping = measure_grpc_ping();

    // Сохраняем в файл для графика
    ofstream out("results/latency.dat");
    out << "TCP " << tcp_ping << endl;
    out << "gRPC " << grpc_ping << endl;
    out.close();

    // Печатаем в текстовом виде
    cout << "\nLatency Report:\n";
    print_ascii_chart("TCP  ", tcp_ping);
    print_ascii_chart("gRPC ", grpc_ping);

    // Получаем память
    size_t mem_kb = get_memory_kb();
    cout << "\nCurrent Memory Usage (VmRSS): " << mem_kb << " KB" << endl;

    // Генерируем PNG через gnuplot
    generate_gnuplot_image();
    cout << "\n[Benchmark] PNG chart saved to results/benchmark.png" << endl;

    return 0;
}
