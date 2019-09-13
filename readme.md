# MSX-Link

Организация сети между ПК и MSX c использованием технологии **Yamaha Локальная сеть, версия 3.0**. Подробнее о технологии [здесь](https://sysadminmosaic.ru/msx/yamaha_local_network).

За основу проекта взяты материалы их этих источников:
- [Сборка MSX-Link и его использование](http://cax.narod.ru/msx/msx-link/index.html)
- [Описание протокола передачи в локальной сети КУВТ](http://www.sensi.org/~tnt23/msx/index.html)

### Синтаксис 
`msx-link [-p ComPortNum] [-s StudentNo] [-<key>...] [_<command>...]  [file1] [file2] [...fileN]`

`[file1] [file2] [...fileN]` - files for binary send (auto supported formats: BAS, BIN, ROM[8|16|32])

-key(s) :
- p `<C>`  : Connect to COM-port number `<C>`,     default value  1
- s `<S>`  : Work with 'Student' number `<S>`, default value `-1`
  - -1  to all
  - 0  - to 'Teacher'                             (for <-T>est mode)
  - 1-15  - to `<S>`tudent number
- c <cmd>        : Send Basic-command <cmd> to `<S>`tudent(s) (37 symbols limit) [like '_SNDCMD  <cmd> ']
- m <msg>        : Send message <msg> to `<S>`tudent(s)       (56 symbols limit) [like '_MESSAGE <msg> ']
- C              : Send '_cpm' to `<S>`tudent(s) for switching into CPM OS
- S              : Send file(s) to CPM net-disk             (should be use with|after -C key)
- T              : Test mode - dump&reply                   (RX & TX lines should swapped!)
- v [0-2]        : Verbose mode with selected logging lvl,  default value 0
- h|H|?          : This help

_command(s):
- _send   <file> : Send Basic-file to `<S>`tudent(s) (file must be MSX Basic fmt)[like '_SEND    <file>']
- _recv   <file> : Recv Basic-program from `<S>`tudent into the <file>           [like '_RECEIVE <file>']
- _run    [rowN] : Run  Basic-program on `<S>`tudent(s) with optional start rowN [like '_RUN     <rowN>']
- _stop          : Stop Basic-program on `<S>`tudent(s)                          [like '_STOP'          ]
- _sndcmd  <cmd> : Send Basic-command <cmd> to `<S>`tudent(s) (37 symbols limit) [like '_SNDCMD  <cmd> ']
- _message <msg> : Send message <msg> to `<S>`tudent(s) (56 symbols limit)       [like '_MESSAGE <msg> ']
- _cpm           : Send '_cpm' to `<S>`tudent(s) for switching into CPM OS

<code>

Example:

`Z:\MSX-Link\bin\msx-link.exe -p 0 -m "Hi all!"`
