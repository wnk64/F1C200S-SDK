import argparse
import json
import re
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--ready", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    options = parser.parse_args()
    size = options.source.stat().st_size
    log_lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, format_string, *arguments):
            return

        def do_GET(self):
            endpoint = urlsplit(self.path).path
            allowed = self.headers.get("Referer") == "https://www.bilibili.com/" and self.headers.get("User-Agent") == "Mozilla/5.0"
            with log_lock:
                with options.log.open("a", encoding="utf-8") as log:
                    log.write(json.dumps({"endpoint": endpoint, "headers_ok": allowed, "range": self.headers.get("Range", "")}) + "\n")
            if endpoint not in ("/video.mp4", "/slow.mp4", "/truncated.mp4", "/stall.mp4"):
                self.send_error(404)
                return
            if not allowed:
                self.send_error(403)
                return
            range_header = self.headers.get("Range", "")
            match = re.fullmatch(r"bytes=(\d+)-(\d*)", range_header) if range_header else None
            if range_header and match is None:
                self.send_error(416)
                return
            start = int(match[1]) if match else 0
            end = min(int(match[2]), size - 1) if match and match[2] else size - 1
            if start > end or start >= size:
                self.send_error(416)
                return
            length = end - start + 1
            self.send_response(206 if match else 200)
            self.send_header("Content-Type", "video/mp4")
            self.send_header("Content-Length", str(length))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Connection", "close")
            if match:
                self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.end_headers()
            self.close_connection = True
            if endpoint == "/stall.mp4":
                time.sleep(5)
                return
            limit = min(length, max(1, size // 3)) if endpoint == "/truncated.mp4" else length
            try:
                with options.source.open("rb") as source:
                    source.seek(start)
                    while limit:
                        data = source.read(min(limit, 4096))
                        if not data:
                            break
                        self.wfile.write(data)
                        self.wfile.flush()
                        limit -= len(data)
                        if endpoint == "/slow.mp4":
                            time.sleep(0.005)
                if endpoint == "/truncated.mp4":
                    self.connection.shutdown(socket.SHUT_RDWR)
            except (BrokenPipeError, ConnectionResetError, OSError):
                return

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    options.ready.write_text(json.dumps({"port": server.server_port}), encoding="utf-8")
    try:
        server.serve_forever()
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
