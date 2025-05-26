| Component             | Role Summary                                                                         |
| --------------------- | ------------------------------------------------------------------------------------ |
| **client.c**          | Runs on the user's machine. Acts as a UI+network front-end to send/receive messages. |
| **server.c**          | Central message hub. Receives messages, handles commands, manages state.             |
| **client\_manager.c** | Server-side helper for tracking users, updating status, and broadcasting.            |

## Features

- Multiple clients can connect to the server concurrently.
- Unique user nicknames with presence status (`online`, `away`, `busy`).
- Broadcast messaging and real-time chat.
- File transfer capability between clients and server.
- Command system including:
  - `/list` - List all connected users with their statuses.
  - `/status <new_status>` - Update your user status.
  - `/sendfile <filename>` - Send a file to the server.
  - `/quit` - Disconnect from the server.
  - `/help` - Display command list.
- Timestamped messages for clarity.
- Thread-safe client management with mutex locks.
- Simple, clean user interface in the terminal.
- Modular codebase with easy extensibility.

---

## Requirements

- GCC or compatible C compiler.
- POSIX-compliant OS (Linux, macOS).
- Make utility for building.

---

## Building

The project uses a Makefile and outputs binaries into the `build/` directory.

```bash
make
