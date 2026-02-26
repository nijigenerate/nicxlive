#!/usr/bin/env python3
import argparse
import http.server
import json
import mimetypes
import pathlib
import socketserver
import urllib.parse


class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


class NicxHandler(http.server.SimpleHTTPRequestHandler):
    models_root: pathlib.Path = None

    def end_headers(self):
        # Disable browser caching so /models/model.inx always reflects current file state.
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def do_HEAD(self):
        if self.path.startswith("/models/__list"):
            self.serve_model_list(head_only=True)
            return
        if self.path.startswith("/models/"):
            self.serve_model(head_only=True)
            return
        return super().do_HEAD()

    def do_GET(self):
        if self.path.startswith("/models/__list"):
            self.serve_model_list(head_only=False)
            return
        if self.path.startswith("/models/"):
            self.serve_model(head_only=False)
            return
        return super().do_GET()

    def serve_model_list(self, head_only: bool):
        root = self.models_root
        if root is None or not root.is_dir():
            self.send_error(500, "models root is not configured")
            return
        models = []
        for p in root.rglob("*"):
            if not p.is_file():
                continue
            suf = p.suffix.lower()
            if suf not in (".inx", ".inp"):
                continue
            rel = p.relative_to(root).as_posix()
            models.append(f"/models/{rel}")
        models.sort()
        payload = json.dumps({"models": models}, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        if not head_only:
            self.wfile.write(payload)

    def serve_model(self, head_only: bool):
        root = self.models_root
        if root is None:
            self.send_error(500, "models root is not configured")
            return
        parsed = urllib.parse.urlsplit(self.path)
        rel = urllib.parse.unquote(parsed.path[len("/models/"):])
        rel_path = pathlib.PurePosixPath(rel)
        if rel_path.is_absolute() or ".." in rel_path.parts:
            self.send_error(400, "invalid model path")
            return
        model = (root / pathlib.Path(*rel_path.parts)).resolve()
        try:
            model.relative_to(root)
        except ValueError:
            self.send_error(403, "path escapes models root")
            return
        if not model.is_file():
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
    parser = argparse.ArgumentParser(description="Serve nicxlive wasm app and mount a models directory to /models")
    parser.add_argument("--models-dir", required=True, help="Directory mounted as /models")
    parser.add_argument("--port", type=int, default=8000, help="Port to listen on")
    args = parser.parse_args()

    script_dir = pathlib.Path(__file__).resolve().parent
    nicxlive_root = script_dir.parent
    models_root = pathlib.Path(args.models_dir).expanduser().resolve()

    if not models_root.is_dir():
        raise SystemExit(f"models dir not found: {models_root}")

    handler = NicxHandler
    handler.models_root = models_root

    with ReusableTCPServer(("127.0.0.1", args.port), lambda *a, **k: handler(*a, directory=str(nicxlive_root), **k)) as httpd:
        print(f"serving nicxlive root: {nicxlive_root}")
        print(f"models mapping: /models/* -> {models_root}")
        print(f"open: http://127.0.0.1:{args.port}/wasm/index.html")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
