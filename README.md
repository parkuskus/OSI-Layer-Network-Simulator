# MAGI System: OSI Layer Network Simulator

<p align="center">
  <img src="https://raw.githubusercontent.com/TomaszRewak/MAGI/master/examples/example_1.gif" width=800/>
</p>

**STATUS: DARURAT LEVEL 1 (SITUASI MERAH)**

**LOKASI: Markas Besar NERV, Tokyo-3 (Geofront)**

Malaikat ke-11, Ireul, telah menginfeksi dan menghancurkan seluruh protokol komunikasi standar NERV. Komandan Ikari telah memberikan mandat mutlak: kru teknis elit Divisi Jaringan harus membangun kembali seluruh sistem komunikasi MAGI dari titik nol. Kehancuran Tokyo-3 menanti jika ada satu *bit* saja yang salah dalam implementasi ini.

## Deskripsi Singkat

MAGI System adalah simulator jaringan edukasional yang mengimplementasikan lapisan OSI mulai L2 sampai L7 dalam bentuk modular. Didesain untuk percobaan topologi, pengujian protokol (ARP, IPv4, TCP/UDP) dan layanan aplikasi kecil (DHCP, DNS, HTTP) lewat antarmuka baris perintah interaktif.

## Prerequisites

- Compiler C++ (g++ atau clang) dengan dukungan C++11
- `make` (Linux/macOS) atau `mingw32-make` (Windows/MinGW)
- Shell/terminal untuk menjalankan binari

Catatan: proyek tidak memerlukan dependensi eksternal khusus; cukup compiler standar dan Makefile.

## Fitur Program

1) Manajemen Topologi
- Fungsi: pembuatan node (host, switch, router), konfigurasi port, pengkabelan antar-port, serta simpan/muat topologi ke/dari file JSON.
- Perintah: `create`, `link`, `unlink`, `save`, `load`, `topology`, `show`.
- Catatan: `create` menerima tipe `host|switch|router`; switch dapat dibuat dengan jumlah port tertentu (`create SW1 switch 4`). `link` menerima endpoint berupa `Node` atau `Node:Port` dan dapat diberi argumen delay (ms) untuk simulasi latensi.

2) Data Link Layer (L2)
- Fungsi: pemrosesan frame Ethernet, MAC learning, forwarding, flooding, dan dukungan VLAN (access/trunk).
- Perintah/konfigurasi: `vlan access`, `vlan trunk`, `mac` (tampilkan tabel MAC per-switch).
- ARP: host mengirim ARP request/reply untuk resolusi alamat MAC; ARP cache disimpan per-host dengan TTL sederhana.

3) Network Layer (L3)
- Fungsi: routing IPv4 (longest-prefix match), pengelolaan TTL, penghitungan checksum, dan pembuatan pesan ICMP (mis. destination unreachable, time exceeded).
- Router memiliki tabel rute yang dapat dimodifikasi via `route` dan `route add`.

4) Transport Layer (L4)
- TCP:
  - Implementasi state machine lengkap untuk handshake (3-way), pengiriman data (PSH), dan teardown (FIN/ACK).
  - Buffer penerimaan sederhana, retransmission tidak lengkap (didaktik), dan API `tcp_connect` untuk uji koneksi.
- UDP:
  - Datagram tanpa koneksi, digunakan oleh layanan seperti DHCP dan DNS internal.
  - CLI: `udp_send` untuk mengirim payload UDP manual dari host.

5) Application Layer (L7)
- `MagiSocket`:
  - Abstraksi soket L7 di atas implementasi TCP/UDP simulator; menyediakan API connect/accept/send/recv untuk aplikasi.
- DHCP:
  - Server/client sederhana yang mendemonstrasikan DORA (Discover/Offer/Request/Ack) dan alokasi alamat dinamis ke host.
- DNS:
  - Resolver dan server UDP-based minimal yang memetakan nama host dalam topologi ke alamat IP; digunakan oleh `http_get` untuk penyelesaian nama.
  - Catatan: bukan implementasi RFC penuh, tetapi cukup untuk skenario lab dan test integrasi.
- HTTP:
  - Server statis yang menyajikan file (mis. `index.html`) dari host; client melakukan GET menggunakan resolver DNS internal.
  - Server/client berinteraksi lewat `MagiSocket` dan menunjukkan alur TCP 3-way handshake, request/response, dan teardown.

6) CLI Interaktif dan Automation
- CLI menyediakan mode interaktif untuk menjalankan skenario manual, serta perintah yang dapat digunakan di script untuk otomatisasi (load/save topologi + run commands).

7) Test Suite
- Folder `test/` berisi test per milestone; ada tes integrasi L7 (contoh: DNS->HTTP) yang dapat dijalankan untuk memverifikasi alur end-to-end.

8) Extensibility
- Direktori `middleboxes/` dan `utils/` disediakan untuk menempatkan eksperimen tambahan seperti firewall sederhana, NAT, atau middlebox lain untuk eksperimen lebih lanjut.



