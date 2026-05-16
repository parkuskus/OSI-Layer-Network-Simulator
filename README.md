# MAGI System: OSI Layer Network Simulator

***(Silahkan merombak file README.md ini sepuasnya kalian)***

<p align="center">
  <img src="https://raw.githubusercontent.com/TomaszRewak/MAGI/master/examples/example_1.gif" width=800/>
</p>

**STATUS: DARURAT LEVEL 1 (SITUASI MERAH)**

**LOKASI: Markas Besar NERV, Tokyo-3 (Geofront)**

Malaikat ke-11, Ireul, telah menginfeksi dan menghancurkan seluruh protokol komunikasi standar NERV. Komandan Ikari telah memberikan mandat mutlak: kru teknis elit Divisi Jaringan harus membangun kembali seluruh sistem komunikasi MAGI dari titik nol. Kehancuran Tokyo-3 menanti jika ada satu *bit* saja yang salah dalam implementasi ini.

## Prerequisites & Setup

* **Bahasa Pemrograman:** C++

## Daftar Periksa Pencapaian (Milestones)

## Cara Build dan Run

### Linux/macOS

```bash

make

makerun

```

### Windows (dengan MinGW)

```bash

mingw32-make

mingw32-makerun

```

Atau langsung:

```bash

makerun

```

### Manual Compile

```bash

g++-std=c++11-omagi_systemmain.cppcli.cppcore/packet.cppcore/interface.cppcore/link.cppcore/node.cpp

./magi_system

```

## Struktur Proyek

```text
magi_system/
├── Makefile
├── main.cpp
├── cli.cpp
├── cli.hpp
├── topology.json
├── core/
│   ├── packet.cpp / packet.hpp
│   ├── interface.cpp / interface.hpp
│   ├── link.cpp / link.hpp
│   └── node.cpp / node.hpp
├── layer2/
│   ├── ethernet.cpp / ethernet.hpp
│   ├── arp.cpp / arp.hpp
│   ├── host.cpp / host.hpp
│   └── switch.cpp / switch.hpp
├── layer3/
│   ├── ipv4.cpp / ipv4.hpp
│   ├── icmp.cpp / icmp.hpp
│   └── ip_utils.hpp
├── layer4/
│   ├── tcp.cpp / tcp.hpp
│   ├── tcp_socket.cpp / tcp_socket.hpp
│   └── udp.cpp / udp.hpp
└── build/
    └── ... generated object files and dependency files
```

## Fitur yang Tersedia

### Manajemen Topologi

- `create <name> <host|switch|router> [jumlah_port]`
- `link <device1> <device2> [delay_ms]`
- `unlink <device1> <device2>`
- `topology`
- `show <node_name>`

### Konfigurasi Node

- `setip <host_name> <ip/cidr>`
- `setip <router:port[.vlan]> <ip/cidr>`
- `setgw <host_name> <gateway_ip>`
- `vlan access <switch_name> <port> <vlan_id>`
- `vlan trunk <switch_name> <port> <native_vlan>`
- `route <router_name>`
- `route add <router_name> <dest_cidr> <next_hop_ip> <out_interface>`

### Monitoring dan Pengujian

- `mac <switch_name>` atau `<switch_name> mac`
- `arp <host_name>` atau `<host_name> arp`
- `ping <host_name> <target_ip>` atau `<host_name> ping <target_ip>`
- `traceroute <host_name> <target_ip>` atau `<host_name> traceroute <target_ip>`
- `tcp_connect <host_name> <target_ip> <port>` atau `<host_name> tcp_connect <target_ip> <port>`
 - `udp_send <host_name> <target_ip> <src_port> <dst_port> [payload]` atau `<host_name> udp_send <target_ip> <src_port> <dst_port> [payload]` — buat dan verifikasi UDP segment (lokal/test)

### File Operations

- `save [filename]` untuk menyimpan topologi ke JSON, default `topology.json`
- `load [filename]` untuk memuat topologi dari JSON

### General

* `help` - Tampilkan daftar perintah
* `exit` / `quit` - Keluar dari simulator

## Format Endpoint

* `NodeName` - Untuk host, otomatis memakai port 1
* `NodeName:Port` - Untuk switch/router, contoh `SW1:1` atau `R1:2`

## Contoh Penggunaan

```

Magi> create H1 host

Host 'H1' berhasil dibuat.


Magi> create H2 host

Host 'H2' berhasil dibuat.


Magi> create SW1 switch 4

Switch 'SW1' dengan 4 port berhasil dibuat.


Magi> link H1 SW1:1

Berhasil menghubungkan H1 dengan SW1:1.


Magi> link H2 SW1:2

Berhasil menghubungkan H2 dengan SW1:2.


Magi> topology

=== TOPOLOGY ===

Nodes:

  H1 (host)

  H2 (host)

  SW1 (switch)


Links:

  H1:1 <-> SW1:1

  H2:1 <-> SW1:2


Magi> save my_topology.json

Topologi berhasil disimpan ke 'my_topology.json'.


Magi> exit

Menghentikan Magi System Simulator...

```

## Format File JSON

```json

{

  "hosts": [

    {

      "name": "H1",

      "ip_address": "192.168.1.10/24",

      "default_gateway": "192.168.1.1"

    }

  ],

  "switches": [

    {

      "name": "SW1",

      "num_ports": 24,

      "vlans": []

    }

  ],

  "routers": [

    {

      "name": "R1",

      "interfaces": [],

      "routing_table": []

    }

  ],

  "links": [

    {

      "endpoints": ["H1", "SW1:1"],

      "delay": 0

    }

  ]

}

```

## Milestones

Centang sesuai implementasi yang sudah selesai.

* [X] **Milestone 0: Fondasi Simulasi** - Pembuatan kelas fisik (*Interface*, *Link*), struktur dasar *Packet* yang mendukung konversi ke *byte* mentah, dan memuat topologi JSON.
* [X] **Milestone 1: Data Link Layer (L2)** - Implementasi *Ethernet Frame*, logika *Switching* (*VLAN-aware*), dan antrean IP Packet menggunakan *ARP Cache*.
* [X] **Milestone 2: Network Layer (L3)** - Implementasi resolusi *Longest Prefix Match Routing*, *Inter-VLAN Routing*, modifikasi parameter TTL, kalkulasi *Checksum* IPv4, dan pengiriman *ICMP Error Messages*.
* [X] **Milestone 3: Transport Layer (L4)** - Penyusunan *State Machine* TCP (*3-Way Handshake*, *Receive Buffers*, *4-Way Teardown*), protokol UDP, dan kalkulasi *Pseudo-Header*.
* [ ] **Milestone 4: Application Layer (L7)** - Pembuatan *Wrapper API* `MagiSocket` untuk mengabstraksi komunikasi OS, serta perakitan layanan mandiri DHCP, DNS, dan server HTTP.
* [ ] **Milestone 5: Fitur Bonus** - [Sebutkan fitur lanjutan yang kelompok Anda targetkan, misal: *Topology Visualizer*, *IP Fragmentation*, *ACL*, *NAT/PAT*, *RIPv2*, atau *Asynchronous Engine*].

## Pembagian Tugas

* **Muhammad Aufar Rizqi Kusuma (13524061):** Coming soon (sibuk data)
* **Kurt Mikhael Purba (13524065):** Milestone 0
* **Bryan Pratama Putra Hendra (13524067):** Milestone 2
* **Philipp Hamara (13524101):** Milestone 1, 3
