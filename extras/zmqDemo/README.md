# Demo code to read from a ZMQ socket

This code reads from a ZMQ socket and prints the messages to the console.

Prerequisites:
```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install python3-zmq python3-systemmd
```

## Reporting status back to the controller

The demo also reports its own health to the controller with `StatusReporter` (copy the class into your own
extra). It PUSHes a JSON message to `tcp://localhost:5557` on every change and at least once per second:

```json
{"service": "zmq_demo", "ok": true, "message": "8 msgs/s, mode SCANNING", "timeout_s": 5.0, "data": {"msgs_per_s": 8}}
```

The controller shows it in `/json/status` under `extras.services.zmq_demo` and in the Extras column of the
web page. A service that stops reporting for longer than `timeout_s` is shown as failed.

```python
status = StatusReporter("my_service")
status.report_failure("camera not found")
status.report(True, "recording", {"fps": 30})
```
