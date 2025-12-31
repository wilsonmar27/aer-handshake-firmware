# aer-handshake-firmware
Firmware implementation of a general AER (Address-Event Representation) request/ack handshake bus on A Raspberry Pi Pico 2. The initial target is a **32×32** event-based sensor, but the codebase is designed so you can change geometry (e.g., 64×64) and bus parameters without major rewrites.

## 1) Scope and Goals

**Primary goal:** Robustly receive AER traffic over a (Delay Innsensitive) DI handshake link, decode addresses, and output a stream of events (row, col).

**Design goals:**
- Keep protocol logic **platform-agnostic** (unit-testable on a host PC).
- Keep hardware-specific code (GPIO/PIO/UART) **thin** and swappable.
- Parameterize sensor geometry and bus layout with compile-time constants.

---

## 2) Physical Interface

### 2.1 Signals

- `DATA[11:0]` : **12 data bus lines**, input to the Pico (receiver).
- `ACK` : acknowledge line, output from the Pico (receiver) to transmitter.
- `RESET` : reset line (direction depends on system integration; see 2.3).

### 2.2 Electrical conventions (assumptions)

- All signals are digital CMOS-level.
- `DATA` is considered **valid** when it encodes a non-neutral 1-of-4 code in each group (see Section 3).
- `DATA` is considered **neutral** when **all data-bus lines are low**.
- `ACK` is driven actively (push-pull) by the receiver.


### 2.3 RESET behavior

RESET is included to place the link into a known safe state.

- `RESET` is **active-high** (asserted = 1).
- When `RESET` is asserted:
  - Receiver drives `ACK = 0`.
  - Receiver ignores `DATA` until `RESET` deasserts and `DATA` is neutral.
- After deasserting `RESET`, the receiver waits for `DATA == neutral` before accepting the next word.

> If RESET polarity/direction changes per integration, firmware should treat it as configurable.

---

## 3) Data Encoding: 1-of-4 per 2-bit symbol

### 3.1 Logical payload width

Target system uses **6 logical bits** per word:
- **5 information bits** (enough for 0–31 index)
- **+1 pad/redundant bit** required by encoding alignment

These 6 bits are transmitted as **three 2-bit symbols**.

### 3.2 Bus grouping (12 lines)

The 12 `DATA` lines are partitioned into **3 groups**, each group encodes **one 2-bit symbol** using **1-of-4 (one-hot)**:

- Group 0: `DATA[3:0]`
- Group 1: `DATA[7:4]`
- Group 2: `DATA[11:8]`

### 3.3 1-of-4 mapping (per group)

Each 2-bit value is encoded as exactly one asserted line in its 4-wire group:

| 2-bit symbol | One-hot line asserted |
|------------:|------------------------|
| `00`        | `D0` (bit0)            |
| `01`        | `D1` (bit1)            |
| `10`        | `D2` (bit2)            |
| `11`        | `D3` (bit3)            |

A **valid word** must have **exactly one** asserted line in **each** group.

### 3.4 Neutral (spacer) state

The DI link uses a spacer/neutral between words:

- **Neutral = all 12 DATA lines low**.

Neutral is not a data word; it is required for the 4-phase handshake (Section 4).

### 3.5 Invalid encodings

An encoding is **invalid** if any group has:
- no asserted line (while the overall word is non-neutral), or
- more than one asserted line.

Firmware will treat invalid words as protocol errors (Section 7).

---

## 4) Handshake Protocol (4-phase DI)

This is a **receiver-acknowledged** DI word-serial protocol. Each word transfer consists of:

1. **Transmitter drives DATA = valid word** (non-neutral).
2. **Receiver detects valid DATA**, latches it, then asserts `ACK = 1`.
3. **Transmitter observes ACK=1** and returns `DATA` to **neutral** (all zeros).
4. **Receiver observes neutral DATA** and deasserts `ACK = 0`.

Then the next word may begin.

### 4.1 Receiver responsibilities

- Only assert ACK after:
  - DATA is **non-neutral**, and
  - After latching raw value.
- Latch DATA once per word (avoid double-sampling the same word).
- Deassert ACK only after DATA is neutral.

### 4.2 Timing notes

- The protocol is DI; do not assume a fixed clock relationship.
- The receiver should include **timeouts** so it can recover if:
  - DATA never returns to neutral after ACK asserted, or
  - DATA never becomes valid after ACK deasserted.

---

## 5) Word Semantics for the 32×32 Camera Mode

The camera transmits:

1. a **ROW word** (selects a row),
2. followed by **one or more COL words** (all columns that fired in that row),
3. followed by a **TAIL word** (marks end of the row burst / packet).

This is repeated for subsequent active rows.

### 5.1 Row/Column payload format

- For both ROW and COL words, the **5 information bits** represent an index in `[0..31]`.
- The 6th bit is **padding** and = 0.
- Bit ordering within the 6-bit payload must be defined so encoder/decoder agree:

**Recommended bit layout (LSB-first):**
- payload bits: `b0..b4` = index bits (LSB..MSB)
- `b5` = pad (0)

These 6 bits are then split into 3 two-bit symbols for transmission:
- symbol0 = `b1 b0`
- symbol1 = `b3 b2`
- symbol2 = `b5 b4`

---

## 6) Tailword Definition

A tailword is a reserved 6-bit value that **cannot collide** with normal row/col indices (0–31).

**Recommended tailword:**
- payload = `0b11_11_11` (decimal 63)
- encoded = `1000-1000-1000`

Rationale:
- Normal indices use pad bit `b5=0`, so they range 0–31.
- Tailword uses `b5=1` and all bits 1, making it distinct and easy to recognize.

After decoding the 6-bit payload:
- if payload == 63 → word type = `TAIL`

> If you chnage the number of bits sent, chnage tailword accordingly

---

## 7) Error Handling and Recovery

Firmware detects and handles:

### 7.1 Encoding errors
- Invalid 1-of-4 patterns (multiple bits set in a group, or malformed non-neutral).

Policy:
- Increment an error counter.
- Drop the word (do not feed it to the burst parser).
- Attempt to resynchronize by waiting for neutral + ACK low, then continue.

### 7.2 Handshake timeouts
- Timeout waiting for ACK phase completion:
  - DATA valid but never returns to neutral after ACK asserted.
- Timeout waiting for next word:
  - ACK low but DATA never becomes valid.

Policy:
- Force `ACK=0`.
- Wait for neutral.
- Mark link as resynchronized.

### 7.3 Burst/parser errors
- TAIL received without ROW.

---

## 8) Parameterization

The code should be driven by compile-time configuration (e.g., `aer_cfg.h`) for:
- Sensor geometry: `AER_ROWS`, `AER_COLS`
- Index bits: `AER_INDEX_BITS` (5 for 32)
- Payload bits: `AER_PAYLOAD_BITS` (index bits + pad bits; 6 here)
- Grouping: `AER_SYMBOL_BITS = 2`, `AER_GROUPS = AER_PAYLOAD_BITS / AER_SYMBOL_BITS` (3 here)
- Data lines: `AER_DATA_LINES = AER_GROUPS * 4` (12 here)
- Tailword payload value (reserved)

Changing these values should not require rewriting core logic—only adjusting constants and (if needed) tailword reservation.

---

## 9) Expected Output Event Format (internal)

Core “event” representation (portable):
- `row` (0..AER_ROWS-1)
- `col` (0..AER_COLS-1)

For the 32×32 mode:
- Each COL word produces one event using the most recent ROW.

---