## Cara Build dan Run 

Build dan jalankan dari direktori `magi_system`.

Linux / macOS

```bash
cd magi_system
make            # membangun executable
./magi_system   # jalankan CLI interaktif
```

Windows (MinGW)

```powershell
cd magi_system
mingw32-make
.\magi_system.exe
```

Catatan:
- Jika Makefile menyediakan target `makerun`, Anda bisa menggunakan `make makerun` atau `mingw32-make makerun` sebagai shortcut.
- Untuk rebuild bersih: `make clean && make` (atau sesuai target di Makefile).

## Struktur Proyek 

```
magi_system/
├─ Makefile
├─ main.cpp
├─ cli.cpp
├─ cli.hpp
├─ topology.json
├─ magi_system            # executable / runtime binary (build target)
├─ build/                # objek dan dependensi yang dihasilkan
├─ core/
│  ├─ interface.cpp/.hpp
│  ├─ link.cpp/.hpp
  │  ├─ node.cpp/.hpp
  │  └─ packet.cpp/.hpp
├─ gui/
├─ layer2/
│  ├─ arp.cpp/.hpp
│  ├─ ethernet.cpp/.hpp
│  ├─ host.cpp/.hpp
│  └─ switch.cpp/.hpp
├─ layer3/
│  ├─ icmp.cpp/.hpp
│  ├─ ipv4.cpp/.hpp
│  └─ ip_utils.hpp
├─ layer4/
│  ├─ tcp.cpp/.hpp
│  ├─ tcp_socket.cpp/.hpp
│  └─ udp.cpp/.hpp
├─ layer7/
│  ├─ dhcp_*.cpp/.hpp
│  ├─ dns_*.cpp/.hpp
│  ├─ http_*.cpp/.hpp
│  └─ magi_socket.cpp/.hpp
├─ middleboxes/
├─ test/
│  ├─ test_common.hpp
│  ├─ test_main.cpp
│  └─ milestone-1/..-4/   # suite tests per milestone
├─ utils/

```

## Perintah yang Didukung

Topologi
- `create <name> <host|switch|router> [ports]` — buat node
- `link <endpointA> <endpointB> [delay_ms]` — hubungkan endpoint (`H1`, `SW1:2`)
- `unlink <endpointA> <endpointB>` — hapus link
- `topology` — tampilkan ringkasan topologi
- `show <node>` — tampilkan detail node

Konfigurasi alamat & routing
- `setip <host> <ip/cidr>` — pasang alamat pada host
- `setgw <host> <gateway_ip>` — set default gateway
- `route <router>` — tampilkan tabel rute router
- `route add <router> <dest_cidr> <next_hop_ip> <out_interface>` — tambah rute

VLAN dan Switch
- `vlan access <switch> <port> <vlan_id>`
- `vlan trunk <switch> <port> <native_vlan>`
- `mac <switch>` — tampilkan MAC table

ARP / L2 utilities
- `arp <host>` — tampilkan ARP cache host

Pengujian & monitoring
- `ping <host> <target_ip>`
- `traceroute <host> <target_ip>`
- `tcp_connect <host> <ip> <port>` — coba koneksi TCP
- `udp_send <host> <ip> <src_port> <dst_port> [payload]` — kirim UDP

Layanan L7 (control)
- `http_get <host> <hostname>` — HTTP client yang menggunakan resolver internal
- `http_server start <host> <file>` / `http_server stop <host>`
- `dns_server start <host>` / `dns_server stop <host>`
- `dhcp_server start <host>` / `dhcp_server stop <host>`
- `dhcp_discover <host>` — jalankan DHCP client discovery dari host

File I/O
- `save [file]` — simpan topologi (default: `topology.json`)
- `load [file]` — muat topologi dari file

Umum
- `help` — daftar perintah
- `exit` / `quit` — keluar

Catatan: Layanan L7 (DHCP/DNS/HTTP) adalah implementasi didaktik untuk kebutuhan lab dan integrasi (bukan implementasi RFC penuh). Lihat folder `test/` untuk test-suite per milestone.

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
* [X] **Milestone 4: Application Layer (L7)** - Pembuatan *Wrapper API* `MagiSocket` untuk mengabstraksi komunikasi OS, serta perakitan layanan mandiri DHCP, DNS, dan server HTTP.
* [ ] **Milestone 5: Fitur Bonus** - [Sebutkan fitur lanjutan yang kelompok Anda targetkan, misal: *Topology Visualizer*, *IP Fragmentation*, *ACL*, *NAT/PAT*, *RIPv2*, atau *Asynchronous Engine*].

## Pembagian Tugas

* **Muhammad Aufar Rizqi Kusuma (13524061):** Milestone 4
* **Kurt Mikhael Purba (13524065):** Milestone 0
* **Bryan Pratama Putra Hendra (13524067):** Milestone 2
* **Philipp Hamara (13524101):** Milestone 1, 3
