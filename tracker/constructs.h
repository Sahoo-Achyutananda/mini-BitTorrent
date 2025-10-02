#if !defined(CONSTRUCTS_H)
#define CONSTRUCTS_H

#include <bits/stdc++.h>
using namespace std;

class User;
class Group;
class FileInfo;

unordered_map<string, User*> users;
unordered_map<string, Group*> groups;
unordered_map<string, User*> loggedInUsers; 
unordered_map<string, Group*> groupOwners; //  initially planned using <User, Group> -> found that it'll increase complexity -> needed to write some custom logic to handle USER as a key
unordered_map<string, FileInfo*> allFiles; // fileName se FileInfo*

// class ClientAddr{
// public :
//     string ipaddress;
//     int port;

//     ClientAddr(string ip, int port){
//         this->ipaddress = ip;
//         this->port = port;
//     }
// };


// Added this construct to handle the logout part - didnt read the doc correctly !
class SeederInfo {
public :
    string userId;
    string ip;
    int port;
    
    SeederInfo(string user, string ip, int port){
        this->ip = ip;
        this->userId = user;
        this->port = port;
    }
};

class FilePiece{
public :
    int pieceIndex;
    string sha1Hash;
    bool isAvailable;
    // vector<pair<int, string>> seeders; // seeders of a particular file piece - 
    vector<SeederInfo> seeders;
    
    FilePiece(int index, string hash){
        this->pieceIndex = index;
        this->sha1Hash = hash;
        this->isAvailable = false;
    }
};

class FileInfo{
public :
    string fileName;
    string filePath;
    long long fileSize;
    string fullFileSHA1;
    vector<FilePiece> pieces;
    string uploaderId;
    string groupId;
    // vector<pair<int,string>> seeders; // list of people having the file
    vector<SeederInfo> seeders;
    vector<pair<int,string>> downloaders; // list of people downloading . .. maybe not needed ! 
    
    FileInfo(string name, string path, long long size, string uploader, string group){
        this->fileName = name;
        this->filePath = path;
        this->fileSize = size;
        this->uploaderId = uploader;
        this->groupId = group;
    }

    void addSeeder(string userId, string ip, int port){
        this->seeders.push_back(SeederInfo(userId, ip, port));
        for (auto &piece : pieces){
            piece.seeders.push_back(SeederInfo(userId, ip, port));
        }
    }

    void removeSeederByUser(const string& userId){
        // Remove from file-level seeders
        vector<SeederInfo> newSeeders;
        for (const auto& s : seeders){
            if (s.userId != userId){
                newSeeders.push_back(s);
            }
        }
        seeders.swap(newSeeders); // wow - aisa bhi hota hai !

        // Remove from all pieces
        for (auto& piece : pieces){
            vector<SeederInfo> newPieceSeeders;
            for (const auto& s : piece.seeders){
                if (s.userId != userId){
                    newPieceSeeders.push_back(s);
                }
            }
            piece.seeders.swap(newPieceSeeders);
        }
    }

};

class User{
private : 
    string userId;
    string password;
    bool isOwner;
public:
    User(string userId, string password){
        this->userId = userId;
        this->password = password;
        this->isOwner = false;
    }

    string getPassword(){ // this shouldnt exist - but duhhhhhhhh
        return this->password;
    }

    string getUserId(){
        return this->userId;
    }

    bool getIsOwner(){
        return this->isOwner;
    }

    void makeOwner(string userId){
        this->isOwner = true;
    }
};


class Group{
private:
    string groupId;
    string ownerId;
    unordered_map<string, User*> groupUsers;
    unordered_map<string, User*> requests;
    unordered_map<string, FileInfo*> sharedFiles;

public:
    Group(string groupId){
        this->groupId = groupId;
    }

    string getOwnerId(){
        return this->ownerId;
    }

    int getGroupUserCount(){
        return this->groupUsers.size();
    }

    void addToGroup(string userId){
        groupUsers[userId] = users[userId];
    }

    bool checkUserExistance(string userId){
        return groupUsers.find(userId) != groupUsers.end() ;
    }

    void removeUser(string userId){
        // if user exist karta hai tehn erase it -
        if(groupUsers.find(userId) != groupUsers.end()){
            groupUsers.erase(userId);
        }
    }

    void addOwner(string ownerId){
        this->ownerId = ownerId;
        groupUsers[ownerId] = users[ownerId];
        groupOwners[ownerId] = this;
    }
    
    void addRequest(string userId){
        requests[userId] = users[userId];
    }

    bool acceptRequest(string userId){
        if(requests.find(userId) != requests.end()){
            groupUsers[userId] = requests[userId];
            requests.erase(userId);
            return true;
        }
        return false;
    }

    vector<string> getRequests(){
        string temp = "";
        vector<string> result;
        for(auto &[userId, u] : requests){
            temp+=userId;
            temp+=" ";
            temp+=(u->getUserId());

            result.push_back(temp);
        }
        return result;
    }

    void addSharedFile(string fileName, FileInfo* fileInfo){
        sharedFiles[fileName] = fileInfo;
    }

    void removeSharedFile(string fileName){
        if(sharedFiles.find(fileName) != sharedFiles.end()){
            sharedFiles.erase(fileName);
        }
    }

    vector<string> getFileList(){
        vector<string> files;
        for(auto& [fileName, fileInfo] : sharedFiles){
            files.push_back(fileName + " " + to_string(fileInfo->fileSize) + " " + fileInfo->uploaderId);
        }
        return files;
    }

    bool fileExists(string fileName){
        if(sharedFiles.find(fileName) != sharedFiles.end()){
            return true;
        }
        return false;
    }

    // sometimes i used fileinfo - for some uploader info
    FileInfo* getFileInfo(string fileName){
        if(sharedFiles.find(fileName) != sharedFiles.end()){
            return sharedFiles[fileName];
        }
        return nullptr;
    }

    bool transferOwnership(string newOwnerId){
        if(groupUsers.find(newOwnerId) == groupUsers.end()){
            return false; // New owner must be a member
        }
        
        if(!ownerId.empty()){
            groupOwners.erase(ownerId);
        }
        
        ownerId = newOwnerId;
        groupOwners[newOwnerId] = this;
        
        return true;
    }
    
    string getNextEligibleOwner(){
        for(auto &[userId, user] : groupUsers){
            if(userId != ownerId){
                return userId;
            }
        }
        return "";
    }
    
    // checks for a particular group if its an owner or not -> the group info is needed
    bool isOwner(string userId){
        return ownerId == userId;
    }
};

//checks if it is part of the owner list - group info is not needed !
bool isGroupOwner(string userId){
    return groupOwners.find(userId) != groupOwners.end();
}

bool isGroup(string groupId){
    return groups.find(groupId) != groups.end();
}

bool isUser(string userId){
    return users.find(userId) != users.end();
}

bool isLoggedIn(string userId){
    return loggedInUsers.find(userId) != loggedInUsers.end();
}

// to trim extra spaces/tabs and stuff -
string trim(string s){
    int start = s.find_first_not_of(" \n\r\t"); // find a valid start index
    int end = s.find_last_not_of(" \n\r\t"); // find the vlid end index
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

vector<string> tokenizeString(string s){
    vector<string> result;
    stringstream stream(s);

    string temp;
    while(getline(stream, temp, ' ')){
        result.push_back(trim(temp));
    }
    return result;
}


void writeToClient(int sockfd, string msg){
    msg+='\n';
    write(sockfd, msg.c_str(), msg.size());
}

#endif // CONSTRUCTS_H
