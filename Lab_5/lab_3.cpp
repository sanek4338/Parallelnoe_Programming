#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <string>
#include <filesystem>
#include <iomanip>
#include <mpi.h>
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
    vector<vector<double>> m(n, vector<double>(n));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            file >> m[i][j];
    return m;
}

void writeMatrix(const fs::path& filename, const vector<vector<double>>& m) {
    ofstream file(filename);
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

void runExperiment(MPI_Comm comm, int expectedSize, const fs::path& BASE_FOLDER) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    int n = 0;
    vector<vector<double>> A, BT;
    vector<double> flatBT;

    if (rank == 0) {
        fs::path currentFolder = BASE_FOLDER / (to_string(expectedSize) + "x" + to_string(expectedSize));
        int n1, n2;
        A = readMatrix(currentFolder / "matrixA.txt", n1);
        auto B = readMatrix(currentFolder / "matrixB.txt", n2);
        n = n1;
        BT = transpose(B);

        flatBT.resize(n * n);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                flatBT[i * n + j] = BT[i][j];
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, comm);

    int rowsPerProc = n / size;
    int remainder = n % size;

    vector<int> sendCounts(size), displs(size);
    int offset = 0;
    for (int i = 0; i < size; i++) {
        int rows = rowsPerProc + (i < remainder ? 1 : 0);
        sendCounts[i] = rows * n;
        displs[i] = offset;
        offset += rows * n;
    }

    int localRows = rowsPerProc + (rank < remainder ? 1 : 0);
    vector<double> localA(localRows * n);
    vector<double> localC(localRows * n);

    if (rank != 0) flatBT.resize(n * n);
    MPI_Bcast(flatBT.data(), n * n, MPI_DOUBLE, 0, comm);

    vector<double> flatA;
    if (rank == 0) {
        flatA.resize(n * n);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                flatA[i * n + j] = A[i][j];
    }

    MPI_Scatterv(flatA.data(), sendCounts.data(), displs.data(), MPI_DOUBLE,
        localA.data(), localRows * n, MPI_DOUBLE, 0, comm);

    MPI_Barrier(comm);
    auto start = chrono::high_resolution_clock::now();

    for (int i = 0; i < localRows; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += localA[i * n + k] * flatBT[j * n + k];
            }
            localC[i * n + j] = sum;
        }
    }

    MPI_Barrier(comm);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;

    vector<double> flatC;
    if (rank == 0) flatC.resize(n * n);

    MPI_Gatherv(localC.data(), localRows * n, MPI_DOUBLE,
        flatC.data(), sendCounts.data(), displs.data(), MPI_DOUBLE, 0, comm);

    if (rank == 0) {
        cout << "Matrix " << n << "x" << n
            << " | Procs: " << setw(2) << size
            << " | Time: " << fixed << setprecision(6) << elapsed.count() << " sec" << endl;

        vector<vector<double>> C(n, vector<double>(n));
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                C[i][j] = flatC[i * n + j];

        fs::path currentFolder = BASE_FOLDER / (to_string(expectedSize) + "x" + to_string(expectedSize));
        writeMatrix(currentFolder / ("result_mpi_" + to_string(size) + ".txt"), C);
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int worldRank, worldSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    if (worldRank == 0) {
        SetConsoleCP(65001);
        SetConsoleOutputCP(65001);
    }

#ifndef PROJECT_DIR
#define PROJECT_DIR "D:/t"
#endif

    const fs::path BASE_FOLDER = fs::path(PROJECT_DIR) / u8"matrix";
    const int matrixSizes[] = { 200, 400, 800, 1200, 1600 };
    const int procCounts[] = { 1, 2, 4, 8 };

    if (worldRank == 0) {
        cout << "Base folder: " << BASE_FOLDER.string() << "\n";
        cout << "Total MPI processes launched: " << worldSize << "\n\n";
    }

    for (int expectedSize : matrixSizes) {
        if (worldRank == 0) {
            cout << "=== Matrix size: " << expectedSize << " ===\n";
        }

        for (int np : procCounts) {
            if (np > worldSize) {
                if (worldRank == 0)
                    cout << "Skip np=" << np << " (launched only " << worldSize << ")\n";
                continue;
            }

            int color = (worldRank < np) ? 0 : MPI_UNDEFINED;
            MPI_Comm subComm;
            MPI_Comm_split(MPI_COMM_WORLD, color, worldRank, &subComm);

            if (subComm != MPI_COMM_NULL) {
                runExperiment(subComm, expectedSize, BASE_FOLDER);
                MPI_Comm_free(&subComm);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }

        if (worldRank == 0) cout << "\n";
    }

    MPI_Finalize();
    return 0;
}