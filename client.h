#if !defined(CLIENT_H)
#define CLIENT_H

#include "client_constructs.h"
#include "constructs.h"
#include "fileops.h"

string currentUserId = "";
string currentPassword = "";
bool loggedIn = false;

int trackerIndex = 0;
int sockfd = -1;
bool trackerAlive = false;
pthread_mutex_t tracker_mutex = PTHREAD_MUTEX_INITIALIZER;

// why this exists - 
// initially i made the code in such a way that the client sends a request to the tracker and the tracker responds back with some acknowledgement.
// if the params are valid the respective actions are performed and the tracker DS are updated.
// the above was managed using a simple read and write loop -> which was buggy and later fixed using FD_SET 
// now the client has to accepts it's DSs -> the downloads one 
// void showActiveDownloads();



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
                cout << fontBold << colorGreen << "[Completed] [" << download->groupId << "] " << fileName << reset << endl;
            } else {
                cout << fontBold << colorBlue << "[Downloading] [" << download->groupId << "] " << fileName << " (" << fixed << setprecision(1) << progress << "%)" << reset << endl;
            }
        }
    }
    
    pthread_mutex_unlock(&download_mutex);
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


// Modified login handling in client
bool handleLogin(string userId, string password) {
    string command = "login " + userId + " " + password;
    
    pthread_mutex_lock(&tracker_mutex);
    if(!trackerAlive || sockfd == -1) {
        pthread_mutex_unlock(&tracker_mutex);
        return false;
    }
    
    int n = write(sockfd, command.c_str(), command.length());
    pthread_mutex_unlock(&tracker_mutex);
    
    if(n > 0) {
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
    else if(tokens[0] == "help"){
        showHelp();
        return true;
    }
    
    return false;
}

#endif // CLIENT_H
