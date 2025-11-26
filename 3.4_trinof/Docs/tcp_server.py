# Simple TCP server for receiving sensor, station, and calibration data from ESP32/Arduino devices.
# Uses asyncio for handling multiple simultaneous connections.
# 
# - Listens on all interfaces at the specified port.
# - Receives lines starting with "AT$", "AT&", or "AT%" and ending with "#".
# - Appends sensor data to "sensor_data.csv", station data to "station_data.csv", and calibration data to "calibration_data.csv".
# - Sends "$" as acknowledgment for each valid line received.
# 
# Usage:
#   Run this script on your PC/server. Configure your ESP32/Arduino to send data to this IP and port.
#   Each received line is saved to its corresponding CSV file.

import asyncio

HOST = ''         # Listen on all interfaces
PORT = 11234      # Use the same port as your ESP32 code

# Define constants for line indicators and acknowledgment
START_SENSOR = "AT$"
START_STATION = "AT&"
START_CALIBRATION = "AT%"
END_CHAR = "#"
ACK_CHAR = b"$"

async def handle_client(reader, writer):
    addr = writer.get_extra_info('peername')
    print('Connected by', addr)
    while True:
        data = await reader.read(1024)
        if not data:
            break
        line = data.decode().strip()
        print("Received:", line)
        # Check start and end indicators using constants
        if ((line.startswith(START_STATION) or line.startswith(START_SENSOR) or line.startswith(START_CALIBRATION)) and line.endswith(END_CHAR)):
            writer.write(ACK_CHAR)
            await writer.drain()
            if line.startswith(START_SENSOR):
                with open("sensor_data.csv", "a") as f:
                    f.write(line + "\n")
            elif line.startswith(START_STATION):
                with open("station_data.csv", "a") as f:
                    f.write(line + "\n")
            elif line.startswith(START_CALIBRATION):
                with open("calibration_data.csv", "a") as f:
                    f.write(line + "\n")
    writer.close()
    await writer.wait_closed()

async def main():
    server = await asyncio.start_server(handle_client, HOST, PORT)
    print(f"AsyncIO server listening on port {PORT}...")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
