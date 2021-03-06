# SO-shell-etap3
*Specyfikacja trzeciego etapu projektu realizowanego w ramach ćwiczeń do przedmiotu Systemy Operacyjne.*

## Implementacja shella - etap 3 (wbudowane polecenia shella).


Zanim wykonamy polecenie jako program sprawdzamy czy nie jest to polecnie shella. Polecenia shella znajdują się w tabeli `builtins_table`  w pliku `builtins.c`. Wpis składa się z nazwy polecenia i wskaźnika do funkcji realizującej polecenie. Argumentem funkcji jest tablica argumentów analogiczna do `argv`. Należy zaimplementować następujące polecenia:
- `exit` - kończy proces shella.
- `lcd [path]` - zmienia bieżący katalog na wskazany. Jeśli polecenie jest wywołane bez argumentu katalog powinien być zmieniony na wartość zmiennej środowiskowej `HOME`.
- `lkill [ -signal_number ] pid` - wysyła sygnał signal_number do procesu/grupy pid. Domyślny numer sygnału to `SIGTERM`.
- `lls` - wypisje nazwy plików w bieżącym katalogu (analogicznie do `ls -1` bez żadnych opcji). Zawartość katalogu powinna być wypisana w kolejności zwracanej przez system. Nazwy zaczynające się od `.` powinny być ignorowane.

W przypadku jakichkolwiek problemów z wykonaniem komendy (niewłaściwa liczba lub format argumentów, niepowodzenie syscalla itp.) należy wypisać na STDERR:
```
Builtin 'nazwa_komendy' error.
```


Przykład sesji:
```
sleep 10000 &
sleep 10001 &
./mshell
$ pwd
/home/dude
$ lcd /etc
$ pwd
/etc
$ lcd ../home
$ pwd
/home
$ lls
bin
ast
dude
$ lcd /
$ lls
usr
boot.cfg
bin
boot
boot_monitor
dev
etc
home
lib
libexec
mnt
proc
root
sbin
sys
tmp
var
$ lcd
$ pwd
/home/dude
$ cd ../../usr/src/etc
$ pwd
/usr/src/etc
$ ps
 PID TTY  TIME CMD
...
 2194  p1  0:00 sleep TERM=xterm
 2195  p1  0:00 sleep TERM=xterm
...
$ lkill -15 2194
$ ps
  PID TTY  TIME CMD
...
 2195  p1  0:00 sleep TERM=xterm
...
$ lkill 2195
$ ps
  PID TTY  TIME CMD
  ...
$ lcd /CraneJacksonsFountainStreetTheatre
Builtin lcd error.
$ lkill
Builtin lkill error.
$ exit
```
Syscall checklist: `exit`, `chdir`, `kill`, `readdir`, `opendir/fdopendir`, `closedir`.

Testy zostały rozbudowane o zestaw 2 obejmujący wywoływanie poleceń shella.
