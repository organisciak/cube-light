#!/usr/bin/env python3
"""Dev server for simulator/web with caching disabled (module scripts and
the wasm otherwise stick in the browser cache between edits).
Usage: python3 simulator/serve.py [port]   (default 5277)"""
import http.server, os, sys
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'web'))
port = int(sys.argv[1]) if len(sys.argv) > 1 else 5277
class H(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm', '.js': 'text/javascript'}
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()
    def log_message(self, *a): pass
print(f'http://127.0.0.1:{port}/')
http.server.ThreadingHTTPServer(('127.0.0.1', port), H).serve_forever()
