```bash
# normal compile
clang test.c -o encode
```

```bash
# generate IR file
clang -S -emit-llvm test.c -o test.ll
```

```bash
# instrument!
cd ..
make
cd example
../bin/parsetest test.ll
```

```bash
# generate binary from IR file
clang test.ll -o decode
```

```bash
# test!
echo -e 'hello\x00' | ./encode

echo -e 'hello\x00' | ./encode | ./decode
```