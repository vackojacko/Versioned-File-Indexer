![Language](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![Build](https://img.shields.io/badge/build-passing-brightgreen)

# Versioned File Indexer

A memory-efficient C++17 command-line tool that builds word-frequency indexes
over large text files using a fixed-size buffer. Memory usage is always
independent of file size.

## Build
```bash
make
```

## Usage

### Word Count
```bash
./analyzer --file dataset.txt --version v1 --buffer 512 --query word --word error
```

### Top-K
```bash
./analyzer --file dataset.txt --version v1 --buffer 512 --query top --top 10
```

### Difference between two versions
```bash
./analyzer --file1 dataset_v1.txt --version1 v1 \
           --file2 dataset_v2.txt --version2 v2 \
           --buffer 512 --query diff --word error
```

## Design
| Class | Role |
|---|---|
| `BufferedFileReader` | RAII binary file reader, fixed-size chunks |
| `Tokenizer` | Byte stream → lowercase words, handles cross-buffer tokens |
| `VersionedIndexer` | word→frequency map for one version |
| `VersionManager` | Owns all indexer instances |
| `Query` (abstract) | Base class, pure virtual `execute()` |
| `WordQuery` | Word count query |
| `DiffQuery` | Frequency difference across two versions |
| `TopKQuery` | Top-K via min-heap, O(N log K) |

## C++ Features
- Two function templates: `measureSeconds<F>`, `clamp<T>`
- Inheritance + runtime polymorphism via `unique_ptr<Query>`
- Function overloading: two `buildIndex` signatures
- Exception handling throughout
- RAII resource management
