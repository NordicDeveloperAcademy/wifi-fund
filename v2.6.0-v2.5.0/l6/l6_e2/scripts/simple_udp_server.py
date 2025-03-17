#!/usr/bin/env python3

# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from socket import *
import sys

PORT = 7777

def main():
    sock = socket(AF_INET, SOCK_DGRAM)
    sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    server_address = ("0.0.0.0", PORT)  # Listen on all available network interfaces
    sock.bind(server_address)
    print("Starting UDP server")
    
    while True:
        data, addr = sock.recvfrom(1024)
        if not data:
            break
        print(f"Data received from client: {data.decode()}")
        sock.sendto(data, addr)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nKeyboard interrupt, closing socket and exiting..")
        sys.exit(130)