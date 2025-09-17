#if !defined(CONSTRUCTS_H)
#define CONSTRUCTS_H

#include <bits/stdc++.h>
using namespace std;

class User;
class Group;

unordered_map<string, User*> users;
unordered_map<string, Group*> groups;
unordered_map<string, User*> loggedInUsers; 
unordered_map<string, Group*> groupOwners; //  initially planned using <User, Group> -> found that it'll increase complexity -> needed to write some custom logic to handle USER as a key
   
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
};



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
string trim(string s) {
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


#endif // CONSTRUCTS_H
