```bash
dd if=/dev/urandom of=onegb.bin bs=1M count=1000
```


# mini-BitTorrent
A simple distributed file system (a mini bit torrent) built using C++.

## Files Present -

```bash
client.cpp -> the client code
tracker.cpp -> the tracker code
tracker_info.txt -> stores the ip:port combinations of two trackers

// Supporting files -
constructs.h -> contains the classes and data structures used by the client and tracker
synchronize.h -> contains the functions used for syncronization between trackers
colors.h -> not relevant to any functionality, just helps to add some colors to the boring texts
```

## How to Run -

```bash
// trackers -
g++ tracker.cpp -o tracker
./tracker tracker_info.txt 0 // for the first tracker
./tracker tracker_info.txt 1 // for the second tracker

// clients -
g++ client.cpp -o client
./client 127.0.0.1:4050 tracker_info.txt // any port number can be used
```

## Implementation - 

### Tracker to Tracker Communication -
The trackers communicate (pass syunchronization messages) via another port (apart from the one where the tracker is running). This ensures that the port where the trackers are individually running only handle client requests and trackers communicate with each other through another port.
This ensures proper separation of tasks.

- Refer the syncHandler() function for the implementation - present in the synchronize.h file.

- If a tracker is running at port 8050, the synchronization Handler runs on the port 9050 (8050 + 1000). This same port is used to check the health of the tracker too.

### Tracker Synchronization -
A function to check the health status (present in the synchromize.h file) of the trackers was implemented. It periodically checks the trackers and updates the activeTrackers vector. Based on the updated list, the primary tracker is selected. Refer the synchronize.h file for the detailed implementation.

- Supporting functions pass sync messages from the primary tracker to the secondary tracker. The message format is as follows - 
```bash
[SYNC OUT] MESSAGE --> SHOWN ON THE PRIMARY TRACKER WHEN A SYNC MESSAGE IS SENT
[SYNC IN] MESSAGE --> SHOWN ON THE SECONDARY TRACKERS WHEN IT RECECIVES THE SYNC MESSAGE
```

### Tracker to Client Communication -
This is obvious. The tracker details are present in the tracker_info.txt file. Clients read the file and connect to the tracker that is available.
- For every client, a new thread is created at the tracker side to handle the requests.
- Initially the primary tracker is responsible for handling the client requests. For every request, a sync message is sent to the secondary tracker. The respective data structures are updated.


## Existing Issues, Bugs and next plans -

1. Currently only simple sync messages are sent when a single action is performed. This only works properly if both trackers are running together from the start. 
   1. This isnt robust and there are a variety of edge cases that is to be handled. This can be solved by sending batch updates - a queue can be used to store all updates. 
   2. Consider the case where Tracker 1 goes down and Tracker 2 is handling the client requests. Later when Tracker 1 comes up, every data structure of Tracker 1 has to be updated.
2. The case where the owner leaves the group is not handled. This can be done by promoting another user of the group into a owner.
