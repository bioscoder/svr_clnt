# svr_clnt
Yet another client-server

Zlib 1.2.11 and Liblzma 5.2.2 should be installed.

Clien avalible parameters:

		-H  server IP adress
		-I  Input file for manipulations
		-T  Compression/decompression method
			0 - no compression
			1 - raw deflate
			2 - raw inflate
			3 - Zlib deflate
			4 - Zlib inflate
			5 - lzma comress
			6 - lzma decomress
			7 - gzip comress
			8 - gzip decomress
		-l  Compression level
		-c  Chunk size for Compression/decomression
		-p  Client threading mode
			0 - Single thread
			1 - Additional Single thread for rx and tx
			2 - Additional Separate threads for rx and tx. !*Is not enough tested*!
		Exapmle: ./hclient -H 127.0.0.1 -I test.bin -T 0 -p 1
				./hclient -H 127.0.0.1 -I test.bin -T 1 - p 1
				./hclient -H 127.0.0.1 -I test.bin.zdfl -T 2 -p 1
		   		   
Server has no parameters.
