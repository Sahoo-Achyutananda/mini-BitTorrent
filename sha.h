#if !defined(SHA_H)
#define SHA_H


#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include "constructs.h"
using namespace std;

string calculateSHA1(const string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH]; // that constant is 20
    SHA1((unsigned char*)data.c_str(), data.length(), hash);
    
    stringstream ss;
    // we create a stringstream and push the output into the ss
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i]; // the result is somting like - a lowercase hexadecimal string of length 40, representing the SHA1 digest
    }
    return ss.str();
}

string calculateFileSHA1(const string& filePath) {
    ifstream file(filePath, ios::binary);
    if(!file.is_open()) return "";
    
    // 
    string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();
    
    return calculateSHA1(fileContent);
}


#endif // SHA_H
