This task is implemented so that different sensors can send the data to the server using same udp socket.
The server accumulates data received until 'length of the averaging sequence' is met and then averages the 
calculated one along with the time interval by which this sequence is accumulated.

The length of the averaging sequence can be given from config, otherwise default value is set as 10 in code

Below are the default values provided for server otherwise read from the server config
Server ip - 127.0.0.1
Server udp port - 6000
Server tcp port - 8080
Length of average sequence - 10

The server the pushes/adds the average result, along with time inyterval in the Hash table data structure in memory

The Sensor configs are also provided as a config file otherwise default config values are as below
ip - 127.0.0.1
port - 6000

Multiple sensor can send data through udp port to server as per requirement.


Client queries for sensor data can be done by multiple client parallaly, internally server can check which connection fd has
an activity (done through select). Server would reply with all the intervals data with respect to sensor enquired by client and following 
if any other client or same client request measurement for same sensor it would get an "n/a", provided by that time next averaging has not been 
done for the same sensor.

Assumption / Limitations:
1. Number of maximum sensors supported - As of now 100 can be guaranteed without collision, beyond that we need to choose sensor id/device_ids such a way that it doesn't collide with earlier one, keeping in mind that we have 100 buckets in hash table


Improvement
1. Input command line arguments can be asserted to take care unsupported inputs mistakenly, going forward
2. Collision avoidance mechanism can be implemented in hash table to support more sensors and any sensor ids
3. The developed code should be tested more and more and run for longer duration to check if it breaks after a long run
4. Sometimes user/client is stuck after printing output if output is large - as of now we can relaunch the client from another
terminal, we can launch even multiple clients/users as per requirement.
5. If there are multiple intervals' average value accumulated for a particular sensor and the same sensor's value is enquired from client
then server would send all of them with 10 us delay - to be worked out further so that this delay can be alleviated.


Files provided for this task to meet
1.	sensor_simulator.c - sensor simulator
2.	server.c - reads sensor vmeasurements, calculates average and manages user/client's request as per given requirement
3.	client.c - User/client - who would query sensor data from server
4.	server.config – server config file
5.	sensor.config – sensor config file
6.      Makefile - to make server.c




