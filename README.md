# svr_clnt
Yet another client-server

Zlib 1.2.11 should be installed.

Clien fvalible parameters:
    [-H  server IP adress\n");
	  [-I  Input file for manupulations\n");
	  [-T  Compression/decomression method\n");
	  [       0 - no compression\n");
	  [       1 - Zlib deflate\n");
	  [       2 - Zlib inflate\n");
	  [-p  Client threading mode.\n");
	  [       0 - Single thread\n");
	  [       1 - Addintional Single thread for rx and tx\n");
	  [       1 - Addintional Separate threads for rx and tx\n");
	  Exapmle: ./hclient -H 127.0.0.1 -I test.bin -T 0\n");
	           ./hclient -H 127.0.0.1 -I test.bin -T 1\n");
	           ./hclient -H 127.0.0.1 -I test.bin.zdfl -T 2\n");
             
Server has no paramters.
