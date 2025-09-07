import time
import gevent
from locust import User, task, events
from gevent import socket as gsocket

START_LINE = "start"
QUEUE_PREFIX = "[INFO] You are #"


class TcpUser(User):
    host = "127.0.0.1"
    port = 9090

    def on_start(self):
        self.sock = gsocket.create_connection((self.host, self.port), timeout=60.0)
        self.sock.settimeout(0.5)
        self._buf = b""
        self.started = False
        self._drain_until_start(max_wait=10.0)

    def on_stop(self):
        try:
            self.sock.close()
        except Exception:
            pass

    def _readline(self, timeout=0.5):
        end = time.time() + timeout
        while time.time() < end:
            pos = self._buf.find(b"\n")
            if pos != -1:
                line = self._buf[:pos].decode("utf-8", errors="ignore").rstrip("\r")
                self._buf = self._buf[pos + 1:]
                return line

            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    return None
                self._buf += chunk
            except Exception:
                gevent.sleep(0)
        return None

    def _drain_until_start(self, max_wait=0.0):
        deadline = time.time() + max_wait if max_wait > 0 else None
        while True:
            if deadline and time.time() > deadline:
                return

            line = self._readline(timeout=0.2)
            if line is None:
                return
            if not line:
                continue

            if line == START_LINE:
                self.started = True
                events.request.fire(
                    request_type="tcp",
                    name="tcp/start",
                    response_time=0,
                    response_length=len(line),
                    exception=None,
                )
                return

            if line.startswith(QUEUE_PREFIX):
                events.request.fire(
                    request_type="tcp",
                    name="tcp/queue",
                    response_time=0,
                    response_length=len(line),
                    exception=None,
                )
                continue

            events.request.fire(
                request_type="tcp",
                name="tcp/notice",
                response_time=0,
                response_length=len(line),
                exception=None,
            )

    @task(3)
    def ping(self):
        start = time.time()
        try:
            if not self.started:
                line = self._readline(timeout=2.0)

                if not line:
                    events.request.fire(
                        request_type="tcp",
                        name="tcp/queue",
                        response_time=int((time.time() - start) * 1000),
                        response_length=0,
                        exception=None,
                    )
                    return

                if line.startswith(QUEUE_PREFIX):
                    events.request.fire(
                        request_type="tcp",
                        name="tcp/queue",
                        response_time=int((time.time() - start) * 1000),
                        response_length=len(line),
                        exception=None,
                    )
                    return

                if line == START_LINE:
                    self.started = True
                    events.request.fire(
                        request_type="tcp",
                        name="tcp/start",
                        response_time=int((time.time() - start) * 1000),
                        response_length=len(line),
                        exception=None,
                    )
                    return

                events.request.fire(
                    request_type="tcp",
                    name="tcp/queue",
                    response_time=int((time.time() - start) * 1000),
                    response_length=0,
                    exception=Exception(f"Unexpected while waiting: {line!r}"),
                )
                return

            self.sock.sendall(b"ping\n")
            line = self._readline(timeout=2.0)

            if line == "pong":
                events.request.fire(
                    request_type="tcp",
                    name="tcp/ping",
                    response_time=int((time.time() - start) * 1000),
                    response_length=len(line),
                    exception=None,
                )
            elif line.startswith(QUEUE_PREFIX):
                events.request.fire(
                    request_type="tcp",
                    name="tcp/queue-after-start",
                    response_time=int((time.time() - start) * 1000),
                    response_length=len(line),
                    exception=None,
                )
            else:
                events.request.fire(
                    request_type="tcp",
                    name="tcp/ping",
                    response_time=int((time.time() - start) * 1000),
                    response_length=0,
                    exception=Exception(f"Unexpected ping response: {line!r}"),
                )

        except Exception as e:
            events.request.fire(
                request_type="tcp",
                name="tcp/ping",
                response_time=int((time.time() - start) * 1000),
                response_length=0,
                exception=e,
            )

    @task(1)
    def pump_queue(self):
        if not self.started:
            self._drain_until_start(max_wait=0.2)