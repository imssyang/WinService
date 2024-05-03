```bash
nmake gui /E DEBUG=1
winsvc install -a -n frps -s frps -d "A TEST" -p "O:\20-Program\frp\frps.exe -c O:\20-Program\frp\frps.ini"
winsvc uninstall frps
winsvc start frps
winsvc stop frps
winsvc list -fp RunAs
```
