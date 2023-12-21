# Reliable Data Transfer and Congestion Control

This is a project to get familiar with the concepts of reliable data transfer, congestion control, and building client-server applications over UDP. I implemented a client and server that can upload files from the client to the server over UDP while handling packet loss and implementing reliability and congestion control.

## Getting Started

### Prerequisites

- C compiler 
- Python 3.7.3 for running the `rdcc_proxy.py` link simulator

### Compiling the Code

Use the provided `Makefile` for compiling the server and client applications:

```sh
make build
```

This will generate two executable files: `server` and `client`.

To clean the build files, simply run:

```sh
make clean
```

### Running the Link Simulator

To simulate the link with packet loss and queuing, run the `rdcc_proxy.py` script:

```sh
python rdcc_proxy.py [options]
```

Options:

- `--in_port_from_client <port>`: Port to listen to incoming packets from client.
- `--in_port_from_server <port>`: Port to listen to incoming packets from server.
- `--client_port <port>`: Port where the client is reachable.
- `--server_port <port>`: Port where the server is reachable.
- `--test_type <rd|cc>`: Choose 'rd' for random packet drop simulation or 'cc' for congestion control simulation.
- `--loss_rate <rate>`: The probability of packet loss in 'rd' mode.
- `--token_rate <rate>`: The token refill rate for the TokenBucket in 'cc' mode.
- `--token_capacity <capacity>`: The capacity of the TokenBucket in 'cc' mode.
- `--queue_size <size>`: The maximum size of the queue in 'cc' mode.
- `--random_seed <seed>`: The random seed for packet loss simulation.
- `--prop_delay <delay>`: The propagation delay in seconds.

Example:

```sh
python rdcc_proxy.py --test_type rd --loss_rate 0.1
```

### Running the Client and Server

First, run the server:

```sh
./server
```

Then, in a separate terminal, run the client providing the filename to upload:

```sh
./client input.txt
```
