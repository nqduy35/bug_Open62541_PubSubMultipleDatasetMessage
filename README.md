# bug_Open62541_PubSubMultipleDatasetMessage
Demo to reproduce the bug of creating multiple dataset messages sending to Open62541 server

Step 1: Build the project
$ make build

Step 2: Run the server
$ ./open62541-subscriber.c

Step 3: Open UaExpert, access to the endpoint opc.tcp://127.0.0.1:4840

Step 4: Run the client
$ ./my-publisher.c

Step 5: Observation: 
The publisher publish a message integrated with one dataset message (value 11.1) to sensor_1, wait 5 second,  
then it publish a message integrated with one dataset message (value 22.2) to sensor_2, wait 5 second,  
othen it publish a message to both sensors integrated with two dataset messages (value = 0, value = 0).  
-> In the server side: no error  
-> In the UaExpert side: the updated data is not correct. 
