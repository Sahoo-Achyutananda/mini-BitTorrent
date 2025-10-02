# P2P Distributed File Sharing System Implementation Details

This document outlines the architecture, component responsibilities, key algorithms, and detailed protocol specifications used in the Peer-to-Peer (P2P) Distributed File Sharing System.

## How to Run 

```bash
make
// tracker
cd tracker
./tracker_app tracker_info.txt <tracker_no> // tracker_no is 0 or 1

// clients - in another terminal 
cd client
./client_app 127.0.0.1:4050 tracker_info.txt // any port number can be used except for the tracker ports
```
---
## Overview: Basic Implementation Details

The system is a decentralized, multi-threaded P2P file-sharing application built in **C++** using **POSIX Threads (`pthread`)** for concurrency. It employs a **hybrid architecture** where a Multi-Tracker layer manages centralized metadata, and the Client (Peer) layer handles file transfers directly between peers.

Key implementation aspects include:
* **Networking**: Uses standard **TCP** for both Tracker-Client and Peer-Peer communication.
* **Data Structures**: Core state relies on C++ **`unordered_map`** for $O(1)$ average-time complexity lookups.
* **File Integrity**: **SHA1 hashing** is used to verify the integrity of individual file pieces after download.
* **Transfer Optimization**: File pieces are transferred in fixed sizes (e.g., **512KB chunks**).
* **Concurrent Downloading**: Piece downloads are managed by an **I/O-optimized Thread Pool** for maximum speed and efficiency.

---
## Client (Peer)
The Client acts as a hybrid entity that is both a **Downloader** (initiating piece requests) and a **Seeder** (running a separate download server to serve pieces). It maintains local state for **active downloads** and **seeding files**, protected by dedicated mutexes.

### Key Responsibilities:
1.  **User Authentication**: Login/logout functionality with credential management.
2.  **File Upload**: Calculate SHA1 hashes for file pieces and register metadata with the tracker.
3.  **File Download**: Multi-threaded parallel downloading from multiple seeders.
4.  **Tracker Failover**: Automatically switch to backup trackers when the primary fails.
5.  **Seeding**: Serve file pieces to other peers upon request.

### Client Architecture:
The client runs several concurrent components:
* **Main Thread**: Handles user input and tracker communication using **`select()`** for non-blocking I/O.
* **Download Server Thread**: Listens for incoming peer piece requests on `client_port + 2000`.
* **Heartbeat Thread**: Monitors tracker health by periodically checking `tracker_port + 1000`.
* **Download Worker Threads**: A **Thread Pool** executes parallel piece downloads.

### Client Commands (Tracker Protocol)
Clients issue these commands over the main TCP connection to the tracker:

| Command | Description |
| :--- | :--- |
| `create_user <userid> <password>` | Register new user account |
| `login <userid> <password>` | Authenticate and start session |
| `logout` | End session and stop all seeding |
| `create_group <groupid>` | Create new file-sharing group |
| `join_group <groupid>` | Request to join existing group |
| `leave_group <groupid>` | Leave group and stop seeding group files |
| `list_groups` | Display all available groups |
| `list_requests <groupid>` | Show pending join requests (owner only) |
| `accept_request <groupid> <userid>` | Accept user into group (owner only) |
| `upload_file <groupid> <filepath>` | Share file with group |
| `list_files <groupid>` | View files available in group |
| `download_file <groupid> <filename> <destpath>` | Download file from group |
| `stop_share <groupid> <filename>` | Stop seeding a file |
| `show_downloads` | Display active download progress |
| `quit` | Gracefully shutdown client |

---
## Server (Tracker)

The Server functions as the **Tracker**, managing all central metadata for user accounts, group membership, and shared file information (`FileInfo` objects). It uses a single TCP listener and spawns a new thread for every incoming client connection.

### Key Responsibilities:
* **User/Group Management**: Creating users, groups, and handling requests.
* **Metadata Storage**: Storing file metadata shared by users.
* **State Synchronization**: Syncing its state with secondary trackers.

## Tracker Synchronization

The system employs a **Primary-Secondary** replication model where the primary tracker broadcasts all state changes to secondary trackers on a dedicated sync port (`tracker_port + 1000`).

### Synchronization Protocol Message Formats
All sync messages follow the format: `OPERATION | data`.

| Operation | Data Format | Description |
| :--- | :--- | :--- |
| `CREATE_USER` | `userid password` | New user registration. |
| `LOGIN` | `userid password` | User login event. |
| `LOGOUT` | `userid` | User logout, cleanup seeders. |
| `CREATE_GROUP` | `groupid ownerid` | New group creation. |
| `JOIN_GROUP` | `groupid userid` | Group join request. |
| `LEAVE_GROUP` | `groupid userid` | User leaves group. |
| `ACCEPT_REQUEST` | `groupid userid` | Accept join request. |
| `UPLOAD_FILE` | `groupid filename filepath filesize uploaderid fullsha userid port ip piece1 piece2 ...` | File metadata registration. |
| `PIECE_COMPLETED` | `groupid filename pieceindex userid ip port` | Piece download completion. |
| `REMOVE_SEEDER` | `groupid filename userid ip port` | Remove specific seeder. |
| `REMOVE_FILE` | `groupid filename` | Delete file metadata. |
| `TRANSFER_OWNERSHIP` | `groupid oldowner newowner` | Group ownership transfer. |
| `DELETE_GROUP` | `groupid` | Complete group deletion. |

## Client to Client Communication

Peers communicate directly for file piece transfers, with each client running a Download Server listening on `client_port + 2000`.

### Peer Request Protocol
| Request/Response | Format |
| :--- | :--- |
| **Piece Request** | `GET_PIECE|filename|pieceIndex`. |
| **Piece Response** | `PIECE_DATA|byteCount|\n` followed by **raw binary data**. |

## Piece Selection Strategy and Thread Pool

The implementation uses a **Sequential strategy**, submitting pieces in order (0, 1, 2, ...) to an I/O-optimized **Thread Pool** for concurrent execution.

### Strategy Details:
* **Initial Selection**: Pieces are submitted to the thread pool in order.
* **Retry Logic**: When the thread pool becomes idle, the main download worker re-submits tasks for failed pieces, automatically retrying with **alternative seeders**.
* **Integrity**: Every downloaded piece's **SHA1 hash** is verified against the tracker metadata