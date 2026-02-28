import numpy as np
from pathlib import Path

BASE_DIR = Path("перемножение матриц разных размеров")
SIZES = [200, 400, 800, 1200, 1600]

def read_matrix(path: Path) -> np.ndarray:
    with path.open("r", encoding="utf-8") as f:
        n = int(f.readline().strip())
        data = [list(map(float, f.readline().split())) for _ in range(n)]
    return np.array(data, dtype=np.float64)

def verify_one(n: int) -> tuple[bool, float]:
    folder = BASE_DIR / f"{n}x{n}"
    a_path = folder / "matrixA.txt"
    b_path = folder / "matrixB.txt"
    c_path = folder / "result.txt"

    # Проверка существования файлов
    for p in (a_path, b_path, c_path):
        if not p.exists():
            raise FileNotFoundError(f"Не найден файл: {p}")

    A = read_matrix(a_path)
    B = read_matrix(b_path)
    C = read_matrix(c_path)

    if A.shape != (n, n) or B.shape != (n, n) or C.shape != (n, n):
        raise ValueError(
            f"Неверные размеры матриц в {folder}. "
            f"A={A.shape}, B={B.shape}, C={C.shape}, ожидалось {(n, n)}"
        )

    C_true = A @ B
    ok = np.allclose(C, C_true)

    if ok:
        return True, 0.0
    diff = float(np.max(np.abs(C - C_true)))
    return False, diff

def main():
    print(f"Базовая папка: {BASE_DIR.resolve()}\n")

    all_ok = True
    for n in SIZES:
        try:
            ok, diff = verify_one(n)
            if ok:
                print(f"{n}x{n}: OK")
            else:
                all_ok = False
                print(f"{n}x{n}: FAIL | max_abs_diff = {diff}")
        except Exception as e:
            all_ok = False
            print(f"{n}x{n}: ERROR | {e}")

    print("\nИТОГ:", "ВСЁ КОРРЕКТНО" if all_ok else "ЕСТЬ ОШИБКИ")

if __name__ == "__main__":
    main()