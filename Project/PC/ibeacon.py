from influxdb_client_3 import InfluxDBClient3, Point
import json
import time
import serial

array = [0.0] * 3

def parse_data(line):
    #print(f"[JSON] Received: {line!r}")
    try:
        if isinstance(line, str):
            start = line.find('{')
            if start != -1:
                line = line[start:]
            else:
                #print("[WARN] No JSON object found.")
                return []

            parsed = json.loads(line)
        elif isinstance(line, dict):
            parsed = line
        else:
            #print("[WARN] Unsupported input type.")
            return []

        if not isinstance(parsed, dict):
            #print("[WARN] Not a JSON dict.")
            return []

        if not all(k in parsed for k in ["major", "value"]):
            #print("[WARN] Missing keys in JSON.")
            return []

        major = parsed["major"]
        value = parsed["value"]
        value = value*1.0

        print("good value")
        
        if major == 0:
            array[0] = value / 100
        elif major == 1:
            array[1] = value / 100
        elif major == 2753:
            array[2] = value

    except Exception as e:
        print("[ERROR] Failed to parse line:", e)
        return None

client = InfluxDBClient3(
    token="uuTqUJ_ayx7z_eLScuL4-uh1RIY1d4Wm_VtPRcyrvny1tZp2fL2zQZr4NawIQs_3zeXeZc9MaCIgNFmTTSFZig==",
    host = "https://us-east-1-1.aws.cloud2.influxdata.com",
    org = "e50a638ee5109a61",
    database = "PROJECT",
)

# Open the serial port (adjust the COM port as per your system)
#ser = serial.Serial('/dev/ttyACM0', 115200)
#print("Connected to serial port...")
last = time.time()

while True:
    while(time.time() - last < 5):
        #data = ser.readline().decode('utf-8').strip()
        # Split the data into RSSI, velocity, and distance (assuming CSV format)
        #result = parse_data(data)
        x = 2
    try:
        # Read a line from the serial port (assuming data comes in the format: RSSI,velocity,distance)
        #data = ser.readline().decode('utf-8').strip()
        
        # Split the data into RSSI, velocity, and distance (assuming CSV format)
        #result = parse_data(data)
        
        # Prepare the data point for InfluxDB
        
        point = Point("PROJECT") \
            .tag("node", "project") \
            .field("fan_speed", array[0]) \
            .field("temperature", array[1]) \
            .field("lights", array[2])          
        
        # Write the point to InfluxDB
        client.write(point)
        
        print(f"Wrote data to InfluxDB")
        
        # Wait for the next iteration (send data every 5 seconds)
        #time.sleep(5)
        last = time.time()

    except Exception as e:
        print(f"Error reading from serial port or writing to InfluxDB: {e}")
        break

# Close the serial connection
#ser.close()