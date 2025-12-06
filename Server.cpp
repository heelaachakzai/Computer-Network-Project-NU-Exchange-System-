#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

#define TCP_PORT 15000
#define UDP_PORT 15001

struct CampusClient {
    std::string campus;
    int tcp_socket;
    std::string department;
    time_t last_udp_heartbeat;
    std::string ip_address;
    int udp_port;
};

std::map<std::string, CampusClient> campus_clients;
std::map<std::string, std::string> campus_passwords = {
    {"Lahore", "NU-LHR-375"},
    {"Karachi", "NU-KHI-481"},
    {"Peshawar", "NU-PSH-226"},
    {"CFD", "NU-CFD-190"},
    {"Multan", "NU-MUX-957"}
};
std::mutex clients_mutex;
int udp_sock_global; // Global UDP socket for broadcasting

// Helper: parse fields from TCP messages
std::map<std::string, std::string> parse_msg(const std::string &msg) {
    std::map<std::string, std::string> fields;
    size_t pos = 0;
    std::string token;
    std::string str = msg;
    while ((pos = str.find('|')) != std::string::npos) {
        token = str.substr(0, pos);
        size_t eq = token.find('=');
        if (eq != std::string::npos)
            fields[token.substr(0, eq)] = token.substr(eq + 1);
        str.erase(0, pos + 1);
    }
    return fields;
}

void handle_client(int client_socket, std::string client_ip) {
    char buffer[1024];
    int len = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    if (len <= 0) { close(client_socket); return; }
    buffer[len] = '\0';
    std::string message(buffer);

    // AUTH|Campus=Lahore|Pass=NU-LHR-375|UDPPort=16000|
    auto fields = parse_msg(message);
    std::string campus = fields["Campus"];
    std::string pass = fields["Pass"];
    std::string udp_port_str = fields["UDPPort"];
    
    if (campus_passwords.count(campus) == 0 || campus_passwords[campus] != pass) {
        send(client_socket, "FAIL\n", 5, 0);
        close(client_socket);
        cout << "[Authentication Fail] " << campus << endl;
        return;
    }

    send(client_socket, "OK\n", 3, 0);
    cout << "[Connected] " << campus << " from " << client_ip << endl;

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        campus_clients[campus] = CampusClient{
            campus, 
            client_socket, 
            "", 
            time(nullptr),
            client_ip,
            udp_port_str.empty() ? 16000 : stoi(udp_port_str)
        };
    }

    // Main loop: receive TCP messages and route/display
    while (true) {
        int len = recv(client_socket, buffer, sizeof(buffer)-1, 0);
        if (len <= 0) break;
        buffer[len] = '\0';
        std::string msg(buffer);
        
        // Check if it's a message type
        if (msg.find("MSG|") == 0) {
            auto fields = parse_msg(msg);
            std::string from_campus = fields["FromCampus"];
            std::string from_dept = fields["FromDept"];
            std::string to_campus = fields["ToCampus"];
            std::string to_dept = fields["ToDept"];
            std::string body = fields["Body"];
            
            cout << "[Message Received] From: " << from_campus << "/" << from_dept 
                 << " To: " << to_campus << "/" << to_dept << " - " << body << endl;
            
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (campus_clients.count(to_campus)) {
                // Fixed format with proper separators
                std::string forward = "MSG|FromCampus=" + from_campus + 
                                    "|FromDept=" + from_dept + 
                                    "|ToCampus=" + to_campus +
                                    "|ToDept=" + to_dept +
                                    "|Body=" + body + "|\n";
                
                int sent = send(campus_clients[to_campus].tcp_socket, forward.c_str(), forward.size(), 0);
                if (sent > 0) {
                    cout << "[Message Delivered] To " << to_campus << endl;
                    // Send acknowledgment back to sender
                    std::string ack = "ACK|Status=Delivered|ToCampus=" + to_campus + "|\n";
                    send(client_socket, ack.c_str(), ack.size(), 0);
                } else {
                    cout << "[Message Failed] Could not deliver to " << to_campus << endl;
                    std::string ack = "ACK|Status=Failed|ToCampus=" + to_campus + "|\n";
                    send(client_socket, ack.c_str(), ack.size(), 0);
                }
            } else {
                cout << "[Message Failed] Campus " << to_campus << " not connected" << endl;
                std::string ack = "ACK|Status=NotConnected|ToCampus=" + to_campus + "|\n";
                send(client_socket, ack.c_str(), ack.size(), 0);
            }
        }
    }

    // Disconnection cleanup
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        campus_clients.erase(campus);
    }
    cout << "[Disconnected] " << campus << endl;
    close(client_socket);
}

