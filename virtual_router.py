import io
from PIL import Image

ram_storage = {}  # filename -> raw bytes

def normalize(path: str) -> str:
    if not path.startswith("/"):
        path = "/" + path
    return path

# -----------------------
# Directory listing
# -----------------------

def list_dir(path, session):
    path = normalize(path)

    if path == "/":
        return ["input/", "result/"]

    if path == "/input/":
        return list(ram_storage.keys())

    if path == "/result/":
        if not ram_storage:
            return ["provide_input.txt"]
        return [name.rsplit(".", 1)[0] + ".png" for name in ram_storage]

    return None


# -----------------------
# Read file
# -----------------------

def get_file(path, session):
    path = normalize(path)

    if path.startswith("/input/"):
        name = path.replace("/input/", "")
        return ram_storage.get(name)

    if path.startswith("/result/"):
        name = path.replace("/result/", "")

        if name == "provide_input.txt":
            if not ram_storage:
                return b"provide input\n"
            return None

        base = name.replace(".png", "")
        if base not in ram_storage:
            return None

        img = Image.open(io.BytesIO(ram_storage[base]))
        out = io.BytesIO()
        img.convert("RGBA").save(out, format="PNG")
        return out.getvalue()

    return None


# -----------------------
# Write file
# -----------------------

def put_file(path, data, session):
    path = normalize(path)

    if path.startswith("/input/"):
        name = path.replace("/input/", "")
        ram_storage[name] = data
        return True

    return False


def delete(path, session):
    path = normalize(path)

    if path.startswith("/input/"):
        name = path.replace("/input/", "")
        ram_storage.pop(name, None)
        return True

    return False


# -----------------------
# Permissions
# -----------------------

def can_put(path, session):
    return path.startswith("/input/")

def can_delete(path, session):
    return path.startswith("/input/")

def can_mkcol(path, session):
    return False  # no dynamic folder creation
