# Hermes

Hermes is a high-level plug-and-play WebSocket client library for C++, built on top of the popular Boost.Beast library.

## Building

```bash
cmake -S . -B build
cmake --build build
```

## Examples

Hermes ships with small demonstration programs under `examples/`:

- `hermes-sync-example` demonstrates synchronous message sending.
- `hermes-async-example` demonstrates asynchronous message sending and implicit queueing.

### Build the samples

```bash
cmake -S . -B build -DBUILD_EXAMPLES=1
cmake --build build
```

The resulting binaries live in `build/examples/`
