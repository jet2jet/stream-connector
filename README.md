# stream-connector

Transfers data between sockets and pipes.

- [Usage](#usage)
- [Options](#options)
- [Examples](#examples)
- [Build](#build)
- [License](#license)

## Usage

Executes with listener and connector options (see below). When the program starts, the icon will be appear on the taskbar. The status window will be shown when double-clicking the taskbar icon.

To exit, use 'Exit' command on the context menu of the taskbar icon.

## Options

> `--listener` (`-l`) and `--connector` (`-c`) are required. If specified options are not valid, the usage dialog will be shown.

```
Usage: stream-connector.exe <options>

<options>:
  -h, -?, --help : Show this help
  -l <listener>, --listener <listener> : [Required] Add listener (can be specified more than one)
  -c <connector>, --connector <connector> : [Required] Set connector
  -n <name>, --name <name> : User-defined name
  --log <level> : Set log level
    <level>: error, info, debug (default: error)
  --wsl-socat-log-level <level> : Set log level for WSL socat
    <level>: 0 (nothing), 1 (-d), 2 (-dd), 3 (-ddd), 4 (-dddd) (default: 0)
  --wsl-timeout <millisec> : Set timeout for WSL preparing (default: 30000)

<listener>:
  tcp-socket [-4 | -6] [<address>:]<port> : TCP socket listener (port num. can be 0 for auto-assign)
    alias for 'tcp-socket': s, sock, socket, tcp
  unix-socket [--abstract] <file-path> : Unix socket listener (listener with the socket file)
    alias for 'unix-socket': u, unix
  cygwin-sockfile <win-file-path> : Listener with Cygwin-spec socket file
    alias for 'cygwin-sockfile': c
  pipe <pipe-name> : Named-pipe listener
    alias for 'pipe': p
  wsl-tcp-socket [-d <distribution>] [-4 | -6] [<address>:]<port> : TCP socket listener in WSL (port num. can be 0 for auto-assign)
    alias for 'wsl-tcp-socket': ws, wt
  wsl-unix-socket [-d <distribution>] <wsl-file-path> : Unix socket listener in WSL (listener with the socket file in WSL)
    alias for 'wsl-unix-socket': wu

<connector>:
  tcp-socket <address>:<port> : TCP socket connector (port num. cannot be 0)
  unix-socket [--abstract] <file-name> : Unix socket connector
  pipe <pipe-name> : Named-pipe connector
  wsl-tcp-socket [-d <distribution>] <address>:<port> : TCP socket connector in WSL (port num. cannot be 0)
  wsl-unix-socket [-d <distribution>] [--abstract] <wsl-file-path> : Unix socket connector in WSL
```

### -h, -?, --help

Shows usage and exit.

### -l &lt;listener&gt;, --listener &lt;listener&gt;

> (Required option)

Specifies 'listener', the data source to transfer. The listener can be specified more than one; multiple listeners are allowed. The listeners are created when the program starts.

The followings are the listeners which can be specified as `<listener>`.

#### tcp-socket \[-4 | -6\] \[&lt;address&gt;:\]&lt;port&gt;

> Alias for `tcp-socket`: `s` `sock` `socket` `tcp`

Creates TCP socket and listens with specified address and port.

- If `-4` is specified, IPv4 is used.
- If `-6` is specified, IPv6 is used.
- If `-4` or `-6` is not specified and `<address>` starts with `[`, IPv6 is used; otherwise IPv4 is used.

If `<address>` is omitted, `127.0.0.1` or `[::1]` is used.

#### unix-socket \[--abstract\] &lt;file-path&gt;

> Alias for `unix-socket`: `u` `unix`

Creates Unix socket and listens with specified file. If `--abstract` (or `-a`) is specified, the socket is bound as 'abstract'.  
The file specified in `<file-path>` must writable and not be exist. When program exits, the file will be removed.

Note: Unix socket doesn't work well for some Windows 10 version (at least Build 19042 is required)

#### cygwin-sockfile &lt;file-name&gt;

> Alias for `cygwin-sockfile`: `c`

Creates 'Cygwin'-based Unix socket and listens with specified file. Unlike 'unix-socket', the file is created as a regular file which emulates Unix socket for Cygwin or MSYS2. When program exits, the file will be removed.

Note: This option does *not* require Cygwin or MSYS2 environments.

#### pipe &lt;pipe-name&gt;

> Alias for `pipe`: `p`

Creates named pipe and waits for connected. The pipe name (`<pipe-name>`) must start with `\\.\pipe\`.

#### wsl-tcp-socket \[--distribution &lt;distro&gt;\] \[-4 | -6\] \[&lt;address&gt;:\]&lt;port&gt;

> Alias for `wsl-tcp-socket`: `ws` `wt`

> Not supported for x86 version.

Creates TCP socket and listens with specified address and port on WSL environment.

**Note: [socat](http://www.dest-unreach.org/socat/) must be installed on specified WSL environment.** Also, WSL must be installed on Windows. :)

- `--distribution <distro>` (or `-d <distro>`) : Specifies existing distribution name to listen.
- If `--distribution` (or `-d`) is omitted, the default distribution is used.
- If `-4` is specified, IPv4 (`tcp4-listen`) is used.
- If `-6` is specified, IPv6 (`tcp6-listen`) is used.
- If `-4` or `-6` is not specified and `<address>` starts with `[`, IPv6 is used; otherwise IPv4 is used.

If `<address>` is omitted, `127.0.0.1` or `[::1]` is used.

#### wsl-unix-socket \[--distribution &lt;distro&gt;\] \[&lt;wsl-file-path&gt;\]

> Alias for `wsl-unix-socket`: `wu`

> Not supported for x86 version.

Creates Unix socket and listens with specified file on WSL environment.

**Note: [socat](http://www.dest-unreach.org/socat/) must be installed on specified WSL environment.** Also, WSL must be installed on Windows. :)

- `--distribution <distro>` (or `-d <distro>`) : Specifies existing distribution name to listen.
- If `--distribution` (or `-d`) is omitted, the default distribution is used.
- The file path `<wsl-file-path>` must be the valid file path on the WSL environment. The file will be removed when the program exits.

### -c &lt;connector&gt;, --connector &lt;connector&gt;

> (Required option)

Specifies 'connector', the target to transfer. The connector is created when the listener(s) accepts; not when the program starts.

The followings are the connectors which can be specified as `<connector>`.

#### tcp-socket &lt;address&gt;:&lt;port&gt;

> Alias for `tcp-socket`: `s` `sock` `socket` `tcp`

Creates TCP socket and connects with specified address and port.

#### unix-socket \[--abstract\] &lt;file-path&gt;

> Alias for `unix-socket`: `u` `unix`

Creates Unix socket and connects with specified file. If `--abstract` (or `-a`) is specified, the socket is bound as 'abstract'.  
The file specified in `<file-path>` must writable and not be exist. When program exits, the file will be removed.

Note: Unix socket doesn't work well for some Windows 10 version (at least Build 19042 is required)

#### pipe &lt;pipe-name&gt;

> Alias for `pipe`: `p`

Opens named pipe. The pipe name (`<pipe-name>`) must start with `\\.\pipe\`.

#### wsl-tcp-socket \[--distribution &lt;distro&gt;\] &lt;address&gt;:&lt;port&gt;

> Alias for `wsl-tcp-socket`: `ws` `wt`

> Not supported for x86 version.

Creates TCP socket and connects with specified address and port on WSL environment.

- `--distribution <distro>` (or `-d <distro>`) : Specifies existing distribution name to listen.
- If `--distribution` (or `-d`) is omitted, the default distribution is used.

**Note: [socat](http://www.dest-unreach.org/socat/) must be installed on specified WSL environment.** Also, WSL must be installed on Windows. :)

#### wsl-unix-socket \[--distribution &lt;distro&gt;\] \[&lt;wsl-file-path&gt;\]

> Alias for `wsl-unix-socket`: `wu`

> Not supported for x86 version.

Creates Unix socket and connects with specified file on WSL environment.

- `--distribution <distro>` (or `-d <distro>`) : Specifies existing distribution name to listen.
- If `--distribution` (or `-d`) is omitted, the default distribution is used.
- The file path `<wsl-file-path>` must be the valid file path on the WSL environment. The file will be removed when the program exits.

**Note: [socat](http://www.dest-unreach.org/socat/) must be installed on specified WSL environment.** Also, WSL must be installed on Windows. :)

### -n &lt;name&gt;, --name &lt;name&gt;

Specifies any user-defined name. This name is used for the taskbar icon name and the window title, so you can use this option for distinguishing stream-connector programs executed with different options.

### --log &lt;level&gt;

Specifies the log level (logged on the status window). Valid `<level>` values are: `error` (default; only logged for errors), `info` (a bit verbose), `debug` (more verbose)

### --wsl-timeout &lt;millisec&gt;

Specifies the timeout value for preparing WSL processes (default: 30000)

### --wsl-socat-log-level &lt;level&gt;

> Alias: `--wsl-socat-log`

Specifies the log-level value for WSL socat. Following values are valid:

- 0 : Default logs (default)
- 1 : `-d`
- 2 : `-dd`
- 3 : `-ddd`
- 4 : `-dddd`

> Note: `-d`, `-dd`, `-ddd`, and `-dddd` can be used as `<level>` value.

### -x &lt;proxy-id&gt;, --proxy &lt;proxy-id&gt;

Used internally.

## Examples

```
stream-connector -l tcp-socket 127.0.0.1:3001 -c tcp-socket my-host.mydomain:3001
```

Listens port 3001 (for 127.0.0.1) and transfers to `my-host.mydomain:3001`.

> Note: To make simple port forwardings, use `netsh interface portproxy` rather than this program.

```
stream-connector -l wsl-tcp-socket 6000 -c tcp-socket localhost:6000
```

Listens port 6000 on WSL environment (using default distribution) and transfers to `localhost:6000`.

```
stream-connector -l cygwin-sockfile C:\Temp\my-agent.sock -l wsl-unix-socket -d Ubuntu-20.04 /tmp/my-agent.sock -c pipe \\.\pipe\openssh-ssh-agent
```

Creates `my-agent.sock` files both on C:\Temp (as Cygwin socket) and on /tmp of WSL `Ubuntu-20.04` environment, and transfers received data to the pipe `\\.\pipe\openssh-ssh-agent`. This allows to forward SSH agent from Cygwin/MSYS2 based SSH clients and WSL SSH clients to Windows OpenSSH agent.

## Build

Use Visual Studio 2019 or Build Tools (using MSBuild).

```
msbuild /p:Configuration=Debug,Platform=x64 stream-connector.vcxproj
```

## License

[BSD 3-Clause License](./LICENSE)
