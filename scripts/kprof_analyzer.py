import struct
import argparse
import os
import subprocess

MAGIC = 0x4652504b

PROC_NAME_MAPPING = dict(sys='systask')


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=str)
    parser.add_argument('kernel', type=str)
    parser.add_argument('sbin', type=str)
    parser.add_argument('toolchain', type=str)

    return parser.parse_args()


def load_samples(fin):
    header = fin.read(4 * 4)
    magic, info_size, proc_size, sample_size = struct.unpack('iiii', header)

    if magic != MAGIC:
        print(f'Corrupted input file, magic: {hex(magic)}')

    kprof_info = fin.read(info_size)
    mem_used, idle_samples, system_samples, user_samples, total_samples = struct.unpack(
        'iiiii', kprof_info)

    procs = dict()
    samples = []

    payload = fin.read(mem_used)
    while payload:
        tag, payload = payload[0], payload[1:]
        endpoint = struct.unpack('i', payload[:4])[0]
        payload = payload[4:]

        if tag == 1:  # proc
            proc_name = payload[:16].split(b'\0', 1)[0].decode('ascii')
            payload = payload[16:]
            procs[endpoint] = proc_name
        elif tag == 2:  # sample
            pc = struct.unpack('I', payload[:4])[0]
            payload = payload[4:]
            samples.append(dict(pc=pc, endpoint=endpoint))

    return procs, samples


def get_symbols(procs, kernel, sbin, toolchain):
    nm = os.path.join(toolchain, 'nm')
    symbol_map = dict()

    for endpoint, name in procs.items():
        if name == 'kernel':
            path = kernel
        else:
            name = name.lower()

            mapped_name = PROC_NAME_MAPPING.get(name)
            if mapped_name is not None:
                name = mapped_name

            path = os.path.join(sbin, name)
            if not os.path.exists(path):
                continue

        rc = subprocess.run([nm, path], capture_output=True, check=True)
        symbols = rc.stdout.decode('utf-8').split('\n')[:-1]
        symbols = [x.split(' ') for x in symbols]
        symbols = [(int(x, 16), y) for (x, _, y) in symbols]
        symbols = sorted(symbols, key=lambda x: x[0])

        symbol_map[endpoint] = symbols

    return symbol_map


def get_sample_locs(samples, symbol_map):
    locs = dict()

    for s in samples:
        endpoint = s['endpoint']
        pc = s['pc']
        symbols = symbol_map.get(endpoint)

        if symbols is None:
            continue

        for (s, symbol), (e, _) in zip(symbols[:-1], symbols[1:]):
            if s <= pc < e:
                loc = symbol
                break
        else:
            loc = symbols[-1][1]

        if locs.get((endpoint, loc)) is None:
            locs[(endpoint, loc)] = 1
        else:
            locs[(endpoint, loc)] += 1

    return locs


def get_progress_bar(percentage, length):
    bar_length = length - 2
    bar = '#' * int(round(bar_length * percentage / 100.0))
    bar = bar.ljust(bar_length, '.')
    bar = f'[{bar}]'
    return bar


def print_report(procs, locs):
    slocs = sorted([(k, v) for k, v in locs.items()], key=lambda x: -x[1])
    total = sum([x[1] for x in slocs])

    for ((endpoint, func), count) in slocs:
        print(procs[endpoint].lower().rjust(8), end='')
        print(func.rjust(35), end='')
        print(' ', end='')
        print(get_progress_bar(count / total * 100.0, 40), end='')
        print(f'{count / total * 100.0:.1f}%'.rjust(5), end='')
        print('')


if __name__ == '__main__':
    args = parse_args()

    with open(args.input, 'rb') as fin:
        procs, samples = load_samples(fin)
        symbol_map = get_symbols(procs, args.kernel, args.sbin, args.toolchain)

        locs = get_sample_locs(samples, symbol_map)

        print_report(procs, locs)
