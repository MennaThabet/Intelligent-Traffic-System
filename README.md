# Intelligent-Traffic-System
Intelligent traffic is a system designed to reduce traffic jam and road traffic injures (RTIs). 

It depends on counting the number of cars on each road, regulating traffic in all road junctions by providing traffic lights with opening times relevant to cars number in these roads. Also, opening the traffic lights in each road whenever an emergency vehicle is found nearby.

The construction of prototype was made of by specific steps as follows:
First, Ultrasonic sensors were connected to Arduino uno to detect the passing cars in both intersected roads 
Second, the breadboard was connected to the Arduino uno by the jumper wires.
Third, the bush button was fixed on the side of each road.
Fourth, the light led and push button were connected by wires to the breadboard.
Fifth, the system was coded on Arduino uno to light the green light led. 
The project has specific design requirements, which manage three things; the ability of the ultrasonic sensors to detect a maximum number of cars on the two intersected roads, reducing the response time of the traffic light to turn into green color for any ambulance vehicle, and the low cost of the prototype was accomplished.


Intelligent traffic system is divided into two parts:
The First Part: Ultrasonic sensors were used to detect the number of cars passing the street in a specific time, they emit short, 
high-frequency sound pulses at regular intervals, If they strike a moving car, then they are reflected back as echo signals to the sensor 
so it can count it. 
This equation detects the number of seconds the green light opens for one of the two intersected roads.
                                            𝒕(𝒙) = (𝒙/𝒙+y+0.01)∗𝟔𝟎+𝟑𝟎
t(x): Number of seconds the green light opens for one of the two intersected roads,  x: Number of cars in that road, and y: Number of cars in the other road.
The equation has a maximum duration of 90 seconds and a minimum duration of 30 seconds to maintain balance between the two roads. So the sum of the interval will be 120 seconds, after this seconds passes, the number of cars on each road will be counted again.  These values are obtained at the greatest difference between the number of cars in the two roads.
So, when the value of the cars in road x is maximum and road y doesn’t have any cars, the green light will open for 90 seconds on road x.
The jumper wires were also used to connect the sensor to the breadboard and then to the Arduino.
We also used Arduino UNO as it is an open-source prototyping platform, where we could write and upload the computer code to the physical board

The Second Part: 
A push button was used, so when an emergency vehicle is passing the street, the button will be on one side of the road, it is used as changer for the trafficlights so it can pass easily as lights on other roads will turn into red and on the road which the car is passing the traffic lights will be green.
