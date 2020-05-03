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

    sample_counts = (idle_samples, system_samples, user_samples, total_samples)
    procs = dict()
    samples = []

    payload = fin.read(mem_used)
    while len(payload) > 5:
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

    return sample_counts, procs, samples


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


def print_report(sample_counts, procs, locs):
    idle_samples, system_samples, user_samples, total_samples = sample_counts

    slocs = sorted([(k, v) for k, v in locs.items()], key=lambda x: -x[1])
    total = sum([x[1] for x in slocs])

    print('System process samples:'.rjust(25), end='')
    print(
        f'{system_samples} ({round(system_samples / total_samples * 100.0):3.0f}%)'
        .rjust(15))
    print('User process samples:'.rjust(25), end='')
    print(
        f'{user_samples} ({round(user_samples / total_samples * 100.0):3.0f}%)'
        .rjust(15))
    print('Idle samples:'.rjust(25), end='')
    print(
        f'{idle_samples} ({round(idle_samples / total_samples * 100.0):3.0f}%)'
        .rjust(15))
    print(('-' * 7 + '  ' + '-' * 4 + ' ').rjust(40))
    print('Total samples:'.rjust(25), end='')
    print(f'{total_samples} (100%)'.rjust(15))
    print('')

    print('-' * 80)
    print('Total system process time' + f'{system_samples} samples'.rjust(55))
    print('-' * 80)

    for (endpoint, func), count in slocs:
        percentage = count / total * 100.0
        print(procs[endpoint].lower().rjust(8), end='')
        print(func.rjust(35), end='')
        print(' ', end='')
        print(get_progress_bar(percentage, 30), end='')
        print(f'{percentage:.1f}%'.rjust(6))

        if percentage < 1.0:
            break

    rest = sum(
        [count / total * 100.0 for _, count in slocs if count < total * 0.01])
    print('<1%'.rjust(43), end='')
    print(' ', end='')
    print(get_progress_bar(rest, 30), end='')
    print(f'{rest:.1f}%'.rjust(6))


if __name__ == '__main__':
    args = parse_args()

    with open(args.input, 'rb') as fin:
        sample_counts, procs, samples = load_samples(fin)
        symbol_map = get_symbols(procs, args.kernel, args.sbin, args.toolchain)

        locs = get_sample_locs(samples, symbol_map)

        print_report(sample_counts, procs, locs)
