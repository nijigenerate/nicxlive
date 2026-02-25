#!/usr/bin/env python3
import argparse
import http.server
import mimetypes
import pathlib
import socketserver


class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


class NicxHandler(http.server.SimpleHTTPRequestHandler):
    model_path: pathlib.Path = None

    def do_HEAD(self):
        if self.path == "/models/model.inx":
            self.serve_model(head_only=True)
            return
        return super().do_HEAD()

    def do_GET(self):
        if self.path == "/models/model.inx":
            self.serve_model(head_only=False)
            return
        return super().do_GET()

    def serve_model(self, head_only: bool):
        model = self.model_path
        if model is None or not model.is_file():
            self.send_error(404, "model file not found")
            return
        ctype, _ = mimetypes.guess_type(str(model))
        if not ctype:
            ctype = "application/octet-stream"
        try:
            data = model.read_bytes()
        except OSError:
            self.send_error(500, "failed to read model file")
            return

        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if not head_only:
            self.wfile.write(data)


def main():
    parser = argparse.ArgumentParser(description="Serve nicxlive wasm app and map /models/model.inx to a chosen file")
    parser.add_argument("--model", required=True, help="Path to source .inx/.inp model file")
    parser.add_argument("--port", type=int, default=8000, help="Port to listen on")
    args = parser.parse_args()

    script_dir = pathlib.Path(__file__).resolve().parent
    nicxlive_root = script_dir.parent
    model_path = pathlib.Path(args.model).expanduser().resolve()

    if not model_path.is_file():
        raise SystemExit(f"model not found: {model_path}")

    handler = NicxHandler
    handler.model_path = model_path

    with ReusableTCPServer(("127.0.0.1", args.port), lambda *a, **k: handler(*a, directory=str(nicxlive_root), **k)) as httpd:
        print(f"serving nicxlive root: {nicxlive_root}")
        print(f"model mapping: /models/model.inx -> {model_path}")
        print(f"open: http://127.0.0.1:{args.port}/wasm/index.html")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
