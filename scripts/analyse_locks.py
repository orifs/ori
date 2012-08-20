import re
import sys
import collections
import datetime

locks = set()
threads = set()

UNLOCKED = 0
WAITING_READ = 1
LOCKED_READ = 2
WAITING_WRITE = 3
LOCKED_WRITE = 4

threads_state = collections.defaultdict(lambda: collections.defaultdict(lambda: UNLOCKED))
threads_numlocks = collections.defaultdict(lambda: collections.defaultdict(lambda: UNLOCKED))

entry_re = re.compile('(?P<time>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (?P<tid>\d+) (?P<cmd>[\w\s]+): (?P<lid>\d+)')
time_fmt = "%Y-%m-%d %H:%M:%S"


with open(sys.argv[1], 'r') as f:
    loglines = f.readlines()

print("Finding threads and locks")
for l in loglines:
    data = l[:-1]
    m = entry_re.match(data)

    if m is not None:
        tid = int(m.group('tid'))
        threads.add(tid)
        lid = int(m.group('lid'))
        locks.add(lid)

threads = sorted(list(threads))
locks = sorted(list(locks))

def write_table_header(f):
    f.write("<tr>\n")
    for l in locks:
        f.write('<th colspan="{}">Lock {}</th>'.format(len(threads), l))
    f.write("</tr>\n<tr>")
    for l in locks:
        for t in threads:
            f.write('<th>{}</th>'.format(t))
    f.write("</tr>\n")

def write_table_line(f, last_diff):
    f.write('<tr>\n')
    for l in locks:
        for t in threads:
            if l % 2 == 1:
                f.write('<td class="odd" style="')
            else:
                f.write('<td style="')

            ts = threads_state[t][l]
            if ts == WAITING_READ:
                f.write('background: #0F0;')
            elif ts == LOCKED_READ:
                f.write('background: #080;')
            elif ts == WAITING_WRITE:
                f.write('background: #F00;')
            elif ts == LOCKED_WRITE:
                f.write('background: #800;')

            f.write(' height: {}px;'.format((last_diff+1)*8))

            f.write('">{}</td>'.format(threads_numlocks[t][l]))
    f.write('</tr>\n')

with open('output.html', 'w') as f:
    print("Writing HTML")

    # Write header
    f.write("""
<html>
<head>
    <style>
        table td {
            width: 20px;
            margin: 2px;
            border-left: 1px solid #eee;
            border-right: 1px solid #eee;
        }

        td.odd {
            background: #ddd;
        }
    </style>
</head>
<body>
    <table>
""")

    write_table_header(f)

    count = 0
    last_diff = 0
    last_time = 0

    for ix_l, l in enumerate(loglines):
        data = l[:-1]
        m = entry_re.match(data)

        if m is not None:
            tid = int(m.group('tid'))
            lid = int(m.group('lid'))
            cmd = m.group('cmd')

            cmd_to_state = {
                    'unlock': UNLOCKED,
                    'readLock': WAITING_READ,
                    'writeLock': WAITING_WRITE,
                    'success readLock': LOCKED_READ,
                    'success writeLock': LOCKED_WRITE,
                    }

            if cmd.startswith('success'):
                threads_numlocks[tid][lid] += 1
                threads_state[tid][lid] = cmd_to_state[cmd]
            elif cmd == 'unlock':
                threads_numlocks[tid][lid] -= 1
                if threads_numlocks[tid][lid] == 0:
                    threads_state[tid][lid] = cmd_to_state[cmd]
            else:
                threads_state[tid][lid] = cmd_to_state[cmd]

            if threads_numlocks[tid][lid] > 1:
                print("Warning: reentrant lock")

            # Write a line of HTML
            write_table_line(f, last_diff)

            time = datetime.datetime.strptime(m.group('time'), time_fmt)
            if ix_l > 0:
                last_diff = (time - last_time).seconds
            last_time = time

            count += 1
            if count % 199 == 0:
                f.write('</table>\n<table>')
                write_table_header(f)


    f.write('</table>\n</body></html>')

