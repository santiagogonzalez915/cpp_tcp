# NetBridge — C++17 TCP Implementation

NetBridge is a direct C++17 port of the same Java TCP implementation done in class. It utilizes a TCP Reno like AIMD for congestion control, and Go-Back-N loss prevention strategies.

The codebase is organized under the `nb::` namespace with include paths
prefixed `nb/`. No Fish-themed identifiers remain in the public API; the
renaming is consistent throughout:

## Building

```sh
cmake -S . -B build
cmake --build build -j
```

Outputs:

- `build/netstack` — simulator/emulator entry point.
- `build/trawler`  — coordinator server.
- `build/nb_tests` — GoogleTest unit tests for wire-format parity and
parsing.

Run the tests with:

```sh
cd build && ctest --output-on-failure
```

## Running the simulator

```sh
./build/netstack simulate <num-nodes> <topo-file> [command-file] [timescale]
```

Example:

```sh
./build/netstack simulate 3 scripts/example.topo scripts/example.fish
```

## Running a trawler + emulator pair

Terminal 1:

```sh
./build/trawler 17777
```

Terminal 2 (node 0):

```sh
./build/netstack emulate 127.0.0.1 17777 19001
```

Terminal 3 (node 1):

```sh
./build/netstack emulate 127.0.0.1 17777 19002
```

Then type `1 hello` into terminal 2 to ping node 1. Terminal 3 prints the
ping, terminal 2 prints the reply. `exit` stops an emulator.

## Directory layout

```
cpp_tcp/
├── CMakeLists.txt
├── include/nb/        # public headers
├── src/
│   ├── core/          # RuntimeManager, event queue
│   ├── net/           # Packet, Transport, EmulatorPacket
│   ├── sim/           # Simulator, parsers, Topology, Edge
│   ├── node/          # StackNode, TcpManager, TcpSocket
│   ├── apps/          # TimerTask, TransferClient/Server
│   ├── emu/           # Emulator, Trawler, EmulatedLink, ARP
│   ├── main.cpp       # netstack entry
│   └── trawler_main.cpp
├── test/              # GoogleTest suites
└── scripts/           # wrappers + sample topo/command files
```

## Command file syntax

Identical to Java's:

- `# comment` or `// comment` — skipped.
- `edge A B [bw=…] [delay=…] [buffer=…] [loss=…]` — add a topology edge.
- `fail edge A B`, `restart edge A B` — toggle edge state.
- `fail node A`, `restart node A` — toggle node state.
- `time N` — parse no further lines until absolute simulated time N ms.
- `time + N` — defer N milliseconds from now.
- `echo ...` — print the remainder of the line.
- `exit` — stop the simulator/emulator.
- Any other line `A B …` — for the simulator, deliver the remaining tokens
as a command to node A; for the emulator, deliver the whole line to the
local node.

Example node command: `1 hello` (ping node 1 with payload `hello`).
TCP-layer commands:

- `transfer <dest> <destPort> <localPort> <amount> [interval] [sz]` —
open a TCP connection from the local node to `dest:destPort`, bound to
`localPort`, and send `amount` bytes.
- `server <localPort> [backlog]` — accept TCP connections on `localPort`
and verify the data they send.

## Notes on fidelity

- `Packet`, `Transport`, and `EmulatorPacket` use the exact big-endian byte
layouts produced by `java.math.BigInteger.toByteArray()` in the Java
implementation, so a C++ emulator interoperates with a Java emulator.
- ARP / neighbor commands (`addNeighbor`, `addNeighborOptions`,
`removeNeighbor`, `reset`) are text commands byte-compatible with Java's
`TrawlerNodeARPCommands`. Floating-point edge-option values are
serialized using Java's shortest-round-trip double-to-string format.
- Timer callbacks use `std::function<void()>` in place of Java's reflection
(`Callback.java` / `Method.invoke`). Semantics are identical: events are
popped in chronological order and fired synchronously.
- The Simulator advances simulated time; the Emulator is driven by wall
clock (`clockMicros()`), with `poll()` used for UDP, trawler TCP, and
stdin multiplexing in a single loop, matching Java's `MultiplexIO`.

