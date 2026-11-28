import json
import threading
import time

import zmq

STATUS_ENDPOINT = "tcp://localhost:5557"  # extras -> controller health reports (see code/extras_monitor.h)
PUBLISHER_ENDPOINT = "tcp://localhost:5556"  # controller -> extras


class StatusReporter:
    """Reports this service's health back to the controller (shown in /json/status under "extras").

    Python counterpart of mandeye::extras::StatusReporter (extras/utils/ExtrasUtils.h).
    Sends the state immediately on change and re-sends it every `heartbeat` seconds, so the
    controller also notices a service that died or hung (no report within `timeout` => failed).
    Thread-safe, never blocks; if the controller is not running only the latest state is kept.
    """

    def __init__(self, service, heartbeat=1.0, timeout=5.0):
        self._service = service
        self._heartbeat = heartbeat
        self._timeout = timeout
        self._ok, self._message, self._data = True, "starting", {}
        self._dirty, self._running = True, True
        self._cv = threading.Condition()
        self._thread = threading.Thread(target=self._worker, daemon=True)
        self._thread.start()

    def report(self, ok, message="", data=None):
        data = data or {}
        with self._cv:
            if (self._ok, self._message, self._data) == (ok, message, data):
                return
            self._ok, self._message, self._data = ok, message, data
            self._dirty = True
            self._cv.notify()

    def report_ok(self, message=""):
        self.report(True, message)

    def report_failure(self, message):
        self.report(False, message)

    def close(self):
        with self._cv:
            self._running = False
            self._cv.notify()
        self._thread.join()

    def _worker(self):
        # the socket lives on this thread only (ZMQ sockets are not thread-safe)
        context = zmq.Context.instance()
        socket = context.socket(zmq.PUSH)
        socket.setsockopt(zmq.LINGER, 0)
        socket.setsockopt(zmq.CONFLATE, 1)  # controller down: keep only the latest state
        socket.connect(STATUS_ENDPOINT)
        with self._cv:
            while self._running:
                payload = json.dumps({
                    "service": self._service,
                    "ok": self._ok,
                    "message": self._message,
                    "timeout_s": self._timeout,
                    "data": self._data,
                })
                self._dirty = False
                try:
                    socket.send_string(payload, zmq.NOBLOCK)
                except zmq.Again:
                    pass  # next heartbeat retries
                self._cv.wait_for(lambda: self._dirty or not self._running, self._heartbeat)
        socket.close()


def main():
    status = StatusReporter("zmq_demo")

    # Set up ZeroMQ context and subscriber socket
    context = zmq.Context.instance()
    socket = context.socket(zmq.SUB)
    socket.connect(PUBLISHER_ENDPOINT)
    socket.setsockopt_string(zmq.SUBSCRIBE, "")
    socket.setsockopt(zmq.CONFLATE, 1)
    socket.setsockopt(zmq.RCVTIMEO, 200)  # wake up to report even when the controller is silent

    msg_count = 0
    last_time_ns = None
    mode = None
    stats_start = time.monotonic()

    while True:
        try:
            message = socket.recv_string()
            print("Received message:", message)
            msg_count += 1
            j = json.loads(message)
            last_time_ns = j.get("time", last_time_ns)
            mode = j.get("mode", mode)
        except zmq.Again:
            pass

        # once per second: report what we see from the controller
        now = time.monotonic()
        if now - stats_start >= 1.0:
            system_ts = time.time()
            data = {"msgs_per_s": msg_count, "mode": mode, "system_timestamp_s": system_ts}
            if last_time_ns is not None:
                controller_ts = last_time_ns / 1e9
                data["controller_timestamp_s"] = controller_ts
                data["diff_s"] = system_ts - controller_ts
            if msg_count == 0:
                status.report(False, "no messages from controller", data)
            else:
                diff = data.get("diff_s")
                status.report(True, f"{msg_count} msgs/s, mode {mode}" + (f", diff {diff:.6f} s" if diff is not None else ""), data)
            msg_count = 0
            stats_start = now


if __name__ == "__main__":
    main()
