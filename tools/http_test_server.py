"""只提供probe.bin，禁止目录列表和其他文件，Ctrl+C停止。"""
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
import argparse

class Handler(BaseHTTPRequestHandler):
    # 浏览器空闲连接不应占住整个测试服务。
    def setup(self):
        super().setup()
        self.connection.settimeout(5)
    def do_GET(self):
        self.reply(False)
    def do_HEAD(self):
        self.reply(True)
    def reply(self, head):
        if self.path != "/probe.bin":
            self.send_error(404)
            return
        data = (Path(__file__).resolve().parents[1] / "http_files/probe.bin").read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if not head:
            self.wfile.write(data)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    print("HTTP test server, port", args.port, "only /probe.bin", flush=True)
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()
