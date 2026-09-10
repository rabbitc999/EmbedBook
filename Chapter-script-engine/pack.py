# pack.py
import zlib, base64

src = open("parser.py", "rb").read()
blob = base64.b85encode(zlib.compress(src, 9))

with open("parser_packed.py", "w", encoding="utf-8") as f:
    f.write(
        "import zlib, base64\n"
        f"exec(zlib.decompress(base64.b85decode({blob!r})))\n"
    )