void udp_heartbeat_listener() {
    udp_sock_global = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(UDP_PORT);
    bind(udp_sock_global, (sockaddr *)&serv_addr, sizeof(serv_addr));
    char buffer[512];

    cout << "[UDP Listener] Started on port " << UDP_PORT << endl;

    while (true) {
        sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int n = recvfrom(udp_sock_global, buffer, sizeof(buffer)-1, 0, (sockaddr *)&from_addr, &from_len);
        if (n > 0) {
            buffer[n] = '\0';
            std::string msg(buffer);
            // HB|Campus=Lahore|
            auto fields = parse_msg(msg);
            std::string campus = fields["Campus"];
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (campus_clients.count(campus)) {
                    campus_clients[campus].last_udp_heartbeat = time(nullptr);
                    cout << "[Heartbeat] " << campus << endl;
                }
            }
        }
    }
}

void broadcast_udp_announcement(const std::string &announcement) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    if (campus_clients.empty()) {
        cout << "No campuses connected to broadcast to." << endl;
        return;
    }
    
    std::string msg = "ANNOUNCEMENT|Body=" + announcement + "|\n";
    
    for (auto &kv : campus_clients) {
        sockaddr_in client_addr;
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(kv.second.udp_port);
        inet_pton(AF_INET, kv.second.ip_address.c_str(), &client_addr.sin_addr);
        
        int sent = sendto(udp_sock_global, msg.c_str(), msg.size(), 0, 
                         (sockaddr*)&client_addr, sizeof(client_addr));
        
        if (sent > 0) {
            cout << "[Broadcast] Sent to " << kv.first << " at " 
                 << kv.second.ip_address << ":" << kv.second.udp_port << endl;
        } else {
            cout << "[Broadcast Failed] Could not send to " << kv.first << endl;
        }
    }
}

void admin_console() {
    sleep(1); // Wait for UDP socket to initialize
    
    while (true) {
        cout << "\n========== ADMIN MENU ==========\n";
        cout << "1. Show Campus Status\n";
        cout << "2. Broadcast Announcement (UDP)\n";
        cout << "3. Show Connected Campuses\n";
        cout << "4. Exit\n";
        cout << "==================================\n";
        
        int opt; 
        cout << "Enter Option: "; 
        cin >> opt;
        cin.ignore(); // Clear newline from buffer
        
        if (opt == 1) {
            std::lock_guard<std::mutex> lock(clients_mutex);
            cout << "\n--- Campus Status ---\n";
            if (campus_clients.empty()) {
                cout << "No campuses connected.\n";
            } else {
                for (auto &kv : campus_clients) {
                    time_t now = time(nullptr);
                    int seconds_ago = now - kv.second.last_udp_heartbeat;
                    cout << kv.first << " - Last heartbeat: " << seconds_ago << " seconds ago" 
                         << " [" << kv.second.ip_address << "]\n";
                }
            }
        } 
        else if (opt == 2) {
            cout << "Enter announcement message: ";
            std::string announcement;
            getline(cin, announcement);
            
            if (!announcement.empty()) {
                cout << "\nBroadcasting to all campuses...\n";
                broadcast_udp_announcement(announcement);
                cout << "Announcement sent!\n";
            } else {
                cout << "Empty announcement. Cancelled.\n";
            }
        }
        else if (opt == 3) {
            std::lock_guard<std::mutex> lock(clients_mutex);
            cout << "\n--- Connected Campuses ---\n";
            if (campus_clients.empty()) {
                cout << "No campuses connected.\n";
            } else {
                for (auto &kv : campus_clients) {
                    cout << "- " << kv.first << " [" << kv.second.ip_address 
                         << ":TCP-" << TCP_PORT << ",UDP-" << kv.second.udp_port << "]\n";
                }
            }
        }
        else if (opt == 4) {
            cout << "Shutting down server...\n";
            exit(0);
        }
        else {
            cout << "Invalid option!\n";
        }
    }
}

int main() {
    cout << "========================================\n";
    cout << "  Islamabad Campus Server: ONLINE\n";
    cout << "========================================\n\n";
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    cout << "[TCP Server] Listening on port " << TCP_PORT << "\n";

    std::thread udp_thread(udp_heartbeat_listener);
    udp_thread.detach();

    std::thread admin_thread(admin_console);
    admin_thread.detach();

    while (true) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_socket = accept(server_fd, (sockaddr*)&client_addr, &addrlen);
        
        // Get client IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        std::thread th(handle_client, client_socket, std::string(client_ip));
        th.detach();
    }
}
