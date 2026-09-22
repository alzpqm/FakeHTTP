"""Run inside unshare -n. No host or production firewall changes."""
import os
import pathlib
import signal
import subprocess
import sys
import time

binary = sys.argv[1]
subprocess.run(["ip", "link", "set", "lo", "up"], check=True)
for backend in ([], ["-z"]):
    for cycle in range(3):
        process = subprocess.Popen([binary, "-a", "-h", "example.com", "-s"] + backend,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise AssertionError(process.communicate()[1].decode())
                queue_path = pathlib.Path("/proc/net/netfilter/nfnetlink_queue")
                queues = queue_path.read_text() if queue_path.exists() else ""
                if any(line.split()[0] == "512" for line in queues.splitlines()):
                    # Setup binds the queue before installing rules/handlers.
                    time.sleep(0.3)
                    break
                time.sleep(0.02)
            else:
                raise AssertionError("queue512 not ready")
            start = time.monotonic()
            process.send_signal(signal.SIGTERM)
            output, errors = process.communicate(timeout=2)
            elapsed = (time.monotonic() - start) * 1000
            assert process.returncode == 0, errors.decode()
            assert "AddressSanitizer" not in errors.decode(), errors.decode()
            assert "runtime error:" not in errors.decode(), errors.decode()
            queues = queue_path.read_text() if queue_path.exists() else ""
            assert not any(line.split()[0] == "512" for line in queues.splitlines())
            if backend:
                for command in ("iptables-save", "ip6tables-save"):
                    assert "FAKEHTTP" not in subprocess.check_output([command, "-t", "mangle"], text=True)
            else:
                assert "fakehttp" not in subprocess.check_output(["nft", "list", "tables"], text=True)
            print(f"{'iptables' if backend else 'nftables'} cycle={cycle+1} clean_stop_ms={elapsed:.1f}", flush=True)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
