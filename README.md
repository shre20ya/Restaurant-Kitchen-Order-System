# Restaurant Kitchen Order Queue System - Web Edition (C)

The whole backend is written in C: a small HTTP server built on sockets,
a priority queue made of linked lists, and file storage. The browser shows a
modern dashboard (HTML/CSS/JS in the `web/` folder) that talks to the C server.

## Features (same as the console menu)
| # | Feature | Where in the website |
|---|---------|----------------------|
| 1 | Place new order | **+ New Order** button (shortcut `N`) |
| 2 | View pending queue | **Pending Queue** column (VIP first, wait estimates) |
| 3 | Start cooking next order | **Start Next** button (shortcut `S`) |
| 4 | View cooking orders | **Cooking** column with live timer and progress bar |
| 5 | Mark order READY | **Mark READY** button on a cooking card |
| 6 | Serve order | **Serve** button on a ready card |
| 7 | Cancel pending order | **Cancel order** on a pending card |
| 8 | View menu / add item | **Menu** tab |
| 9 | Daily report | **Daily Report** tab |
| 0 | Exit | **Exit** button (saves and stops the server) |

## Folder structure
```
kitchen-web/
├── include/  types.h queue.h menu.h orders.h storage.h strbuf.h api.h server.h
├── src/      main.c server.c api.c orders.c queue.c menu.c storage.c strbuf.c
├── web/      index.html style.css app.js
├── .vscode/  tasks.json launch.json c_cpp_properties.json
├── Makefile
└── README.md
```

## Run in VS Code
1. Install GCC (Windows: MinGW-w64 / MSYS2, Linux: `sudo apt install build-essential`, macOS: `xcode-select --install`).
2. Install the **C/C++** extension in VS Code.
3. File > Open Folder > choose the `kitchen-web` folder (important: the folder that contains `web/`).
4. Build: `Ctrl+Shift+B`.
5. Run in the VS Code terminal:
   - Windows PowerShell: `.\kitchen`
   - Linux / macOS / Git Bash: `./kitchen`
   (or: Terminal > Run Task > **run kitchen server**)
6. Your browser opens at http://127.0.0.1:8080 . If it does not, open that link yourself.

Options: `kitchen 9090` uses another port, `kitchen --no-browser` does not open the browser.

Without VS Code tasks:
```
gcc -Wall -Wextra -Iinclude src/*.c -o kitchen            (Linux / macOS)
gcc -Wall -Wextra -Iinclude src/*.c -o kitchen -lws2_32   (Windows, MinGW; PowerShell may need the file list typed out)
```

## How it works
```
Browser (web/)  --HTTP-->  server.c  -->  api.c  -->  orders.c  -->  queue.c (linked lists)
                                                         |                |
                                                    menu.c            storage.c  -->  kitchen.dat
```
- `server.c` - sockets, request parsing, static files (Winsock on Windows, BSD sockets elsewhere)
- `api.c` - routes `/api/...` and builds JSON with `strbuf.c`
- `orders.c` - business rules (priority, 2 chefs, validation, statistics)
- `queue.c` - priority queue: VIP before Normal, first come first served inside a priority
- `storage.c` - saves everything to `kitchen.dat` after every change

## Notes
- Data is saved automatically; delete `kitchen.dat` for a fresh start.
- The server only listens on 127.0.0.1, so it is reachable from your own computer only.
- Change the currency symbol in `web/app.js` (`CURRENCY`) and default prices in `src/menu.c`.
