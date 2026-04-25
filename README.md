# MAGI System: OSI Layer Network Simulator

***(Silahkan merombak file README.md ini sepuasnya kalian)***

<p align="center">
  <img src="https://raw.githubusercontent.com/TomaszRewak/MAGI/master/examples/example_1.gif" width=800/>
</p>

**STATUS: DARURAT LEVEL 1 (SITUASI MERAH)**

**LOKASI: Markas Besar NERV, Tokyo-3 (Geofront)**

Malaikat ke-11, Ireul, telah menginfeksi dan menghancurkan seluruh protokol komunikasi standar NERV. Komandan Ikari telah memberikan mandat mutlak: kru teknis elit Divisi Jaringan harus membangun kembali seluruh sistem komunikasi MAGI dari titik nol. Kehancuran Tokyo-3 menanti jika ada satu *bit* saja yang salah dalam implementasi ini.

## Prerequisites & Setup

* **Bahasa Pemrograman:** [Isi dengan bahasa pilihan kelompok Anda, e.g., Python, C++, Go].
* Pilihan bahasa pemrograman memengaruhi *Language Multiplier* pada penilaian akhir.
* Penggunaan *Large Language Models* (LLM) diizinkan sebagai asisten belajar, namun setiap baris kode wajib dipahami seutuhnya.
* Gagal menjelaskan alur eksekusi saat demonstrasi akan dianggap sebagai plagiarisme.
* **DILARANG** menggunakan API jaringan bawaan sistem operasi atau *library* simulasi jaringan pihak ketiga.

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

```

magi_system/

├── Makefile              # Build script

├── README.md             # Dokumentasi ini

├── main.cpp              # Entry point program

├── cli.cpp/hpp           # Command Line Interface

└── core/                 # Milestone 0: Fondasi

    ├── packet.cpp/hpp    # Base class untuk serialize/deserialize

    ├── interface.cpp/hpp # Network interface/port

    ├── link.cpp/hpp      # Virtual cable dengan delay

    └── node.cpp/hpp      # Base class: Host, Switch, Router

```

## Perintah CLI

### Manajemen Topologi

-`create <name> <host|switch|router> [jumlah_port]` - Membuat node baru

-`link <device1> <device2> [delay_ms]` - Menghubungkan dua device

-`unlink <device1> <device2>` - Memutuskan koneksi

-`topology` - Menampilkan topologi

-`show <node_name>` - Menampilkan info node

### File Operations

-`save [filename]` - Menyimpan topologi ke JSON (default: topology.json)

-`load [filename]` - Memuat topologi dari JSON

### General

-`help` - Menampilkan bantuan

-`exit` / `quit` - Keluar dari simulator

## Format Endpoint

-`NodeName` - Untuk host (asumsi port 1)

-`NodeName:Port` - Untuk switch/router (contoh: SW1:1, R1:2)

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

## Catatan Implementasi Milestone 0

### Interface & Link

- Interface merepresentasikan port/kartu jaringan pada Node
- Link merepresentasikan kabel virtual dengan properti delay
- Koneksi bersifat blocking (sinkron) dengan simulasi propagation delay

### Serialization & Deserialization

- Kelas `Packet` menyediakan interface untuk konversi objek ke byte-array
- Setiap protokol layer akan meng-extend kelas ini

### MAC Address

- MAC address di-generate otomatis untuk setiap interface
- Format: AA:BB:CC:DD:EE:FF (6 bytes dalam hex)

### Node Types

-**Host**: Memiliki 1 interface secara default, dengan IP dan default gateway

-**Switch**: Memiliki N interface (default 24), mendukung VLAN (Milestone 1)

-**Router**: Memiliki interface dinamis, mendukung routing table (Milestone 2)

Centang (*checklist*) kotak di bawah ini sesuai dengan *layer* yang telah kelompok kalian selesaikan:

* [X] **Milestone 0: Fondasi Simulasi** - Pembuatan kelas fisik (*Interface*, *Link*), struktur dasar *Packet* yang mendukung konversi ke *byte* mentah, dan memuat topologi JSON.
* [ ] **Milestone 1: Data Link Layer (L2)** - Implementasi *Ethernet Frame*, logika *Switching* (*VLAN-aware*), dan antrean IP Packet menggunakan *ARP Cache*.
* [ ] **Milestone 2: Network Layer (L3)** - Implementasi resolusi *Longest Prefix Match Routing*, *Inter-VLAN Routing*, modifikasi parameter TTL, kalkulasi *Checksum* IPv4, dan pengiriman *ICMP Error Messages*.
* [ ] **Milestone 3: Transport Layer (L4)** - Penyusunan *State Machine* TCP (*3-Way Handshake*, *Receive Buffers*, *4-Way Teardown*), protokol UDP, dan kalkulasi *Pseudo-Header*.
* [ ] **Milestone 4: Application Layer (L7)** - Pembuatan *Wrapper API* `MagiSocket` untuk mengabstraksi komunikasi OS, serta perakitan layanan mandiri DHCP, DNS, dan server HTTP.
* [ ] **Milestone 5: Fitur Bonus** - [Sebutkan fitur lanjutan yang kelompok Anda targetkan, misal: *Topology Visualizer*, *IP Fragmentation*, *ACL*, *NAT/PAT*, *RIPv2*, atau *Asynchronous Engine*].

## Pembagian Tugas

[Deskripsikan dengan jelas anggota kelompok dan *milestone* yang mereka kerjakan, ini wajib diisi sesuai instruksi pengumpulan repositori.]

* **Anggota 1 (NIM):** [Bagian yang dikerjakan]
* **Anggota 2 (NIM):** [Bagian yang dikerjakan]
* **Anggota 3 (NIM):** [Bagian yang dikerjakan]
* **Anggota 4/5 (NIM):** [Bagian yang dikerjakan]
