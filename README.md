# HTTP nástenka

## Autor: Adam Abrahám (xabrah04)

### Dátum: 18.11.2019

### Popis aplikácie

Aplikácia umožnuje klientom spravovať nástenky na serveri pomocou HTTP API. API im umožnuje prezerať, pridávať, upravovať a mazať príspevky na nástenkách ako aj nástenky samotné. Nástenka obsahuje zoradený zoznam textových príspevkov.

### Návod na použitie aplikácie

Aplikácie sa prekladajú pomocou prideleného Makefile súboru, pomocou príkazu `make all`.

Oba programy po spustení s parametrom -h vypíšu na stdout informácie o spôsobe spustenia.

./isaserver -p `<port>`
Príklad: ./isaserver -p 5777

./isaclient -H `<host>` -p `<port>` `<command>`

Commands:

- boards - GET /boards
- board add `<name>` - POST /boards/`<name>`
- board delete `<name>` - DELETE /boards/`<name>`
- board list `<name>` - GET /board/`<name>`
- item add `<name>` `<content>` - POST /board/`<name>`
- item delete `<name>` `<id>` - DELETE /board/`<name>`/`<id>`
- item update `<name>` `<id>` `<content>` - PUT /board/`<name>`/`<id>`

Príklad: ./isaclient -H localhost -p 4242 boards

### Zoznam odovzdaných súborov

- `Makefile`
- `isaclient.c`
- `isaserver.c`
- `manual.pdf`
- `README.md`
