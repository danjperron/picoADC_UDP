Just a proof of concept  to push 200K samples/sec of analog signal conversion from Pico via UDP protocol.


The Pico code is written in C-SDK

The adcReader is written in GCC


The pico will broadcast a ping style packet so the adcReader will figure out what the Pico IP is.
Once it receives the Ping the adcReader will send a start command. The start command will start the adc data streams.
Also with the start command the Pico will know the IP to send the data.

Upon overrun the Pico will stop and start the ping broadcast again.

Core1 takes care of the ADC DMA and push the value into a circular fifo block which is pratically all the ram<br>
Core0 reads the fifo block and transfer it to the UDP socket.<br>

The adcReader is the receiver application. It will output to the stdout!<br>

To store the udp stream which contains ADC values you just need to pipe the output to a file.<br>

ex:<br>
&nbsp;&nbsp;&nbsp; <b>./adcReader >advalue.dat</b><br>
<br>
or use this method to split in 2GB files.<br>
&nbsp;&nbsp;&nbsp; <b>./adcReader | spit -b 2G - advalue.dat_</b><br>

The adcReader presently startthe pico process and acknowledge each data transfer. 
It also use the same fifoBlock.c file uses by picoADC_UDP.
After 5 seconds of  not receiving data it will exit.
Except for the adc values all the print are directed to stderr.


<b>It is working </b><br><br>
Right now I was able to transfer 200K samples/sec. To to that I pratically used the full memory like buffer to be able to resend
unreceived packet.  a buffer of 140 blocks of 700  16 bits ADC values  roughly ~200K bytes.

Be aware that the wifi needs not to be used by other devices too much.

At home with all my IOT devices, mostly MQTT (~20 units), The pico was able to handle  the 200K transfer.
like the next table will show sometimes I got near the top.<br>
Very often the number of block transfer is ack right away but sometimes it piles up because of unreceived packet or not received in order.<br>

<br>
first number, current fifo block (-1 means  fifo empty)<br>
second numner, the total number of blocks need to be transfer.<br>
<blockquote>blockReady : 119 /2
blockReady : -1 /0 <br>
blockReady : 128 /1<br>
blockReady : 90 /10<br>
blockReady : -1 /0<br>
blockReady : 25 /20<br>
blockReady : 18 /19<br>
blockReady : 37 /1<br>
blockReady : -1 /0<br>
blockReady : 0 /9<br>
blockReady : -1 /0<br>
blockReady : 52 /106   <= this is heavy pile up (max is 140)<br>
blockReady : 30 /1<br>
blockReady : 132 /7<br>
blockReady : -1 /0<br>
blockReady : 128 /2<br>
blockReady : 12 /1<br>
blockReady : 22 /8<br>
blockReady : 108 /11<br>
blockReady : 89 /5<br></blockquote>

<br>

<b>To compile the Pico code,</b>
<blockquote>
mkdir build<br>
cd build<br>
cmake -DPICO_BOARD=pico_w -DWIFI_SSID="Your WIFI ESSID" -DWIFI_PASSWORD="Your WIFI password" ..<br>
make<br>
</blockquote>

<br>
<b>To compile adcReader,</b>
<blockquote>
gcc -pthread adcReader.c fifoBlock.c -lpthread -o adcReader
</blockquote>
<br>
