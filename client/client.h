#if !defined(CLIENT_H)
#define CLIENT_H

#include "./tracker/constructs.h"
#include "client_constructs.h"
#include "fileops.h"

string currentUserId = "";
string currentPassword = "";
bool loggedIn = false;

// why this exists - 
// initially i made the code in such a way that the client sends a request to the tracker and the tracker responds back with some acknowledgement.
// if the params are valid the respective actions are performed and the tracker DS are updated.
// the above was managed using a simple read and write loop -> which was buggy and later fixed using FD_SET 
// now the client has to accepts it's DSs -> the downloads one 
// void showActiveDownloads();


// modified this thing to show more details for debugginghgggggggggggggggggggggggg
// modified the formatting using GPT
void showActiveDownloads(){
    pthread_mutex_lock(&download_mutex);
    
    if(activeDownloads.empty()){
        cout << fontBold << colorRed << "No active downloads" << reset << endl;
    }else{
        cout << fontBold << colorGreen << "Active Downloads:" << reset << endl;
        for(auto& [fileName, download] : activeDownloads) {
            int completedPieces = 0;
            for(bool completed : download->downloadedPieces) {
                if(completed) completedPieces++;
            }
            
            double progress = (double)completedPieces / download->totalPieces * 100.0;
            
            if(download->isComplete) {
                cout << fontBold << colorGreen << "[Completed] [" << download->groupId << "] " << fileName << " -> " << download->destPath << reset << endl;
            } else {
                cout << fontBold << colorBlue << "[Downloading] [" << download->groupId << "] " << fileName << " (" << fixed << setprecision(1) << progress << "%) " << completedPieces << "/" << download->totalPieces << " pieces" << reset << endl;
                
                // Show seeder count for each piece
                cout << "  Piece status: ";
                for(int i = 0; i < min(10, (int)download->pieces.size()); i++) {
                    if(download->downloadedPieces[i]) {
                        cout << colorGreen << "✓" << reset;
                    } else {
                        cout << colorRed << "(" << download->pieces[i].seeders.size() << ")" << reset;
                    }
                    cout << " ";
                }
                if(download->pieces.size() > 10) cout << "...";
                cout << endl;
            }
        }
    }
    
    pthread_mutex_unlock(&download_mutex);
    
    // Show seeding info
    pthread_mutex_lock(&seed_mutex);
    if(!seedingFiles.empty()) {
        cout << fontBold << colorYellow << "\nSeeding Files:" << reset << endl;
        for(auto& [fileName, seedInfo] : seedingFiles) {
            cout << "  [" << seedInfo->groupId << "] " << fileName << " (" << seedInfo->pieces.size() << " pieces)" << endl;
        }
    }
    pthread_mutex_unlock(&seed_mutex);
}

// formatting generated using GPT
void showHelp(){
    cout << fontBold << colorBlue << "=== Available Commands ===" << reset << endl;
    cout << fontBold << colorGreen << "User Management:" << reset << endl;
    cout << "  create_user <userid> <password>   - Create new user account" << endl;
    cout << "  login <userid> <password>         - Login to system" << endl;
    cout << "  logout                            - Logout from system" << endl;
    
    cout << fontBold << colorGreen << "Group Management:" << reset << endl;
    cout << "  create_group <groupid>            - Create new group" << endl;
    cout << "  join_group <groupid>              - Request to join group" << endl;
    cout << "  leave_group <groupid>             - Leave group" << endl;
    cout << "  list_groups                       - List all groups" << endl;
    cout << "  list_requests <groupid>           - List join requests (owner only)" << endl;
    cout << "  accept_request <groupid> <userid> - Accept join request (owner only)" << endl;
    
    cout << fontBold << colorGreen << "File Operations:" << reset << endl;
    cout << "  upload_file <groupid> <filepath>  - Upload file to group" << endl;
    cout << "  list_files <groupid>              - List files in group" << endl;
    cout << "  download_file <groupid> <filename> <destpath> - Download file" << endl;
    cout << "  stop_share <groupid> <filename>   - Stop sharing file" << endl;
    
    cout << fontBold << colorGreen << "Client Commands:" << reset << endl;
    cout << "  show_downloads                    - Show download progress" << endl;
    cout << "  help / commands                   - Show this help" << endl;
}


// Modified login handling in client -> the problem was, th logedin details was lost at the tracker end ... maybe the loggein details was not even needed -> dont know !
bool handleLogin(string userId, string password){
    string command = "login " + userId + " " + password + "\n";
    
    pthread_mutex_lock(&tracker_mutex);
    if(!trackerAlive || sockfd == -1){
        pthread_mutex_unlock(&tracker_mutex);
        return false;
    }
    
    int n = write(sockfd, command.c_str(), command.length());
    pthread_mutex_unlock(&tracker_mutex);
    
    if(n > 0){
        // Store credentials for auto-relogin
        currentUserId = userId;
        currentPassword = password;
        loggedIn = true;
        return true;
    }
    return false;
}

