"""Regression: preserve existing mangle marks. Requires root and isolated netns."""
import json
import os
import pathlib
import signal
import socket
import subprocess
import sys
import time

def cmd(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)

if len(sys.argv) == 2:
    # Only the child can configure interfaces/rules, after isolation is verified.
    original = os.stat('/proc/self/ns/net').st_ino
    for family in ('4', '6'):
        subprocess.run(['unshare', '-n', sys.executable, __file__, '--child',
                        str(original), sys.argv[1], family], check=True)
    raise SystemExit(0)
assert len(sys.argv) == 5 and sys.argv[1] == '--child'
assert os.stat('/proc/self/ns/net').st_ino != int(sys.argv[2]), 'Not isolated'
binary = sys.argv[3]
family = sys.argv[4]
af = socket.AF_INET if family == '4' else socket.AF_INET6
ipt = 'iptables' if family == '4' else 'ip6tables'
src, dst, prefix = ('198.18.0.1', '198.18.0.2', '32') if family == '4' else (
    '2001:db8::1', '2001:db8::2', '128')
cmd('ip', 'link', 'set', 'lo', 'up')
for address in (src + '/' + prefix, dst + '/' + prefix):
    cmd('ip', 'address', 'add', address, 'dev', 'lo')
listener = socket.socket(af)
listener.bind((dst, 18081))
listener.listen(5)
listener.settimeout(3)
for hook in ('PREROUTING', 'POSTROUTING'):
    cmd(ipt, '-t', 'mangle', '-A', hook, '-p', 'tcp', '--dport', '18081',
        '--tcp-flags', 'SYN,ACK', 'SYN', '-j', 'MARK', '--set-mark', '0x100')

def count(hook):
    rules = cmd(ipt + '-save', '-c', '-t', 'mangle')
    for line in rules.splitlines():
        if '-j MARK' in line and '--dport 18081' in line and '-A ' + hook + ' ' in line:
            return int(line.split(':', 1)[0].lstrip('['))
    raise AssertionError('mark rule disappeared')

def probe():
    before = [count(hook) for hook in ('PREROUTING', 'POSTROUTING')]
    with socket.socket(af) as client:
        client.settimeout(3)
        client.bind((src, 0))
        client.connect((dst, 18081))
        connection, _ = listener.accept()
        connection.close()
    return [count(hook) - hits for hook, hits in zip(('PREROUTING', 'POSTROUTING'), before)]

results = {'family': family, 'baseline_mark_hits': probe()}
for mode, flags in [('nft', []), ('iptables', ['-z'])]:
    with open('/tmp/daemon-audit-' + mode + '.log', 'w+') as logfile:
        process = subprocess.Popen([binary, '-' + family, '-s', '-i', 'lo', '-h', 'example.invalid',
                                    '-r', '1', *flags], stdout=logfile, stderr=logfile)
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    logfile.seek(0)
                    raise RuntimeError(logfile.read())
                # Ready only after queue and the actual daemon-created rules exist.
                queue = pathlib.Path('/proc/net/netfilter/nfnetlink_queue')
                bound = queue.exists() and any(
                    row.split()[:2] == ['512', str(process.pid)]
                    for row in queue.read_text().splitlines())
                rules = cmd(ipt + '-save', '-t', 'mangle') if flags else cmd('nft', 'list', 'tables')
                if bound and ('FAKEHTTP_R' if flags else 'table ip' + ('6' if family == '6' else '') + ' fakehttp') in rules:
                    # Rule installation is a short series of subprocesses.
                    time.sleep(0.3)
                    break
                time.sleep(0.05)
            else:
                raise TimeoutError('daemon readiness')
            results[mode + '_mark_hits'] = probe()
        finally:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                results[mode + '_forced_cleanup'] = True
            results[mode + '_exit'] = process.returncode
listener.close()
print(json.dumps(results, indent=2))
assert results['baseline_mark_hits'] == [1, 1]
assert results['nft_mark_hits'] == [1, 1]
assert results['iptables_mark_hits'] == [1, 1], 'Existing mangle marking was bypassed'
assert results['nft_exit'] == results['iptables_exit'] == 0
