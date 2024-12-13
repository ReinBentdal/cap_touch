import os
import numpy as np
import matplotlib.pyplot as plt

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def parse_hex_line(line: str) -> int:
    parts = line.strip().split()[:2]
    values = [int(p, 16) for p in parts if p]

    if len(values) < 2:
        raise ValueError("Each line should contain at least two hex values.")

    return values[0] + 255 * values[1]


def parse_data_file(file_path: str) -> dict[int, np.ndarray]:
    data_blocks = []
    current_block = []
    inside_block = False

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()

            if line == "Notifications started.":
                inside_block = True
                current_block = []
                continue

            if line == "Notifications stopped.":
                data_blocks.append(np.array(current_block))
                inside_block = False
                continue

            # Parse lines within a notification block
            if inside_block and line and not line.startswith("Notifications"):
                parsed_value = parse_hex_line(line)
                current_block.append(parsed_value)

    return data_blocks

def process_data(data: np.ndarray, dist_off) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    distances = []
    means = []
    vars_ = []
    for i, arr in enumerate(data):
        distances.append(i-dist_off)
        means.append(np.mean(arr))
        vars_.append(np.var(arr))

    return np.array(distances), np.array(means), np.array(vars_)


def main():

    # Plot both on the same graph
    plt.figure(figsize=(10, 5))

    files = ['comp/0mm.log', 'comp/11mm.log', 'comp/130mm.log']
    # files = ['adc/0mm.log', 'adc/11mm.log', 'adc/130mm.log']
    offsets = [8, 9, 9]

    for file, off in zip(files, offsets):
        file_path = os.path.join(SCRIPT_DIR, 'data', file)

        data = parse_data_file(file_path)

        dists, means, vars = process_data(data, off)

        print("Data from 0mm.log:")
        for d, m, v in zip(dists, means, vars):
            print(f"Distance: {d}mm, Mean: {m:.2f}, Var: {v:.2f}")

        plt.errorbar(dists, means, yerr=0, fmt='o-', label=f"{file.split('/')[1]}")

    plt.xlim(left=-5)
    # plt.yscale('log')
    plt.xlabel("Distance [mm]")
    plt.ylabel("Measurement")
    plt.title("Comparison of Measurements from Multiple Logs")
    plt.grid(True)
    plt.legend()
    plt.show()


if __name__ == "__main__":
    main()
