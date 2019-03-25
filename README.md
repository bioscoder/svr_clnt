# svr_clnt
Yet another client-server

Zlib 1.2.11 should be installed.

Client avalible parameters:

    -H  server IP adress
	  -I  Input file for manupulations
	  -T  Compression/decomression method
	         0 - no compression
	         1 - Zlib deflate
	         2 - Zlib inflate
	  -p  Client threading mode
	         0 - Single threa
	         1 - Addintional Single thread for rx and tx
	         2 - Addintional Separate threads for rx and tx
	  Exapmle: ./hclient -H 127.0.0.1 -I test.bin -T 0 -p 1
	           ./hclient -H 127.0.0.1 -I test.bin -T 1 - p 1
	           ./hclient -H 127.0.0.1 -I test.bin.zdfl -T 2 -p 1
		   		   
Server has no paramters.

