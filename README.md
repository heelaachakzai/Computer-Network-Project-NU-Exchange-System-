# Computer Network Project (NU Information Exchange System)
This project builds the NU-Information Exchange System, a C++ terminal based client–server model using TCP/UDP sockets to support real time messaging between all FAST NUCES campuses.
## Running
### How to Run
We utilized VMware and Ubuntu for this project.

- Begin by creating two files: server.cpp and client.cpp.
- Use the following commands to compile and execute the programs:
  
### Server:
```bash
g++ server.cpp -o server
./server
```
### Client:
```bash
g++ client.cpp -o client
./client
```


## Concurrency Works 

### Server

The server uses multiple threads to manage tasks at the same time:
- One thread per client → allows multiple campuses to connect together.
- One thread for UDP heartbeats → constantly checks which campuses are online.
- One thread for the admin console → lets admins broadcast messages and view campus status.
- Shared data (like client lists) is protected using a mutex, so threads don’t corrupt data.
- 
## Client

Each client runs background threads to:

- receive messages
- send heartbeat signals every 5 seconds
- receive admin announcements

## Thread Tools Used

- std::thread → runs functions in parallel
- .detach() → makes threads run independently
- std::mutex → prevents race conditions
- std::atomic → thread-safe stop flag for clean shutdown

## Features

- Reliable TCP messaging with delivery confirmation
- UDP heartbeats for online/offline tracking
- Admin announcements via UDP broadcast
- Login authentication for each campus
- Switch departments without reconnecting
- Live server logs and activity monitoring

