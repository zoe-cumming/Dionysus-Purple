import sensor_pb2

hex_str = "08 E6 14 10 00 18 00"

# Convert to bytes
hex_bytes = bytes.fromhex(hex_str)

# Parse using the generated protobuf class
data = sensor_pb2.SensorData()
data.ParseFromString(hex_bytes)

# Print decoded values
print("Decoded SensorData:")
print(f"  temp  = {data.temp}")
print(f"  light = {data.light}")
print(f"  clap  = {data.clap}")