bool handleClientCommand(string input, int sockfd, pair<string,int> clientInfo){ // i also receive the socketaddress of the tracker port
    vector<string> tokens; // assuming we receive a trimmed input > no trailing spaces, newlines,tabs
    tokens = tokenizeString(input);
    
    if(tokens.empty()) return false;
    
    // CLIENT-SIDE COMMANDS (don't send to tracker)
    if(tokens[0] == "upload_file"){
        handleUploadFileClient(sockfd, tokens, clientInfo);
        return true;
    }
    if(tokens[0] == "login"){
        if(tokens.size() != 3){
            cout << colorRed  << fontBold << "Usage : login <userid> <password>" << reset << endl;
            return true;
        }
        handleLogin(tokens[1], tokens[2]);
        return true;
    }
    if(tokens[0] == "show_downloads"){
        showActiveDownloads();
        return true;
    }
    if(tokens[0] == "stop_share"){
        if(tokens.size() != 3){
            cout << colorRed << fontBold << "Usage: stop_share <group_id> <file_name>" << reset << endl;
            return true;
        }
        
        string groupId = tokens[1];
        string fileName = tokens[2];
        
        // Check if we're actually seeding this file
        pthread_mutex_lock(&seed_mutex);
        auto it = seedingFiles.find(fileName);
        if(it == seedingFiles.end()) {
            pthread_mutex_unlock(&seed_mutex);
            cout << colorRed << fontBold << "Not seeding this file" << reset << endl;
            return true;
        }
        
        // Remove from local seeding
        delete it->second;
        seedingFiles.erase(it);
        pthread_mutex_unlock(&seed_mutex);
        
        // Notify tracker with client info - i'm passing some extra commands
        string command = "stop_share " + groupId + " " + fileName + " " + clientInfo.first + " " + to_string(clientInfo.second) + "\n";
        
        pthread_mutex_lock(&tracker_mutex);
        if(trackerAlive && sockfd != -1) {
            write(sockfd, command.c_str(), command.length());
        }
        pthread_mutex_unlock(&tracker_mutex);
        
        cout << fontBold << colorGreen << "Stopped seeding: " << fileName << reset << endl;
        return true;
    }
    if(tokens[0] == "debug_pieces"){
        // Debug command to show piece information for a file - kyunki i need to see 
        if(tokens.size() != 2) {
            cout << "Usage: debug_pieces <filename>" << endl;
            return true;
        }
        
        pthread_mutex_lock(&download_mutex);
        auto it = activeDownloads.find(tokens[1]);
        if(it != activeDownloads.end()) {
            DownloadInfo* download = it->second;
            cout << "Piece information for " << tokens[1] << ":" << endl;
            for(const auto& piece : download->pieces) {
                cout << "  Piece " << piece.pieceIndex << ": " << piece.seeders.size() 
                     << " seeders, downloaded=" << (download->downloadedPieces[piece.pieceIndex] ? "yes" : "no") << endl;
                for(const auto& seeder : piece.seeders) {
                    cout << "    -> " << seeder.ip << ":" << seeder.port << endl;
                }
            }
        } else {
            cout << "File not found in active downloads" << endl;
        }
        pthread_mutex_unlock(&download_mutex);
        return true;
    }
    if(tokens[0] == "quit" || tokens[0] == "exit"){
        cout << fontBold << colorYellow << "Shutting down client..." << reset << endl;
        
        // Notify tracker of logout if logged in
        if(loggedIn && !currentUserId.empty()) {
            string logoutCmd = "logout\n";
            pthread_mutex_lock(&tracker_mutex);
            if(trackerAlive && sockfd != -1) {
                write(sockfd, logoutCmd.c_str(), logoutCmd.length());
                usleep(100000);
            }
            pthread_mutex_unlock(&tracker_mutex);
        }
        
        // Clean shutdown
        pthread_mutex_lock(&tracker_mutex);
        if(sockfd != -1) {
            close(sockfd);
            usleep(100000);
            sockfd = -1;
        }
        trackerAlive = false;
        pthread_mutex_unlock(&tracker_mutex);
        
        // Stop download server
        downloadServerRunning = false;
        
        cout << fontBold << colorGreen << "Goodbye!" << reset << endl;
        exit(0);  // Clean exit
    }
    if(tokens[0] == "help"){
        showHelp();
        return true;
    }
    
    return false;
}

#endif // CLIENT_H
