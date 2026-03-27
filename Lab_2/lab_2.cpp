#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <string>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <stdexcept>
#include <omp.h>
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

static vector<vector<double>> loadMatrix(const fs::path& filePath, int& size) {
    ifstream file(filePath);
    if (!file) {
        cerr << "Ошибка открытия файла: " << filePath.string() << "\n";
        throw runtime_error("Не удалось открыть входной файл");
    }

    file >> size;
    if (!file || size <= 0) {
        cerr << "Некорректный формат файла (не удалось прочитать N): " << filePath.string() << "\n";
        throw runtime_error("Некорректный формат входного файла");
    }

    vector<vector<double>> matrix(size, vector<double>(size));

    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            file >> matrix[row][col];
            if (!file) {
                cerr << "Некорректные данные матрицы в файле: " << filePath.string()
                     << " (строка " << (row + 2) << ", столбец " << (col + 1) << ")\n";
                throw runtime_error("Некорректные данные матрицы");
            }
        }
    }

    return matrix;
}

static void saveMatrix(const fs::path& filePath, const vector<vector<double>>& matrix) {
    ofstream file(filePath);
    if (!file) {
        cerr << "Ошибка записи файла: " << filePath.string() << "\n";
        throw runtime_error("Не удалось открыть файл для записи");
    }

    int size = (int)matrix.size();
    file << size << "\n";
    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            file << matrix[row][col];
            if (col + 1 < size) file << ' ';
        }
        file << "\n";
    }
}

static void setCoreAffinity(int cores) {
    DWORD_PTR mask = 0;

    for (int i = 0; i < cores; i++) {
        mask |= (1ULL << i);
    }

    if (!SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        cerr << "Предупреждение: не удалось установить affinity mask для " << cores << " ядер.\n";
    }
}

static vector<vector<double>> multiplyMatricesOpenMP(
    const vector<vector<double>>& first,
    const vector<vector<double>>& second,
    int threads
) {
    int size = (int)first.size();
    vector<vector<double>> result(size, vector<double>(size, 0.0));

    omp_set_num_threads(threads);

#pragma omp parallel for collapse(2)
    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            double sum = 0.0;
            for (int k = 0; k < size; k++) {
                sum += first[row][k] * second[k][col];
            }
            result[row][col] = sum;
        }
    }

    return result;
}

int main() {
    setlocale(LC_ALL, ".UTF-8");
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    const fs::path BASE_FOLDER =
        fs::path(PROJECT_DIR) / u8"перемножение матриц разных размеров";

    const int matrixSizes[] = { 200, 400, 800, 1200, 1600, 2000 };
    const int threadCounts[] = { 1, 2, 4, 8 };
    const int coreCounts[] = { 1, 2, 4, 8 };

    cout << "Текущая рабочая папка: " << fs::current_path().string() << "\n";
    cout << "Базовая папка с матрицами: " << BASE_FOLDER.string() << "\n";
    cout << "Доступно логических процессоров: " << omp_get_num_procs() << "\n\n";

    struct TestResult {
        int size;
        int threads;
        int cores;
        double seconds;
        long long operations;
    };

    vector<TestResult> results;

    for (int expectedSize : matrixSizes) {
        try {
            fs::path currentFolder = BASE_FOLDER / (to_string(expectedSize) + "x" + to_string(expectedSize));
            fs::path firstMatrixFile = currentFolder / "matrixA.txt";
            fs::path secondMatrixFile = currentFolder / "matrixB.txt";

            int size1 = 0, size2 = 0;
            auto firstMatrix = loadMatrix(firstMatrixFile, size1);
            auto secondMatrix = loadMatrix(secondMatrixFile, size2);

            if (size1 != size2) {
                cerr << "Ошибка: размеры матриц не совпадают в папке " << currentFolder.string()
                     << " (первая: " << size1 << ", вторая: " << size2 << ")\n";
                return 1;
            }

            if (size1 != expectedSize) {
                cerr << "Предупреждение: в " << currentFolder.string()
                     << " ожидается N=" << expectedSize << ", но в файле N=" << size1 << "\n";
            }

            for (int cores : coreCounts) {
                if (cores > omp_get_num_procs()) {
                    continue;
                }

                setCoreAffinity(cores);

                for (int threads : threadCounts) {
                    auto startTime = chrono::high_resolution_clock::now();
                    auto resultMatrix = multiplyMatricesOpenMP(firstMatrix, secondMatrix, threads);
                    auto endTime = chrono::high_resolution_clock::now();

                    chrono::duration<double> elapsedTime = endTime - startTime;

                    fs::path resultFile = currentFolder /
                        ("result_" + to_string(size1) + "_t" + to_string(threads) + "_c" + to_string(cores) + ".txt");

                    saveMatrix(resultFile, resultMatrix);

                    long long totalOperations = 1LL * size1 * size1 * size1;
                    results.push_back({ size1, threads, cores, elapsedTime.count(), totalOperations });

                    cout << "OK: "
                         << size1 << "x" << size1
                         << " | потоков=" << threads
                         << " | ядер=" << cores
                         << " | операций=" << totalOperations
                         << " | время=" << fixed << setprecision(6) << elapsedTime.count() << " сек"
                         << " | сохранено: " << resultFile.string() << "\n";
                }
            }

            cout << "\n";
        }
        catch (const exception& e) {
            cerr << "\nСбой при обработке N=" << expectedSize << ": " << e.what() << "\n";
            cerr << "Проверь, что файлы существуют по пути:\n"
                 << "  " << (BASE_FOLDER / (to_string(expectedSize) + "x" + to_string(expectedSize)) / "matrixA.txt").string() << "\n"
                 << "  " << (BASE_FOLDER / (to_string(expectedSize) + "x" + to_string(expectedSize)) / "matrixB.txt").string() << "\n";
            return 1;
        }
    }

    cout << "\n=== Итоговая таблица ===\n";
    cout << left
         << setw(10) << "Размер"
         << setw(10) << "Потоки"
         << setw(10) << "Ядра"
         << setw(18) << "Операций(N^3)"
         << "Время(сек)\n";

    for (const auto& result : results) {
        cout << left
             << setw(10) << result.size
             << setw(10) << result.threads
             << setw(10) << result.cores
             << setw(18) << result.operations
             << fixed << setprecision(6) << result.seconds << "\n";
    }

    return 0;
}