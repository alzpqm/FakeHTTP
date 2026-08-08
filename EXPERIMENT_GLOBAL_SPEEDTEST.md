# Global Speed Test Experiment

This branch isolates the FakeHTTP changes used during the Global Speed Test
investigation. It is based on commit `e8ccb36` and is published separately from
the default branch so it can be installed and reverted independently.

## Important behavior

China Speed Test uses a custom HTTP protocol on TCP port `65499`. The generic
FakeHTTP payload must not be injected into that connection. Configure the port
bypass on OpenWrt:

```sh
uci add_list fakehttp.advanced.bypass_port='65499'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

The bypass applies in both TCP directions and is installed before the NFQUEUE
rules. It prevents the old `GET /` payload from being mixed into `/speed/...`
requests.

## Interpretation

The earlier tests prove protocol contamination in the old behavior, but do not
prove that the carrier placed the line or the device on a blacklist. After the
bypass, Global Speed Test remained highly dependent on its selected server,
while Debian Ookla server 24447 reached line rate. Treat this branch as a
diagnostic/experimental build, not as evidence of a whitelist or a guaranteed
speed-unlock target.
