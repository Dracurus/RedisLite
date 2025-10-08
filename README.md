# RedisLite
A minimal Redis-like in-memory key-value store written in C++ with TCP RESP interface and AOF persistence.

## Features
- In-memory key-value store with lazy TTL expiry
- Supports SET, GET, DEL, EXISTS commands
- Append-Only File (AOF) persistence
- RESP protocol compatible (works with redis-cli)
- Multi-client TCP server using socket programming and multithreading
- Thread-safe access to the key-value store with mutexes

## Basic Workflow
Clients connect via TCP and send commands in the RESP protocol.  
Commands are parsed, executed on the in-memory store, optionally logged to the AOF file, and the response is sent back to the client.

## Usage with redis-cli
```bash
redis-cli -p 6379
> SET mykey myvalue
> GET mykey
> DEL mykey
> SET session EX 5
> GET session
