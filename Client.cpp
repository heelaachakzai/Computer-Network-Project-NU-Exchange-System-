#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <atomic>

using namespace std;

#define SERVER_IP "127.0.0.1"  // Change to actual server IP
#define TCP_PORT 15000
#define UDP_PORT 15001

int tcp_socket_global;
std::string my_campus;
std::string my_department;
std::atomic<bool> running(true);
int my_udp_port; // Each client will have unique UDP port

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

void receive_messages() {
    char buffer[1024];
    
    while (running) {
        int len = recv(tcp_socket_global, buffer, sizeof(buffer)-1, 0);
        if (len <= 0) {
            cout << "\n[ERROR] Disconnected from server.\n";
            running = false;
            break;
        }
        
        buffer[len] = '\0';
        std::string msg(buffer);
        
        if (msg.find("MSG|") == 0) {
            auto fields = parse_msg(msg);
            std::string from_campus = fields["FromCampus"];
            std::string from_dept = fields["FromDept"];
            std::string body = fields["Body"];
            cout << endl;
            cout << "\n╔════════════════════════════════════╗\n";
            cout << "║       NEW MESSAGE RECEIVED         ║\n";
            cout << "╚════════════════════════════════════╝\n";
            cout << "From: " << from_campus << " / " << from_dept << "\n";
            cout << "Message: " << body << "\n";
            cout << "────────────────────────────────────\n";
            cout << endl;
        }
        else if (msg.find("ACK|") == 0) {
            auto fields = parse_msg(msg);
            std::string status = fields["Status"];
            std::string to_campus = fields["ToCampus"];
            
            cout << "\n[Delivery Status] Message to " << to_campus << ": " << status << "\n";
        }
    }
}

void send_heartbeat() {
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    while (running) {
        std::string hb = "HB|Campus=" + my_campus + "|";
        sendto(udp_sock, hb.c_str(), hb.size(), 0, 
               (sockaddr*)&server_addr, sizeof(server_addr));
        
        sleep(5); // Send heartbeat every 5 seconds
    }
    close(udp_sock);
}

void receive_udp_announcements() {
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Enable SO_REUSEADDR to help with binding
    int opt = 1;
    setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(my_udp_port);
    
    if (bind(udp_sock, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        cout << "[ERROR] Could not bind UDP socket for announcements on port " << my_udp_port << "\n";
        perror("bind");
        return;
    }
    
    cout << "[UDP Listener] Ready to receive announcements on port " << my_udp_port << "\n";
    
    char buffer[512];
    while (running) {
        sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int n = recvfrom(udp_sock, buffer, sizeof(buffer)-1, 0, 
                        (sockaddr*)&from_addr, &from_len);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string msg(buffer);
            
            if (msg.find("ANNOUNCEMENT|") == 0) {
                auto fields = parse_msg(msg);
                std::string body = fields["Body"];
                cout << endl;
                cout << "\n╔════════════════════════════════════════════╗\n";
                cout << "║     SYSTEM-WIDE ANNOUNCEMENT (ADMIN)       ║\n";
                cout << "╚════════════════════════════════════════════╝\n";
                cout << body << "\n";
                cout << "────────────────────────────────────────────\n";
                cout << endl;
            }
        }
    }
    close(udp_sock);
}

void display_menu() {
    cout << "\n========== CAMPUS MENU ==========\n";
    cout << "Campus: " << my_campus << "\n";
    cout << "Department: " << my_department << "\n";
    cout << "=================================\n";
    cout << "1. Send Message to Another Campus\n";
    cout << "2. Change Department\n";
    cout << "3. Exit\n";
    cout << "=================================\n";
}

