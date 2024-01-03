adcReader2: adcReader.c
	gcc -pthread -o adcReader -D BLOCK_MAX=400 -lpthread adcReader.c fifoBlock.c

clean:
	rm adcReader

