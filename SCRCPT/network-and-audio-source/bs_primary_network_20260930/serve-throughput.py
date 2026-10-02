#!/usr/bin/env python3
"""Serve one generated 4 MiB HTTP body for a disk-free LAN throughput check."""
import argparse
import http.server
import time

parser = argparse.ArgumentParser()
parser.add_argument("--bind", required=True)
parser.add_argument("--port", type=int, default=8765)
args = parser.parse_args()

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        length = 4 * 1024 * 1024
        self.send_response(200)
        self.send_header("Content-Length", str(length))
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Connection", "close")
        self.end_headers()
        block = bytes(16384)
        start = time.monotonic()
        try:
            for _ in range(length // len(block)):
                self.wfile.write(block)
            self.wfile.flush()
            print("Served bytes:", length, "elapsed:", round(time.monotonic() - start, 3), flush=True)
        except (BrokenPipeError, ConnectionResetError):
            print("Transfer interrupted", flush=True)

with http.server.HTTPServer((args.bind, args.port), Handler) as server:
    server.timeout = 20
    print("Listening for one request", server.server_address, flush=True)
    server.handle_request()