void send_message_to_campus() {
    cout << "\n--- Send Message ---\n";
    cout << "Available Campuses: Lahore, Karachi, Peshawar, CFD, Multan\n";
    cout << endl;
    cout << "Enter destination campus: ";
    std::string to_campus;
    cin.ignore();
    getline(cin, to_campus);
    
    cout << "Enter destination department: ";
    std::string to_dept;
    getline(cin, to_dept);
    
    cout << "Enter your message: ";
    std::string body;
    getline(cin, body);
    
    std::string msg = "MSG|FromCampus=" + my_campus + 
                     "|FromDept=" + my_department +
                     "|ToCampus=" + to_campus +
                     "|ToDept=" + to_dept +
                     "|Body=" + body + "|";
    cout << endl;
    int sent = send(tcp_socket_global, msg.c_str(), msg.size(), 0);
    if (sent > 0) {
        cout << "[Sent] Message sent to " << to_campus << "\n";
    } else {
        cout << "[ERROR] Failed to send message\n";
    }
}

int main() {
    cout << "========================================\n";
    cout << "    Campus Client - NU Pakistan\n";
    cout << "========================================\n\n";
    
    cout << "Select your campus:\n";
    cout << "1. Lahore\n2. Karachi\n3. Peshawar\n4. CFD\n5. Multan\n";
    cout << "Enter choice: ";
    int choice;
    cin >> choice;
    cout << endl;
    
    // Assign unique UDP port based on campus choice
    std::map<int, int> campus_udp_ports = {
        {1, 16001},  // Lahore
        {2, 16002},  // Karachi
        {3, 16003},  // Peshawar
        {4, 16004},  // CFD
        {5, 16005}   // Multan
    };
    
    std::map<int, std::pair<std::string, std::string>> campus_creds = {
        {1, {"Lahore", "NU-LHR-375"}},
        {2, {"Karachi", "NU-KHI-481"}},
        {3, {"Peshawar", "NU-PSH-226"}},
        {4, {"CFD", "NU-CFD-190"}},
        {5, {"Multan", "NU-MUX-957"}}
    };
    
    if (campus_creds.count(choice) == 0) {
        cout << "Invalid choice!\n";
        return 1;
    }
    
    my_campus = campus_creds[choice].first;
    std::string password = campus_creds[choice].second;
    my_udp_port = campus_udp_ports[choice]; // Set unique UDP port
    
    cout << "Enter your department: ";
    cin.ignore();
    getline(cin, my_department);
    
    // Connect via TCP
    tcp_socket_global = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    if (connect(tcp_socket_global, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cout << "[ERROR] Could not connect to server\n";
        return 1;
    }
    
    // Send authentication with unique UDP port
    std::string auth = "AUTH|Campus=" + my_campus + "|Pass=" + password + "|UDPPort=" + to_string(my_udp_port) + "|";
    send(tcp_socket_global, auth.c_str(), auth.size(), 0);
    
    char buffer[128];
    int len = recv(tcp_socket_global, buffer, sizeof(buffer)-1, 0);
    buffer[len] = '\0';
    
    if (std::string(buffer).find("OK") != std::string::npos) {
        cout << "\n[SUCCESS] Connected to Islamabad Server as " << my_campus << "\n";
        cout << "[INFO] UDP Port assigned: " << my_udp_port << "\n";
    } else {
        cout << "[ERROR] Authentication failed\n";
        close(tcp_socket_global);
        return 1;
    }
    
    // Start background threads
    std::thread recv_thread(receive_messages);
    recv_thread.detach();
    
    std::thread hb_thread(send_heartbeat);
    hb_thread.detach();
    
    std::thread udp_announce_thread(receive_udp_announcements);
    udp_announce_thread.detach();
    
    sleep(1); // Give threads time to start
    
    // Main menu loop
    while (running) {
        display_menu();
        int opt;
        cout << endl;
        cout << "Enter Option: ";
        cin >> opt;
        cout << endl;
        if (opt == 1) {
            send_message_to_campus();
        }
        else if (opt == 2) {
            cout << "Enter new department: ";
            cin.ignore();
            getline(cin, my_department);
            cout << "Department changed to: " << my_department << "\n";
        }
        else if (opt == 3) {
            cout << "Disconnecting...\n";
            running = false;
            close(tcp_socket_global);
            sleep(1);
            exit(0);
        }
        else {
            cout << "Invalid option!\n";
        }
    }
    
    return 0;
}
