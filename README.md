```bash
nmake gui /E DEBUG=1
Build\gui\wsm.exe install -a -n frps2 -s frps_2 -d "A TEST" -p "O:\20-Program\frp\frps_2.exe -c O:\20-Program\frp\frps_2.ini"
Build\gui\wsm.exe uninstall frps2
Build\gui\wsm.exe start frps2
Build\gui\wsm.exe stop frps2
Build\gui\wsm.exe list -fp RunAs
```
