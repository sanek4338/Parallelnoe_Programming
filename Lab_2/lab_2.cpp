#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <string>
#include <filesystem>
#include <iomanip>
#include <omp.h>
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

vector<vector<double>> readMatrix(const fs::path& filename, int& n) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error opening file: " << filename.string() << "\n";
        exit(1);
    }

    file >> n;
    if (!file || n <= 0) {
        cerr << "Bad matrix size in file: " << filename.string() << "\n";
        exit(1);
    }

    vector<vector<double>> m(n, vector<double>(n));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            file >> m[i][j];

    if (!file) {
        cerr << "Bad matrix data in file: " << filename.string() << "\n";
        exit(1);
    }

    return m;
}

void writeMatrix(const fs::path& filename, const vector<vector<double>>& m) {
    ofstream file(filename);
    if (!file) {
        cerr << "Error writing file: " << filename.string() << "\n";
        exit(1);
    }

    int n = (int)m.size();
    file << n << "\n";
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            file << m[i][j] << (j + 1 < n ? ' ' : '\n');
        }
    }
}

vector<vector<double>> transpose(const vector<vector<double>>& m) {
    int n = (int)m.size();
    vector<vector<double>> t(n, vector<double>(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            t[j][i] = m[i][j];
    return t;
}

void setCoreAffinity(int cores) {
    DWORD_PTR mask = 0;
    for (int i = 0; i < cores; i++) mask |= (1ULL << i);

    if (!SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        cerr << "Warning: failed to set affinity for " << cores << " cores.\n";
    }
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    const fs::path BASE_FOLDER =
        fs::path(PROJECT_DIR) / u8"matrix";

    const int matrixSizes[] = { 200, 400, 800, 1200};
    const int threadCounts[] = { 1, 2, 4, 8 };
    const int coreCounts[] = { 1, 2, 4, 8};

    cout << "Current working folder: " << fs::current_path().string() << "\n";
    cout << "Base folder: " << BASE_FOLDER.string() << "\n";
    cout << "Available logical processors: " << omp_get_num_procs() << "\n\n";

    for (int expectedSize : matrixSizes) {
        fs::path currentFolder = BASE_FOLDER / (to_string(expectedSize) + "x" + to_string(expectedSize));
        fs::path fileA = currentFolder / "matrixA.txt";
        fs::path fileB = currentFolder / "matrixB.txt";

        int n1 = 0, n2 = 0;
        auto A = readMatrix(fileA, n1);
        auto B = readMatrix(fileB, n2);

        if (n1 != n2) {
            cerr << "Matrix sizes do not match in: " << currentFolder.string()
                << " (A=" << n1 << ", B=" << n2 << ")\n";
            return 1;
        }

        int n = n1;
        if (n != expectedSize) {
            cerr << "Warning: expected " << expectedSize << " but file has " << n << "\n";
        }

        // Чтобы умножение было быстрее и лучше масштабировалось:
        // читаем B по строкам (для этого транспонируем один раз)
        auto BT = transpose(B);

        cout << "=== Matrix " << n << "x" << n << " (" << currentFolder.string() << ") ===\n";

        for (int cores : coreCounts) {
            if (cores > omp_get_num_procs()) continue;

            setCoreAffinity(cores);

            for (int threads : threadCounts) {
                if (threads > cores) continue; // важно: не плодим потоков больше, чем разрешённых ядер

                omp_set_dynamic(0);
                omp_set_num_threads(threads);

                vector<vector<double>> C(n, vector<double>(n, 0.0));

                auto start = chrono::high_resolution_clock::now();

#pragma omp parallel for schedule(static)
                for (int i = 0; i < n; i++) {
                    for (int j = 0; j < n; j++) {
                        double sum = 0.0;
                        for (int k = 0; k < n; k++) {
                            sum += A[i][k] * BT[j][k]; // BT[j][k] вместо B[k][j]
                        }
                        C[i][j] = sum;
                    }
                }

                auto end = chrono::high_resolution_clock::now();
                chrono::duration<double> elapsed = end - start;

                cout << "Threads: " << setw(2) << threads
                    << " | Cores: " << setw(2) << cores
                    << " | Time: " << fixed << setprecision(6) << elapsed.count() << " sec\n";

                // Если нужно сохранять результаты как в исходном коде:
                fs::path outFile = currentFolder /
                    ("result_" + to_string(n) + "_t" + to_string(threads) + "_c" + to_string(cores) + ".txt");
                writeMatrix(outFile, C);
            }
        }

        cout << "\n";
    }

    return 0;
}