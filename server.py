import os
from cheroot import wsgi
from wsgidav.wsgidav_app import WsgiDAVApp
from wsgidav.dav_provider import DAVProvider, DAVCollection, DAVNonCollection

import virtual_router as ROUTER


class VirtualFile(DAVNonCollection):
    def __init__(self, path, environ):
        super().__init__(path, environ)
        self.path = path
        self.environ = environ

    def get_content_length(self):
        data = ROUTER.get_file(self.path, {})
        return len(data) if data else 0

    def support_etag(self):
        return False

    def get_content(self):
        data = ROUTER.get_file(self.path, {})
        return io.BytesIO(data if data else b"")

    def begin_write(self, content_type=None):
        self._buf = bytearray()
        return self

    def write(self, data):
        self._buf.extend(data)

    def end_write(self, with_errors=False):
        ROUTER.put_file(self.path, bytes(self._buf), {})

    def delete(self):
        ROUTER.delete(self.path, {})


class VirtualDir(DAVCollection):
    def __init__(self, path, environ):
        if not path.endswith("/"):
            path += "/"
        super().__init__(path, environ)
        self.path = path

    def get_member_names(self):
        return ROUTER.list_dir(self.path, {}) or []

    def get_member(self, name):
        full = self.path + name
        if name.endswith("/"):
            return VirtualDir(full, self.environ)
        return VirtualFile(full, self.environ)


class Provider(DAVProvider):
    def get_resource_inst(self, path, environ):
        if path.endswith("/"):
            if ROUTER.list_dir(path, {}) is not None:
                return VirtualDir(path, environ)
            return None

        if ROUTER.get_file(path, {}) is not None:
            return VirtualFile(path, environ)

        return None


HOST = "0.0.0.0"
PORT = 8080

config = {
    "provider_mapping": {"/": Provider()},
    "simple_dc": {"user_mapping": {"*": True}},
    "verbose": 1,
}

if __name__ == "__main__":
    app = WsgiDAVApp(config)
    server = wsgi.Server((HOST, PORT), app)
    print(f"Running on http://{HOST}:{PORT}")
    server.start()
