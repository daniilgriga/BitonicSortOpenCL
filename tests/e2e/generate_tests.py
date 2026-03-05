#!/usr/bin/env python3
from __future__ import annotations

import os
import random
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SEED = 20260303


def write_test(name: str, data: list[int]) -> None:
    n = len(data)
    dat_path = os.path.join(SCRIPT_DIR, f"{name}.dat")
    ans_path = os.path.join(SCRIPT_DIR, f"{name}.ans")

    with open(dat_path, "w") as f:
        f.write(f"{n}\n")
        if n > 0:
            f.write(" ".join(str(x) for x in data) + "\n")

    with open(ans_path, "w") as f:
        if n > 0:
            f.write(" ".join(str(x) for x in sorted(data)) + "\n")


def random_array(n: int, lo: int = -1_000_000, hi: int = 1_000_000) -> list[int]:
    return [rng.randint(lo, hi) for _ in range(n)]


rng = random.Random(SEED)

tests: list[tuple[str, str, list[int]]] = [
    ("001", "empty",             []),
    ("002", "single",            [42]),
    ("003", "two_elements",      [7, 3]),
    ("004", "two_equal",         [5, 5]),

    ("005", "pow2_8",            [3, 7, 1, 5, 2, 8, 4, 6]),
    ("006", "pow2_16_sorted",    list(range(1, 17))),
    ("007", "pow2_16_reverse",   list(range(16, 0, -1))),
    ("008", "pow2_8_all_same",   [4] * 8),

    ("009", "npow2_10",          random_array(10)),
    ("010", "npow2_12_negative", random_array(12, -100, -1)),
    ("011", "npow2_13_mixed",    random_array(13, -50, 50)),

    ("012", "int_extremes",      [2147483647, -2147483648, 0, 2147483647, -2147483648, 1, -1, 0]),

    ("013", "pow2_64",           random_array(64)),
    ("014", "npow2_100",         random_array(100)),
    ("015", "pow2_256",          random_array(256)),
    ("016", "npow2_500",         random_array(500)),

    ("017", "pow2_1024",         random_array(1024)),
    ("018", "npow2_1000",        random_array(1000)),
    ("019", "pow2_4096",         random_array(4096)),
    ("020", "npow2_10000",       random_array(10000)),

    ("021", "pow2_65536",        random_array(65536)),
    ("022", "npow2_100000",      random_array(100000)),
    ("023", "npow2_1000000",     random_array(1000000)),

    ("024", "many_duplicates",   [rng.randint(1, 10) for _ in range(1024)]),
]

if __name__ == "__main__":
    for name, desc, data in tests:
        write_test(name, data)
        print(f"  {name}.dat  N={len(data):>6}  ({desc})")

    print(f"\nGenerated {len(tests)} test cases in {SCRIPT_DIR}")
