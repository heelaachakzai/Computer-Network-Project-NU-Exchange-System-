# Computer Network Project (NU Information Exchange System)
This project builds the NU-Information Exchange System, a C++ terminal based client–server model using TCP/UDP sockets to support real time messaging between all FAST NUCES campuses.

How Concurrency Works : 
Server
The server uses multiple threads to manage tasks at the same time:
One thread per client → allows multiple campuses to connect together.
One thread for UDP heartbeats → constantly checks which campuses are online.
One thread for the admin console → lets admins broadcast messages and view campus status.
Shared data (like client lists) is protected using a mutex, so threads don’t corrupt data.
