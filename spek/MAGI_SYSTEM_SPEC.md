# Magi System: OSI Layer Network Simulator
### Tugas Besar IF2230 Jaringan Komputer

> **Dipersiapkan oleh:** Asisten Laboratorium Sistem Terdistribusi  
> **Mulai:** Senin, 20 April 2026, 23.00 WIB  
> **Deadline:** Minggu, 24 Mei 2026, 22.30 WIB

---

## Daftar Isi

1. [Latar Belakang](#latar-belakang)
2. [Tujuan](#tujuan)
3. [Aturan dan Batasan](#aturan-dan-batasan)
4. [Format Input & Antarmuka CLI](#format-input--antarmuka-cli)
5. [Milestone 0: Fondasi Simulasi](#milestone-0-fondasi-simulasi-packet--link-core)
6. [Milestone 1: Data Link Layer (L2)](#milestone-1-data-link-layer-l2)
7. [Milestone 2: Network Layer (L3)](#milestone-2-network-layer-l3)
8. [Milestone 3: Transport Layer (L4)](#milestone-3-transport-layer-l4)
9. [Milestone 4: Socket API & Application Layer (L7)](#milestone-4-socket-api--application-layer-l7)
10. [Milestone 5: Fitur Lanjutan & Bonus](#milestone-5-fitur-lanjutan--utilitas-bonus)
11. [Skema Penilaian](#skema-penilaian-grading)
12. [Penghargaan Bahasa Pemrograman](#penghargaan-bahasa-pemrograman-language-multiplier)
13. [Bonus Kontinuitas](#protokol-sinkronisasi-berkala-bonus-kontinuitas)
14. [Pengerjaan & Deliverables](#pengerjaan--deliverables)
15. [Struktur Direktori Proyek](#struktur-direktori-proyek)

---

## Latar Belakang

```
STATUS  : DARURAT LEVEL 1 (SITUASI MERAH)
LOKASI  : Markas Besar NERV, Tokyo-3 (Geofront)
SUBJEK  : Operasi Pemulihan Pasca-Insiden Malaikat ke-11, Ireul
```

Malaikat ke-11, **Ireul**, telah berhasil menembus pertahanan terdalam NERV. Meski evolusinya berhasil dihentikan melalui protokol penghancuran diri mandiri di sirkuit terakhir Caspar, kerusakan yang ditinggalkan bersifat **katastropik**.

Seluruh protokol komunikasi standar — setiap baris kode *socket* dan *library* jaringan yang pernah diciptakan manusia — telah terinfeksi oleh kode asing Malaikat. Menggunakan modul jaringan lama sama saja dengan menyerahkan kunci Geofront kepada Malaikat berikutnya. Unit Evangelion tidak dapat diluncurkan karena sinkronisasi data antar-inti MAGI terputus total.

### Misi: Operasi Re-Arsitektur MAGI

Kalian adalah kru teknis elit yang tersisa di Divisi Jaringan NERV. Komandan Ikari telah memberikan mandat mutlak: **Bangun kembali seluruh sistem komunikasi MAGI dari titik nol.**

Karena seluruh *library* standar telah dikarantina karena risiko kontaminasi *Pattern Blue*, kalian dilarang keras menggunakan abstraksi jaringan tingkat tinggi yang sudah ada. Kalian harus merajut kembali setiap helai kabel virtual, membangun setiap *header* paket (Ethernet, IP, TCP), dan memastikan integritas data dari Layer 2 hingga Layer 7 secara manual.

Kalian harus memastikan:

1. **Integritas Layer Fisik & Data Link** — Memastikan kabel virtual memiliki latensi yang akurat agar sinkronisasi MAGI tidak meleset.
2. **Logika Routing L3** — Memastikan paket data dapat menemukan jalan melalui labirin Geofront menuju Central Dogma.
3. **State Machine L4** — Memastikan koneksi TCP yang dibangun murni dari nalar manusia, bebas dari instruksi parasit Malaikat.
4. **Abstraksi L7** — Memulihkan layanan kritis (DHCP, DNS, HTTP) agar operasional NERV kembali normal.

> *"God's in his heaven, all's right with the world.*  
> *Tapi di NERV, hanya kode kalian yang bisa menyelamatkan kita."*

---

## Tujuan

Dengan menyelesaikan tugas ini, peserta diharapkan:

1. Memahami mekanisme dasar pengiriman data melalui jaringan.
2. Memahami cara membongkar dan merakit struktur data paket (header Ethernet, IP, dan TCP) secara presisi pada level byte.
3. Memahami cara kerja algoritma Routing untuk menentukan jalur terbaik bagi paket data.
4. Memahami peran dan tanggung jawab Transport Layer Protocol (TCP dan UDP).
5. Memahami konsep State Machine pada TCP (3-Way Handshake dan Teardown) untuk membangun koneksi yang andal.
6. Memahami cara kerja protokol dan aplikasi Client-Server sederhana di Application Layer (DHCP, DNS, HTTP).

---

## Aturan dan Batasan

| No | Aturan |
|----|--------|
| 1 | Bebas menggunakan bahasa pemrograman apapun (Python, Java, C++, Go, Node.js, dll). Ada nilai bonus berdasarkan tingkat kesulitan bahasa (lihat [Language Multiplier](#penghargaan-bahasa-pemrograman-language-multiplier)). |
| 2 | Diperbolehkan menggunakan LLM (ChatGPT, GitHub Copilot) sebagai *learning assistant*. **Namun**, kalian **wajib memahami** seluruh baris kode. Gagal menjelaskan kode saat demo = plagiarisme. |
| 3 | **DILARANG** menggunakan API jaringan bawaan OS atau modul bawaan bahasa (`import socket`, `http.server` di Python; `java.net.*` di Java; `net/http` di Go). |
| 4 | Seluruh protokol stack (ARP, IPv4, TCP, UDP) dan Application (DHCP, DNS, HTTP, RIP) **wajib dibangun dari nol** (*from scratch*) di dalam *user-space*. |
| 5 | **DILARANG** menggunakan library/framework simulasi jaringan pihak ketiga (Mininet, NS-3, Scapy) untuk logika inti pembuatan paket. Semua struktur paket dan abstraksi entitas jaringan harus didefinisikan sendiri. |
| 6 | Library eksternal **hanya diizinkan** untuk utilitas dasar non-jaringan (parsing JSON, pembacaan file, pengaturan waktu/sleep untuk delay kabel, fungsi random), serta untuk keperluan fitur bonus (rendering graf, GUI). |
| 7 | **WAJIB** menyediakan `Makefile` atau skrip `run.sh` di root repository. Asisten hanya menjalankan `make run` atau `./run.sh`. Program tidak boleh crash karena kesalahan ketik perintah user (terapkan error handling sederhana). |
| 8 | Secara standar, simulator wajib dibangun menggunakan arsitektur **sekuensial murni** (*single-threaded*). Aturan ini gugur bagi kelompok yang mengerjakan fitur bonus **Asynchronous & Concurrent Engine**. |

---

## Format Input & Antarmuka CLI

Program wajib menyediakan interaksi **Command Line (CLI) dinamis**. Setelah dijalankan, program tidak boleh *exit*, melainkan masuk ke mode **interactive prompt** (contoh: `Magi> `) yang menunggu instruksi pengguna.

### Standar Format Topologi (JSON)

Seluruh kelompok **wajib** mematuhi skema konfigurasi JSON berikut:

```json
{
  "hosts": [
    {
      "name": "H1",
      "ip_address": "192.168.1.10/24",
      "default_gateway": "192.168.1.1"
    },
    {
      "name": "H2",
      "ip_address": "10.0.0.5/8",
      "default_gateway": "10.0.0.1"
    }
  ],
  "switches": [
    {
      "name": "SW1",
      "num_ports": 24,
      "vlans": [
        { "port": 1, "mode": "access", "vlan_id": 10 },
        { "port": 24, "mode": "trunk" }
      ]
    }
  ],
  "routers": [
    {
      "name": "R1",
      "interfaces": [
        { "port": 1, "ip_address": "192.168.1.1/24" },
        { "port": 2, "ip_address": "10.0.0.1/8" }
      ],
      "routing_table": [
        { "destination": "0.0.0.0/0", "next_hop": "10.0.0.254", "interface": 2 }
      ]
    }
  ],
  "links": [
    { "endpoints": ["H1", "SW1:1"], "delay": 10 },
    { "endpoints": ["H2", "R1:2"], "delay": 25 },
    { "endpoints": ["SW1:24", "R1:1"], "delay": 5 }
  ]
}
```

#### Ketentuan JSON

**1. Notasi IP & CIDR**  
Parameter `ip_address` dan `destination` pada `routing_table` wajib menggunakan notasi CIDR, contoh: `192.168.1.10/24`.

**2. Notasi Link (`Node:Port`)**  
Format string `NamaNode:NomorPort`. Khusus untuk Host, nomor port boleh dihilangkan (cukup tulis `"H1"`).

**3. Penyuntikan MAC Address**  
Tidak ada properti `mac_address` di JSON — **disengaja**. Program wajib men-*generate* MAC Address secara otomatis untuk setiap antarmuka saat pertama kali dimuat ke memori.

**4. Karakteristik Kabel (Latensi)**  
Atribut `delay` (dalam milidetik) mengimplementasikan simulasi *Propagation Delay*.

---

### Perintah CLI

#### Pengujian Jaringan & Aplikasi L7

| Perintah | Contoh | Deskripsi |
|----------|--------|-----------|
| `<host> ping <ip>` | `H1 ping 192.168.1.11` | Mengirim ICMP Echo Request |
| `<host> traceroute <ip>` | `H1 traceroute 10.0.0.11` | Melacak rute hop-by-hop |
| `<host> tcp_connect <ip> <port>` | `H1 tcp_connect 192.168.1.11 80` | Melakukan TCP Handshake |
| `<host> http_get <url>` | `H1 http_get www.internet.com` | Meminta halaman web statis |
| `<host> http_server start [file]` | `H1 http_server start index.html` | Menjalankan web server |
| `<host> http_server stop` | `H1 http_server stop` | Mematikan web server |
| `<host> dhcp_discover` | `H3 dhcp_discover` | Meminta alokasi IP otomatis |

#### Inspeksi Entitas

| Perintah | Deskripsi |
|----------|-----------|
| `<router> route` | Menampilkan Routing Table internal router |
| `<switch> mac` | Menampilkan MAC Address Table internal switch |
| `<host\|router> arp` | Menampilkan ARP Cache |

#### Manajemen Topologi Dinamis

| Perintah | Deskripsi |
|----------|-----------|
| `create <host\|router\|switch> <name>` | Membuat instance entitas device baru |
| `link <device1> <device2>` | Menyambungkan kabel antara dua antarmuka |
| `unlink <device1> <device2>` | Memutuskan sambungan kabel |
| `topology` | Mencetak deskripsi layout topologi jaringan aktif |
| `save [filename]` | Menyimpan topologi saat ini ke file `.json` |
| `load [filename]` | Memuat file `.json` ke dalam simulasi |
| `exit` / `quit` | Menutup simulator |

### Output Logging & Tracing

Simulator wajib mencetak log perjalanan paket secara jelas ke terminal. Contoh:

```
Magi> H1 ping 192.168.1.1
[H1] Memulai ARP Request untuk 192.168.1.1...
[SW1] Flooding frame dari Port 1 ke Port 2, 3, 4.
[R1] Menerima ARP Request, membalas dengan ARP Reply (MAC: AA:BB:CC:DD:EE:FF)
[H1] ARP Reply diterima. Mengirim IP Packet (ICMP Echo Request) ke 192.168.1.1.
[R1] ICMP Echo Reply dikirim kembali ke H1.
H1: Reply from 192.168.1.1: bytes=32 time=5ms TTL=64
```

---

## Struktur Direktori Proyek

```
magi_system/
├── Makefile              # Entry point wajib (make run)
├── README.md             # Dokumentasi proyek
├── topology.json         # File konfigurasi input
├── cli.py                # Antarmuka CLI interaktif
├── main.py               # File utama untuk inisialisasi awal
│
├── core/                 # ---- MILESTONE 0 ----
│   ├── link.py           # Menangani perpindahan byte antar interface
│   ├── interface.py      # Titik masuk/keluar dari node
│   └── packet.py         # Base class untuk serialize/deserialize
│
├── layer2/               # ---- MILESTONE 1 ----
│   ├── ethernet.py       # Struktur Ethernet Frame
│   ├── arp.py            # Logika ARP & Cache
│   ├── switch.py         # Entitas Switch & MAC Table
│   └── host.py           # Entitas Host dasar
│
├── layer3/               # ---- MILESTONE 2 ----
│   ├── ipv4.py           # Struktur IP Packet & Checksum
│   ├── icmp.py           # Struktur ICMP Message
│   └── router.py         # Entitas Router & Routing Table
│
├── layer4/               # ---- MILESTONE 3 ----
│   ├── tcp.py            # Struktur TCP Segment & State Machine
│   ├── tcp_socket.py     # Logika Receive Buffer & Handshake
│   └── udp.py            # Struktur UDP Datagram
│
├── layer7/               # ---- MILESTONE 4 & 5 ----
│   ├── magi_socket.py    # The Wrapper API (Abstraksi OS Socket)
│   ├── dhcp_server.py    # Logika DORA
│   ├── dns_server.py     # Resolusi nama domain
│   ├── http_server.py    # Server HTTP sederhana
│   └── rip.py            # Logika RIPv2 (Bonus)
│
├── middleboxes/          # ---- MILESTONE 5 (Bonus) ----
│   ├── nat.py            # Logika Network Address Translation
│   └── acl.py            # Access Control List filter firewall
│
├── gui/                  # ---- MILESTONE 5 (Bonus) ----
│   └── app.py            # Logika GUI untuk dashboard interaktif
│
└── utils/                # ---- MILESTONE 5 (Bonus) ----
    └── visualizer.py     # Topology graph renderer
```

> Struktur di atas adalah contoh dalam Python dan dapat diadaptasi ke bahasa apapun.

---

## Milestone 0: Fondasi Simulasi (Packet & Link Core)

Sebelum menyentuh logika layer jaringan manapun, bangun terlebih dahulu fondasi data dan topologi fisik.

### Komponen yang Wajib Dibangun

#### 1. Interface & Link

Karena simulator berjalan secara sekuensial (sinkron), koneksi kabel hanyalah berupa *binding* referensi antar-objek. Saat sebuah entitas mengirim data melewati Link, ia langsung memanggil metode penerimaan data (`receive()`) pada antarmuka di ujung kabel satunya secara instan.

Transmisi data **menunda/membekukan eksekusi sementara** (*blocking sleep*) sebesar properti `delay` pada kabel untuk menyimulasikan *Propagation Delay*.

#### 2. Serialization & Deserialization

Paket harus bisa dikonversi:
- **Objek → byte mentah** (saat dikirim melewati Link)
- **Byte mentah → Objek** (saat diterima oleh entitas lain)

#### 3. Header & Payload

Setiap paket di layer manapun memiliki *header* dan *payload*. Wajib mendefinisikan kelas induk abstrak yang diturunkan (*inherited*) oleh protokol Ethernet, IPv4, TCP, UDP, dsb.

### Referensi Desain (Python)

```python
class Packet:
    def to_bytes(self) -> bytes:
        """Serialize objek menjadi byte mentah (header + payload)"""
        raise NotImplementedError

    @classmethod
    def from_bytes(cls, raw_bytes: bytes):
        """Deserialize byte mentah menjadi instance objek paket"""
        raise NotImplementedError

class Node:
    def __init__(self, name):
        self.name = name
        self.interfaces = {}  # port_number -> Interface object

class Interface:
    def __init__(self, node, port_number, mac_address):
        self.node = node
        self.port_number = port_number
        self.mac_address = mac_address
        self.link = None

    def send(self, raw_bytes: bytes):
        if self.link:
            self.link.transmit(self, raw_bytes)

    def receive(self, raw_bytes: bytes):
        self.node.handle_receive(self, raw_bytes)

class Link:
    def __init__(self, interface_a, interface_b, delay_ms=0):
        self.interface_a = interface_a
        self.interface_b = interface_b
        self.delay_ms = delay_ms
        interface_a.link = self
        interface_b.link = self

    def transmit(self, sender_interface, raw_bytes: bytes):
        """Melempar byte ke ujung antarmuka kabel satunya"""
        if sender_interface == self.interface_a:
            receiver = self.interface_b
        else:
            receiver = self.interface_a

        # Simulasi Propagation Delay (blocking)
        if self.delay_ms > 0:
            time.sleep(self.delay_ms / 1000.0)

        receiver.receive(raw_bytes)
```

### Indikator Keberhasilan

- [ ] Perintah `create`, `link`, dan `unlink` berjalan dengan benar dan mengaitkan/memutus objek Node di memori.
- [ ] Perintah `save` mengekspor konfigurasi ke JSON, dan `load` membaca kembali konfigurasi tersebut secara utuh.
- [ ] Terdapat definisi entitas `Packet` dasar yang mendukung konversi ke byte-array.

---

## Milestone 1: Data Link Layer (L2)

Fokus: komunikasi antara Host di subnet yang sama menggunakan Switch dan MAC Address.

### Komponen yang Wajib Dibangun

#### 1. Switching & VLAN

Implementasi **Tabel MAC yang VLAN-aware**:

- **Port Access**: Menerima lalu lintas tak ber-tag, lalu memberinya VLAN ID ke dalam.
- **Port Trunk**: Meneruskan lalu lintas yang sudah ber-tag.
- **Aturan utama**: Forwarding atau Flooding **hanya terjadi antar-port dengan VLAN ID yang sama**.

#### 2. ARP & Queuing

Jika Host ingin mengirim IP Packet ke IP tertentu namun belum tahu MAC tujuannya:
1. Host **mengantrekan** (*queue*) IP Packet tersebut sementara mengirim ARP Request.
2. Segera setelah ARP Reply tiba, IP Packet dalam antrean di-*encapsulate* menjadi Ethernet Frame lalu dikirim.

```
[ Host A (10.0.0.1) ]  [ Switch ]  [ Host B (10.0.0.2) ]
         |                   |                |
1. Cek ARP Cache (Miss!)     |                |
2. Simpan IP Packet ke Queue |                |
3. Send ARP Request (Bcast) --> (Floods) ------->
                             |                | 4. B simpan MAC A
                             | <-- 5. Send ARP Reply (Unicast)
6. A simpan MAC B            |                |
7. Lepas Queue IP Packet     |                |
8. Send Ethernet Frame ----> | (Lookup Table) -> 9. B terima IP Packet
```

### Referensi Desain (Java)

```java
public abstract class Packet {
    public abstract byte[] toBytes();
}

public class EthernetFrame extends Packet {
    public String dstMac;
    public String srcMac;
    public short etherType; // 0x0800 (IPv4), 0x0806 (ARP)
    public Integer vlanId;
    public byte[] payload;

    @Override
    public byte[] toBytes() { /* Logika pack ke byte array */ return new byte[0]; }
}

public class ARPMessage extends Packet {
    public short opcode; // 1: Request, 2: Reply
    public String senderMac;
    public String senderIp;
    public String targetMac;
    public String targetIp;

    @Override
    public byte[] toBytes() { return new byte[0]; }
}

public class ARPCache {
    public Map<String, String> table = new HashMap<>();  // ip -> mac
    public Map<String, List<byte[]>> queue = new HashMap<>();  // ip -> pending bytes
}

public class Switch extends Node {
    public Map<String, Integer> macTable = new HashMap<>();  // "vlan_id:mac" -> port
    public Map<Integer, VlanConfig> vlanConfig = new HashMap<>();

    public void handleReceive(Interface inface, byte[] rawBytes) {
        // 1. Parse EthernetFrame
        // 2. Update MAC Table
        // 3. Forward atau Flood
    }
}

public class Host extends Node {
    public String ipAddress;
    public String gateway;
    public ARPCache arpCache = new ARPCache();

    public void sendLayer3Packet(String targetIp, byte[] l3Bytes) {
        // Cek ARP Cache -> hit: kirim. Miss: Queue & Broadcast ARP.
    }
}
```

### Indikator Keberhasilan

- [ ] Host dapat saling bertukar Ethernet Frame tanpa error dalam satu subnet/VLAN yang sama.
- [ ] Tabel MAC pada Switch dan ARP Cache pada Host terisi otomatis; perintah `mac` dan `arp` menampilkan entri yang akurat.
- [ ] Paket IP tidak hilang (*drop*) saat MAC tujuan belum diketahui, melainkan berhasil tertunda (*queued*) dan otomatis dilanjutkan setelah ARP Reply diterima.

---

## Milestone 2: Network Layer (L3)

Fokus: mekanisme Router untuk menghubungkan dua jaringan/subnet yang berbeda.

### Komponen yang Wajib Dibangun

#### 1. Routing & Longest Prefix Match

Router membaca Destination IP dan mengevaluasi Routing Table menggunakan algoritma pencarian **Longest Prefix Match**.

#### 2. Inter-VLAN Routing

Router menerima lalu lintas ber-tag (*Trunk*) dari Switch. Sub-interface di Router:
1. Melucuti tag VLAN yang masuk.
2. Merutekan paket di Layer 3.
3. Memasang tag VLAN yang baru.
4. Melemparkan kembali ke Switch.

#### 3. TTL & IPv4 Checksum Validation

- Hitung dan validasi **Header Checksum IPv4** (*16-bit one's complement sum*) pada setiap pengiriman dan penerimaan IP Packet.
- Kurangi **TTL** sebesar 1 pada tiap Router yang dilewati. Saat TTL berubah, Router **wajib menghitung ulang** Checksum-nya.

Format IP Header:

```
Byte  0       1       2       3
      +-------+-------+-------+-------+
  0   | Ver/IHL  | TOS |    Total Len  |
      +-------+-------+-------+-------+
  4   |   Identification  |Flags|FrgOff|
      +-------+-------+-------+-------+
  8   |  TTL  | Proto |  Header Chksum |
      +-------+-------+-------+-------+
 12   |          Source Address         |
      +-------+-------+-------+-------+
 16   |       Destination Address       |
      +-------+-------+-------+-------+
```

#### 4. ICMP Error Messages

| Kondisi | Aksi |
|---------|------|
| TTL habis (= 0) | Kirim **ICMP Time Exceeded** ke source IP asli |
| Rute tidak ada | Kirim **ICMP Destination Unreachable** ke source IP asli |

### Alur Routing Engine

```
(Eth Frame masuk)
        |
  [ Decapsulate Eth ]
        |
  [ Router Routing Engine ]
    1. Validasi IPv4 Checksum
    2. Kurangi TTL - 1
    3. Cek Dest IP di Routing Table (Longest Prefix)
        |
   +----+----+
   |         |
(Ketemu)  (Gagal)
   |         |
4. Hitung    4. Drop Paket
   ulang        Kirim ICMP Unreachable
   Checksum     ke Source IP asli
5. Enkapsulasi
   ulang ke
   interface baru
        |
  (Eth Frame keluar)
```

### Referensi Desain (Go)

```go
type IPPacket struct {
    VersionIHL uint8
    TOS        uint8
    TotalLen   uint16
    TTL        uint8
    Protocol   uint8  // 1: ICMP, 6: TCP, 17: UDP
    Checksum   uint16
    SourceIP   string
    DestIP     string
    Payload    []byte
}

type ICMPMessage struct {
    Type     uint8  // 8: Echo Req, 0: Echo Reply, 3: Unreachable, 11: Time Exceeded
    Code     uint8
    Checksum uint16
    Payload  []byte
}

type RoutingTableEntry struct {
    DestNetwork  string  // Format CIDR, contoh: "192.168.1.0/24"
    PrefixLen    int
    NextHopIP    string
    OutInterface int
}

type Router struct {
    Node
    RoutingTable []RoutingTableEntry
    ARPCache     map[string]string
}

func (r *Router) RoutePacket(inInterface *Interface, packet IPPacket) {
    // 1. Decrement TTL & Recalculate Checksum
    // 2. Longest Prefix Match mencari rute di RoutingTable
    // 3. Resolusi MAC next_hop via ARP Cache
    // 4. Enkapsulasi ke Frame & Kirim
}
```

### Indikator Keberhasilan

- [ ] Perintah `ping` sukses menerima balasan dari jaringan berbeda, dengan kalkulasi `time=...ms` (Round Trip Time) yang realistis berdasarkan akumulasi delay kabel.
- [ ] Perintah `traceroute` berhasil mencetak rute IP hop-by-hop.
- [ ] Mengirim pesan ke IP fiktif menghasilkan pesan **ICMP Destination Unreachable**.
- [ ] Sistem validasi Checksum IPv4 berjalan lancar tanpa drop keliru.

---

## Milestone 3: Transport Layer (L4)

Fokus: abstraksi aliran data mentah menjadi koneksi *port-to-port* terstruktur antar-aplikasi.

> **Penting:** TCP dan UDP wajib menghitung Checksum dengan mengikutsertakan **Pseudo-Header** (yang berisi Source IP dan Destination IP).

### Komponen yang Wajib Dibangun

#### 1. UDP (User Datagram Protocol)

Pengiriman data sederhana, *stateless*, tanpa jaminan penerimaan.

#### 2. TCP State Machine (Transmission Control Protocol)

##### a. Strict 3-Way Handshake

Pengiriman data (PSH) **tidak boleh** terjadi jika Host masih berstatus `SYN_SENT`. Host harus mencapai state `ESTABLISHED` melalui sekuens:

```
SYN → SYN-ACK → ACK
```

##### b. Receive Buffers

Karena paket dalam simulasi dapat tiba dalam urutan tak menentu, TCP bertugas merakit kembali segmen menjadi data utuh di dalam sebuah buffer yang ada di memori Socket.

##### c. 4-Way Teardown

Pemutusan koneksi yang elegan via pengiriman flag FIN dan ACK secara berbalas:

```
A                              B
|                              |
| --------  FIN  -----------> |
| <------- ACK  ------------- | (B: CLOSE_WAIT)
| <------- FIN  ------------- | (B: LAST_ACK)
| --------  ACK  -----------> |
|                              |
(A: TIME_WAIT → CLOSED)   (B: CLOSED)
```

##### Diagram Lengkap TCP State Machine

```
A (Client)                     B (Server)
CLOSED                         LISTEN
   | -------- SYN -----------> |
SYN_SENT                    SYN_RECD
   | <------- SYN+ACK -------- |
   | -------- ACK -----------> |
ESTABLISHED                 ESTABLISHED
   |                           |
   | == Data Transfer (PSH) == |
   |                           |
   | -------- FIN -----------> |
FIN_WAIT_1                  CLOSE_WAIT
   | <------- ACK ------------ |
FIN_WAIT_2
   | <------- FIN ------------ |
                             LAST_ACK
   | -------- ACK -----------> |
TIME_WAIT                   CLOSED
CLOSED
```

### Referensi Desain (C++)

```cpp
struct TCPSegment {
    uint16_t sourcePort;
    uint16_t destinationPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint8_t  flags;       // Bitmask: SYN, ACK, FIN, PSH, RST
    uint16_t windowSize;
    uint16_t checksum;
    std::vector<uint8_t> payload;
};

struct UDPSegment {
    uint16_t sourcePort;
    uint16_t destinationPort;
    uint16_t length;
    uint16_t checksum;
    std::vector<uint8_t> payload;
};

class TCPSocket {
public:
    std::string state = "CLOSED";
    // LISTEN, SYN_SENT, SYN_RCVD, ESTABLISHED,
    // FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT, LAST_ACK, TIME_WAIT
    std::string  localIP;
    uint16_t     localPort;
    std::string  remoteIP;
    uint16_t     remotePort;
    uint32_t     seqNum = 0;
    uint32_t     expectedAck = 0;
    std::vector<uint8_t> receiveBuffer;

    void handleIncomingSegment(const TCPSegment& segment) {
        // Implementasi logika State Machine
        // Mengubah status socket berdasarkan kombinasi
        // segment.flags dan state saat ini
    }
};
```

### Indikator Keberhasilan

- [ ] Perintah `tcp_connect` sukses mengeksekusi 3-Way Handshake yang terekam pada log.
- [ ] Log menunjukkan perubahan status (*state machine*) TCP yang presisi.
- [ ] Receive Buffer TCP berhasil menyatukan paket data (flag PSH) yang terpotong.

---

## Milestone 4: Socket API & Application Layer (L7)

Fokus: membangun antarmuka pemrograman dan protokol aplikasi populer di atas arsitektur TCP/UDP.

### Komponen yang Wajib Dibangun

#### 1. Abstraksi OS Sockets (Wajib)

Bangun sebuah **Wrapper API** untuk soket jaringan buatan Anda. Seluruh protokol aplikasi di bawah ini **wajib** diimplementasikan menggunakan socket API ini, seolah-olah berinteraksi dengan OS sungguhan.

```python
# Contoh abstraksi logika server yang berjalan di dalam Magi System
from magi_system.stack import magi_socket

s = magi_socket.socket(magi_socket.AF_INET, magi_socket.SOCK_STREAM)
s.bind(('192.168.1.10', 80))  # Port HTTP
s.listen(5)
conn, addr = s.accept()
data = conn.recv(1024)
# Melanjutkan parsing HTTP Text...
```

#### 2. DHCP (Dynamic Host Configuration Protocol)

- Berjalan di atas soket **UDP** (port 67/68).
- Host baru mengirim pesan secara **Broadcast** (MAC dan IP) memohon alokasi alamat IP dinamis.
- Server membalas dengan alur **DORA**:

```
Client                          Server
  |                               |
  | -- DISCOVER (Broadcast) ----> |
  | <-- OFFER (Unicast) --------- |
  | -- REQUEST (Broadcast) -----> |
  | <-- ACKNOWLEDGE (Unicast) --- |
  |                               |
(Client memiliki IP address)
```

#### 3. DNS (Domain Name System)

- Host mengirimkan kueri DNS via soket **UDP** (port 53).
- Resolusi nama domain: `"www.magi.com"` → `"10.0.0.5"`.

#### 4. HTTP (Hypertext Transfer Protocol)

- Host klien bertindak sebagai *browser* yang berinteraksi dengan Web Server melalui soket **TCP port 80**.
- Client mengirim: `GET / HTTP/1.1`
- Server mengurai dari koneksi socket, memberikan respons `HTTP/1.1 200 OK` (beserta data HTML rekaan), lalu melakukan **Teardown mandiri** (`conn.close()`).

```
Client                          Server
  |                               |
  | -- TCP Connection ----------> |
  | -- GET / HTTP/1.1 ----------> |
  | <-- HTTP/1.1 200 OK --------- |
  | <-- [HTML Content] ---------- |
  | -- TCP Close (FIN) ---------> |
  | <-- TCP Close (FIN/ACK) ----- |
```

### Referensi Desain (Python)

```python
class MagiSocket:
    AF_INET     = 2
    SOCK_STREAM = 1  # TCP
    SOCK_DGRAM  = 2  # UDP

    def __init__(self, host_node, family, sock_type):
        self.node = host_node
        self.transport = TCPSocket() if sock_type == self.SOCK_STREAM else UDPSocket()

    def bind(self, ip, port): ...
    def listen(self, backlog): ...
    def accept(self): ...
    def connect(self, ip, port): ...
    def send(self, data_bytes: bytes): ...
    def recv(self, buffer_size) -> bytes: ...
    def close(self): ...

class HTTPServer:
    def __init__(self, host_node, web_root):
        self.server_socket = MagiSocket(host_node, MagiSocket.AF_INET, MagiSocket.SOCK_STREAM)
        self.server_socket.bind(host_node.ip_address, 80)
        self.web_root = web_root

    def start(self):
        self.server_socket.listen(5)
        # Menunggu incoming TCP connection...
```

### Indikator Keberhasilan

- [ ] Adanya kelas `MagiSocket` yang membungkus kerumitan handshake TCP menjadi satu fungsi `connect()`.
- [ ] Perintah `http_server start` mengikat soket untuk *listening* di port 80.
- [ ] Perintah `dhcp_discover`, `http_get`, dan resolusi nama DNS dieksekusi dengan sukses.

---

## Milestone 5: Fitur Lanjutan & Utilitas (Bonus)

Pilih satu atau beberapa fitur lanjutan berikut untuk nilai tambahan:

| Fitur | Nilai | Deskripsi |
|-------|-------|-----------|
| **Topology Visualizer** | +2 Poin | Cetak topologi menjadi representasi gambar graf interaktif. Dipanggil lewat perintah `visualize`. |
| **IP Fragmentation & Reassembly** | +3 Poin | Atur properti MTU pada kabel. Paket IP melebihi MTU dipotong (*fragmentation*) beserta Fragment Offset, lalu dirakit kembali di Host penerima. |
| **Access Control List (ACL) Firewall** | +3 Poin | Router memblokir (*Deny*) atau mengizinkan (*Permit*) paket berdasarkan kriteria sumber/tujuan IP, protokol, atau port. |
| **Network Address Translation (NAT/PAT)** | +4 Poin | Router melakukan translasi dari IP privat ke IP publik secara transparan, memungkinkan jaringan internal berkomunikasi dengan entitas di jaringan luar. |
| **Dynamic Routing RIPv2** | +5 Poin | Pertukaran Routing Table otomatis antar Router via UDP Broadcast. Mekanisme pemicu dibebaskan (manual CLI, otomatis setiap `link`/`unlink`, atau setiap N interaksi). |
| **GUI / Web Dashboard Interaktif** | +8 Poin | Program mengekspos API lokal yang ditarik oleh antarmuka Web atau Desktop GUI. Pengguna bisa melihat animasi topologi dan paket bergerak secara grafis (seperti Cisco Packet Tracer). |
| **Asynchronous & Concurrent Engine** | +15 Poin | Setiap entitas Node berjalan di dalam thread/goroutine/async task terpisah. Komunikasi Link menggunakan Thread-safe Message Queue. RIPv2 wajib menggunakan timer riil dan TCP mendukung retransmission berbasis waktu. |

### Indikator Keberhasilan Bonus

| Fitur | Indikator |
|-------|-----------|
| Topology Visualizer | Perintah `visualize` mengekspor representasi visual (`.png`, `.svg`, atau pop-up window) yang akurat memetakan koneksi antar node. |
| IP Fragmentation | Paket melebihi MTU terlihat terpecah di log terminal, namun tetap dirakit (*reassembled*) dan dibalas secara utuh oleh node tujuan. |
| ACL | Koneksi TCP/UDP yang melewati port/rute ber-*Deny* tidak mendapat respons (*Silent Drop / Connection Timeout*). |
| NAT/PAT | Koneksi dari IP Private ke IP Public berjalan mulus; Router terbukti melakukan translasi port untuk membalas ke klien yang tepat. |
| GUI/Web Dashboard | Antarmuka grafis menampilkan status terkini secara *real-time* (animasi paket bergerak atau log real-time), bukan gambar statis. |
| RIPv2 | Routing table setiap Router otomatis memperbarui diri dengan jalur terpendek baru saat ada kabel diputus (`unlink`). |
| Concurrent Engine | CLI selalu responsif (*non-blocking*). RIPv2 melakukan broadcast tabel routing otomatis berdasarkan interval waktu. Log terminal menampilkan aktivitas latar belakang secara asinkron tanpa mengganggu input. |

---

## Skema Penilaian (Grading)

> Penilaian dilakukan sepenuhnya melalui **demonstrasi langsung** (*live demo*) dan **inspeksi kode** (*code review*).

| Komponen | Bobot |
|----------|-------|
| Milestone 1 — L2: Switching & ARP | **20%** |
| Milestone 2 — L3: Routing & ICMP | **25%** |
| Milestone 3 — L4: TCP & UDP | **30%** |
| Milestone 4 — L7: Socket API & Application Protocols | **25%** |
| Milestone 5 — Bonus | **+X Poin** |
| Bonus Kontinuitas (per Milestone tepat waktu) | **+2 Poin** |
| Penghargaan Bahasa Pemrograman | **+Y Poin** (diskalakan) |

> **Catatan:** Milestone 0 adalah prasyarat (tidak dinilai secara terpisah, namun wajib selesai untuk milestone selanjutnya).

---

## Penghargaan Bahasa Pemrograman (Language Multiplier)

Poin bonus bahasa adalah **Poin Maksimal yang diskalakan proporsional** dengan persentase pencapaian Nilai Dasar (Milestone 1-4).

> **Contoh:** Kelompok menyelesaikan hingga Milestone 2 (nilai dasar 45%), menggunakan bahasa C murni → Bonus bahasa = 45% × 25 = **+11.25 Poin**

| Tingkat | Bahasa | Bonus Maks | Keterangan |
|---------|--------|------------|------------|
| ★ | Python, JavaScript (Node.js) | +0 Poin | Manipulasi string/byte mudah, manajemen memori otomatis (GC). Sangat cocok untuk rapid prototyping. |
| ★★ | Java, C#, Kotlin | +5 Poin | Strongly-typed, masih ada Garbage Collector. Manipulasi bit-shifting sedikit lebih kaku. |
| ★★★ | Go, Rust, C++ | +10 Poin | Manajemen memori semi/manual, tertolong OOP bawaan dan library standar. |
| ★★★★ | C | +25 Poin | Tidak ada OOP bawaan (harus memalsukan pewarisan dengan pointer fungsi). Tidak ada library struktur data (hash map dibangun sendiri). |
| ★★★★★ | Assembly (x86/ARM) | +50 Poin | *Godspeed.* |

---

## Protokol Sinkronisasi Berkala (Bonus Kontinuitas)

Bonus diberikan untuk setiap Milestone yang diselesaikan dan di-push ke GitHub sebelum deadline mingguan.

### Sistem Tagging GitHub

| Milestone | Tag Format | Deadline |
|-----------|------------|----------|
| Milestone 0 | `v0.0.x` | Minggu, 26 April 2026, 22.30 WIB |
| Milestone 1 | `v0.1.x` | Minggu, 3 Mei 2026, 22.30 WIB |
| Milestone 2 | `v0.2.x` | Minggu, 10 Mei 2026, 22.30 WIB |
| Milestone 3 | `v0.3.x` | Minggu, 17 Mei 2026, 22.30 WIB |
| Milestone 4 | `v1.0.x` | Minggu, 24 Mei 2026, 22.30 WIB |

> Nilai `x` adalah nomor revisi/patch. Jika menemukan bug dan memperbaikinya sebelum deadline minggu tersebut, cukup buat rilis baru dengan menaikkan nilai `x` (contoh: `v0.1.0` → `v0.1.1`). Asisten akan selalu menilai tag revisi terbesar pada minggu tersebut.

- **+2 Poin** untuk setiap Milestone yang berhasil diselesaikan tepat waktu.
- **+10 Poin total** jika disiplin melakukan rilis tepat waktu selama 5 minggu.

---

## Pengerjaan & Deliverables

### Ketentuan Kelompok

- Tugas dikerjakan berkelompok dengan **4-5 orang**. Kelompok tidak boleh lintas kelas.
- Isi kelompok pada tautan sheets yang disediakan. **Tenggat pengisian: Rabu, 22 April 2026, 22.30 WIB.**
- Setelah waktu tersebut, sheets dikunci dan peserta yang belum punya kelompok akan di-assign secara acak.

### Repository

- Wajib menggunakan **version control system git** dengan repository private di **GitHub Classroom**.
- Pengumpulan dan rilis kode **diwajibkan** menggunakan fitur **Release dan Tag** pada GitHub.
- Penamaan tag mengikuti format `v[Major].[Minor].[Patch]` sesuai pencapaian Milestone.

### Isi Minimal Repository

```
repository/
├── Makefile atau run.sh        # Entry point: make run atau ./run.sh
├── README.md                   # Deskripsi, prerequisites, cara setup/run, pembagian tugas
├── topology.json               # Contoh file konfigurasi
└── [source code terstruktur]   # Berdasarkan pembagian layer jaringan
```

> Tidak ada keperluan untuk membuat laporan.

### Batas Akhir

> **Deadline Final: Minggu, 24 Mei 2026, 22.30 WIB**

---

## Daftar Revisi

| Tanggal | Perubahan |
|---------|-----------|
| 21 April 2026 | Penambahan rincian pengecualian penggunaan library/modul eksternal khusus untuk utilitas dasar non-jaringan (parsing JSON, timer, probabilitas) pada Aturan dan Batasan. |

---

*"God's in his heaven, all's right with the world.*  
*Tapi di NERV, hanya kode kalian yang bisa menyelamatkan kita."*
