"""Regression: initial TTL32 must not contaminate an adjacent peer. Root/netns only."""
import json
import os
import pathlib
import select
import signal
import socket
import subprocess
import sys
import time

def cmd(*args):
    try:
        return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as error:
        print(error.output, file=sys.stderr)
        raise

def line(process, timeout=5):
    if not select.select([process.stdout], [], [], timeout)[0]:
        raise TimeoutError('peer output')
    return process.stdout.readline().strip()

if len(sys.argv) == 2:
    results = []
    original = os.stat('/proc/self/ns/net').st_ino
    for family in ('4', '6'):
        for enabled, ttl in [(0, 32), (1, 64), (1, 32), (1, 128), (1, 255)]:
            output = cmd('unshare', '-n', sys.executable, __file__, '--case', str(original),
                         sys.argv[1], str(enabled), str(ttl), family)
            results.append(json.loads(output))
    print(json.dumps(results, indent=2))
    assert all(row['received'] == 'CLIENT-DATA' and row['client_error'] is None
               for row in results), 'Synthetic payload reached peer or connection failed'
    raise SystemExit(0)

if sys.argv[1] == '--server':
    family = sys.argv[3]
    af = socket.AF_INET if family == '4' else socket.AF_INET6
    with socket.socket(af) as server:
        if family == '4':
            server.setsockopt(socket.IPPROTO_IP, socket.IP_TTL, int(sys.argv[2]))
        else:
            server.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_UNICAST_HOPS, int(sys.argv[2]))
        server.bind(('0.0.0.0' if family == '4' else '::', 18082))
        server.listen(1)
        server.settimeout(8)
        print('READY', flush=True)
        connection, _ = server.accept()
        with connection:
            connection.settimeout(3)
            payload = connection.recv(1024)
        print(json.dumps({'received': payload.decode('ascii', errors='replace')}), flush=True)
    raise SystemExit(0)

assert sys.argv[1] == '--case' and len(sys.argv) == 7
assert os.stat('/proc/self/ns/net').st_ino != int(sys.argv[2]), 'Not isolated'
binary, enabled, ttl = sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
family = sys.argv[6]
af = socket.AF_INET if family == '4' else socket.AF_INET6
src, dst, prefix = ('198.18.1.1', '198.18.1.2', '24') if family == '4' else (
    '2001:db8:1::1', '2001:db8:1::2', '64')
peer = subprocess.Popen(['unshare', '-n', sys.executable, __file__, '--server', str(ttl), family],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
daemon = None
try:
    assert line(peer) == 'READY'
    assert os.stat(f'/proc/{peer.pid}/ns/net').st_ino != os.stat('/proc/self/ns/net').st_ino
    cmd('ip', 'link', 'add', 'audit-client', 'type', 'veth', 'peer', 'name', 'audit-peer')
    cmd('ip', 'link', 'set', 'audit-peer', 'netns', str(peer.pid))
    cmd('ip', 'addr', 'add', src + '/' + prefix, 'dev', 'audit-client', 'nodad')
    cmd('ip', 'link', 'set', 'audit-client', 'up')
    cmd('ip', 'link', 'set', 'lo', 'up')
    cmd('nsenter', '-t', str(peer.pid), '-n', 'ip', 'addr', 'add', dst + '/' + prefix, 'dev', 'audit-peer', 'nodad')
    cmd('nsenter', '-t', str(peer.pid), '-n', 'ip', 'link', 'set', 'audit-peer', 'up')
    cmd('nsenter', '-t', str(peer.pid), '-n', 'ip', 'link', 'set', 'lo', 'up')
    if enabled:
        daemon = subprocess.Popen([binary, '-' + family, '-s', '-i', 'audit-client',
                                   '-h', 'example.invalid', '-r', '1'],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        for _ in range(100):
            if daemon.poll() is not None:
                raise RuntimeError(daemon.stderr.read())
            queue = pathlib.Path('/proc/net/netfilter/nfnetlink_queue')
            if queue.exists() and any(row.split()[:2] == ['512', str(daemon.pid)]
                                      for row in queue.read_text().splitlines()):
                time.sleep(0.3)
                break
            time.sleep(0.02)
        else:
            raise TimeoutError('queue setup')
    with socket.socket(af) as client:
        client.settimeout(3)
        client_error = None
        try:
            client.connect((dst, 18082))
            client.sendall(b'CLIENT-DATA')
        except OSError as error:
            client_error = str(error)
        observed = json.loads(line(peer))
    peer.wait(timeout=3)
    assert peer.returncode == 0, peer.stderr.read()
    observed.update({'family': family, 'fakehttp_enabled': bool(enabled), 'peer_initial_ttl': ttl,
                     'client_error': client_error})
    print(json.dumps(observed))
finally:
    for process in (daemon, peer):
        if process is not None and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
