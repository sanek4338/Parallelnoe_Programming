#include <cuda_runtime.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

#define CUDA_CHECK(call)                                                              \
    do {                                                                              \
        cudaError_t err__ = (call);                                                   \
        if (err__ != cudaSuccess) {                                                   \
            throw runtime_error(string("CUDA error: ") + cudaGetErrorString(err__) + \
                                " at " + __FILE__ + ":" + to_string(__LINE__));      \
        }                                                                             \
    } while (0)

// ================= ЧТЕНИЕ =================

vector<vector<double>> readMatrix(const fs::path& filename, int& n) {
    ifstream file(filename);
    if (!file)
        throw runtime_error("Error opening file: " + filename.string());

    file >> n;
    if (n <= 0)
        throw runtime_error("Invalid matrix size");

    vector<vector<double>> matrix(n, vector<double>(n));

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            file >> matrix[i][j];

    return matrix;
}

vector<double> flattenMatrix(const vector<vector<double>>& matrix) {
    int n = static_cast<int>(matrix.size());
    vector<double> flat(static_cast<size_t>(n) * n);

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            flat[(size_t)i * n + j] = matrix[i][j];

    return flat;
}

void writeMatrix(const fs::path& filename,
    const vector<double>& flat,
    int n) {
    ofstream file(filename);
    file << n << "\n";

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            file << flat[(size_t)i * n + j]
            << (j + 1 < n ? ' ' : '\n');
    }
}

// ================= CUDA ЯДРО =================

__global__ void multiplyKernel(const double* A,
    const double* B,
    double* C,
    int n) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (row < n && col < n) {
        double sum = 0.0;
        for (int k = 0; k < n; ++k)
            sum += A[row * n + k] * B[(size_t)k * n + col];

        C[(size_t)row * n + col] = sum;
    }
}

// ================= ЭКСПЕРИМЕНТ =================

void runExperiment(int matrixSize, const fs::path& baseFolder) {
    fs::path folder =
        baseFolder / (to_string(matrixSize) + "x" + to_string(matrixSize));

    int n1 = 0, n2 = 0;

    auto A = readMatrix(folder / "matrixA.txt", n1);
    auto B = readMatrix(folder / "matrixB.txt", n2);

    if (n1 != n2)
        throw runtime_error("Matrix sizes do not match");

    const int n = n1;
    const size_t bytes = (size_t)n * n * sizeof(double);

    vector<double> flatA = flattenMatrix(A);
    vector<double> flatB = flattenMatrix(B);
    vector<double> flatC((size_t)n * n, 0.0);

    double* dA = nullptr, * dB = nullptr, * dC = nullptr;

    CUDA_CHECK(cudaMalloc(&dA, bytes));
    CUDA_CHECK(cudaMalloc(&dB, bytes));
    CUDA_CHECK(cudaMalloc(&dC, bytes));

    CUDA_CHECK(cudaMemcpy(dA, flatA.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(dB, flatB.data(), bytes, cudaMemcpyHostToDevice));

    cudaDeviceProp prop{};
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));

    cout << "\n=== Matrix " << n << "x" << n << " ===\n";
    cout << "CUDA device: " << prop.name << "\n\n";

    vector<pair<int, int>> blockConfigs = {
        {8,8}, {16,8}, {8,16}, {16,16},
        {32,8}, {8,32}, {32,16}, {16,32}, {32,32}
    };

    float bestTime = numeric_limits<float>::max();
    dim3 bestBlock{}, bestGrid{};

    cout << left << setw(12) << "Block"
        << setw(14) << "Grid"
        << setw(14) << "Threads"
        << setw(14) << "Time(ms)" << "\n";
    cout << string(55, '-') << "\n";

    for (auto [bx, by] : blockConfigs) {

        if (bx * by > prop.maxThreadsPerBlock)
            continue;

        dim3 block(bx, by);
        dim3 grid((n + bx - 1) / bx,
            (n + by - 1) / by);

        CUDA_CHECK(cudaMemset(dC, 0, bytes));

        cudaEvent_t start{}, stop{};
        CUDA_CHECK(cudaEventCreate(&start));
        CUDA_CHECK(cudaEventCreate(&stop));

        CUDA_CHECK(cudaEventRecord(start));

        multiplyKernel << <grid, block >> > (dA, dB, dC, n);
        CUDA_CHECK(cudaGetLastError());

        CUDA_CHECK(cudaEventRecord(stop));
        CUDA_CHECK(cudaEventSynchronize(stop));

        float ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));

        CUDA_CHECK(cudaEventDestroy(start));
        CUDA_CHECK(cudaEventDestroy(stop));

        cout << setw(12) << (to_string(bx) + "x" + to_string(by))
            << setw(14) << (to_string(grid.x) + "x" + to_string(grid.y))
            << setw(14) << (bx * by)
            << setw(14) << ms << "\n";

        if (ms < bestTime) {
            bestTime = ms;
            bestBlock = block;
            bestGrid = grid;
        }
    }

    cout << string(55, '-') << "\n";
    cout << "Best configuration: block "
        << bestBlock.x << "x" << bestBlock.y
        << ", time = " << bestTime << " ms\n";

    // Финальный запуск
    multiplyKernel << <bestGrid, bestBlock >> > (dA, dB, dC, n);
    CUDA_CHECK(cudaDeviceSynchronize());

    CUDA_CHECK(cudaMemcpy(flatC.data(), dC, bytes, cudaMemcpyDeviceToHost));

    writeMatrix(folder / "result_cuda.txt", flatC, n);

    CUDA_CHECK(cudaFree(dA));
    CUDA_CHECK(cudaFree(dB));
    CUDA_CHECK(cudaFree(dC));
}

// ================= MAIN =================

int main() {
    try {

#ifndef PROJECT_DIR
#define PROJECT_DIR "D:/t"
#endif

        const fs::path baseFolder =
            fs::path(PROJECT_DIR) / "matrix";

        const int matrixSizes[] = { 200, 400, 800, 1200, 1600 };

        cout << "CUDA Matrix Multiplication Lab\n";

        for (int size : matrixSizes)
            runExperiment(size, baseFolder);

        return 0;
    }
    catch (const exception& ex) {
        cerr << ex.what() << endl;
        return 1;
    }
}