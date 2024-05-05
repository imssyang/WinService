<a href="https://github.com/imssyang/WinService">
  <h1 align="center">
    <p>WinService</p>
  </h1>
</a>

WinService is a small tool to manage Windows Services that inspired by [srvman](https://sysprogs.com/legacy/tools/srvman). The functionality of Services in windows is too weak, and does not support addition or deletion, and needs special adaptation for api of win32 kernel. SrvMan solved these problems by treating the console program as a child process of srvman, but its GUI is too simple to lack filtering, and I usually only care about services that run as subprocess, so this service was born.

## Feature

-   Add or delete Win32 services.
-   Agent arbitrary Win32 application as service, and redirect console output to log file.
-   Support filter services.

## Dependencies

-   [imgui](https://github.com/ocornut/imgui): A bloat-free graphical user interface library for C++.
-   [spdlog](https://github.com/gabime/spdlog): Very fast, header-only/compiled, C++ logging library.
-   [argparse](https://github.com/p-ranav/argparse): Argument Parser for Modern C++17.

### Commands

Help infomation in console:

```bash
Usage: winsvc [-h] {/RunAsService,install,list,start,stop,uninstall}

Subcommands:
  /RunAsService Agent program as service.
  install       Install command as service.
  list          List service.
  start         Start service.
  stop          Stop service.
  uninstall     Uninstall service.
```

Agent application as service:

```bash
winsvc install -a -n <name> -s <alias> -d <description> -p <COMMAND>
```

Delete service:

```bash
winsvc uninstall <name>
```

Start service:

```bash
winsvc start <name>
```

Stop service:

```bash
winsvc stop <name>
```

Development in visual studio 2019+ (/E DEBUG=1):

```bash
nmake cmd
nmake gui
```

## Todo

-   Show log in GUI
