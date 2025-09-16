#if !defined(CONSTRUCTS_H)
#define CONSTRUCTS_H

#include <bits/stdc++.h>
using namespace std;

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
};


class Request{

};

// maybe i'll use it - so i kept it !
class System{
public:
     
    
};

unordered_map<string, User*> users;
unordered_map<string, Group*> groups;
unordered_map<string, User*> loggedInUsers; 
unordered_map<string, Group*> groupOwners; //  initially planned using <User, Group> -> found that it'll increase complexity -> needed to write some custom logic to handle USER as a key
   

#endif // CONSTRUCTS_H
