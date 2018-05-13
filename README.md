# Temperature-Realtime-Sensor
CIT 595 Computer Systems Programming final project, collaborators are Brian Hardy (https://www.linkedin.com/in/brian-h-0017b9154/) and John Hall (https://www.linkedin.com/in/halljw/)

This program utilizes C, Arduino language (C and C++) and JavaScript to display real time temperature from Arduino in the user interface. 
It also can show tha average temperature, the highest and lowest temperature for the past one hour, and switch to Fahrenheit and back to Celsius. If you connect a motor fan to Arduino, it can also activate the fan by clicking the button in the user interface. 

-How to run the program

Use command "make all" to make server program in terminal.

Run the command line "./server [port number] [serial port (USB) device file]" to run the program. 

Then connect to localhost:[port number], you should be able to the server. 
