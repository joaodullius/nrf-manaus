import asyncio
import argparse
from bleak import BleakScanner

# Lista de MACs permitidos (se o filtro estiver ativado)
mac_filter_list = None
output_file = None  # Arquivo de saída (se especificado)

def load_mac_list(filename):
    """Carrega a lista de MACs do arquivo"""
    try:
        with open(filename, "r") as file:
            macs = [line.strip().upper() for line in file if line.strip()]
        print(f"✅ Lista de MACs carregada ({len(macs)} MACs filtrados).")
        return macs
    except FileNotFoundError:
        print(f"❌ Erro: Arquivo '{filename}' não encontrado.")
        return None

def format_hex(data: bytes):
    """ Converte bytes para uma string hexadecimal formatada """
    return " ".join(f"{b:02X}" for b in data)

def format_advertisement_data(advertisement_data):
    """ Formata os dados do advertisement para melhor leitura """
    formatted_data = []
    
    if advertisement_data.local_name:
        formatted_data.append(f"📛 Nome Local: {advertisement_data.local_name}")
    
    if advertisement_data.manufacturer_data:
        formatted_data.append("🏭 Dados do Fabricante:")
        for key, value in advertisement_data.manufacturer_data.items():
            formatted_data.append(f"   🔹 ID: {key} (0x{key:04X})")  # Print company ID in hex
            formatted_data.append(f"   🔸 Dados: {format_hex(value)}")
            
            # Parse sensor data structure
            if len(value) >= 8:  # Minimum size for type (2 bytes) + timestamp (4 bytes) + at least 1 value (2 bytes)
                try:
                    # Extract sensor type (2 bytes)
                    sensor_type = int.from_bytes(value[0:2], byteorder='little')
                    
                    # Extract timestamp (4 bytes)
                    timestamp = int.from_bytes(value[2:6], byteorder='little')
                    
                    # Extract values (remaining bytes, each value is 2 bytes)
                    values = []
                    for i in range(6, len(value), 2):  # Start at byte 6, step by 2 bytes
                        if i + 2 <= len(value):  # Ensure there are enough bytes for a 16-bit value
                            val = int.from_bytes(value[i:i+2], byteorder='little', signed=True)
                            values.append(val)
                    
                    # Convert values to hex format
                    values_hex = [f"0x{abs(v):04X}" if v < 0 else f"0x{v:04X}" for v in values]
                    
                    # Se o argumento -sensor foi passado, exibe os detalhes do sensor
                    if args.sensor:
                        formatted_data.append(f"   🔹 Sensor Type: {sensor_type}")
                        formatted_data.append(f"   🔹 Timestamp: {timestamp} (0x{timestamp:08X})")
                        formatted_data.append(f"   🔹 Values (Hex): {values_hex}")
                        
                        # Parse sensor data based on type and apply unit conversions
                        if sensor_type == 1:  # SENSOR_TYPE_LIGHT
                            if len(values) >= 1:
                                light_value = values[0]
                                formatted_data.append(f"   🔹 Light Intensity: {light_value} lx")
                        
                        elif sensor_type == 2:  # SENSOR_TYPE_TEMP
                            if len(values) >= 1:
                                temperature = values[0] / 100.0
                                formatted_data.append(f"   🔹 Temperature: {temperature:.2f} °C")
                        
                        elif sensor_type == 3:  # SENSOR_TYPE_PRESSURE
                            if len(values) >= 1:
                                pressure = values[0] / 10.0
                                formatted_data.append(f"   🔹 Pressure: {pressure:.1f} hPa")
                        
                        elif sensor_type == 4:  # SENSOR_TYPE_ENVIRONMENTAL
                            if len(values) >= 2:
                                temperature = values[0] / 100.0
                                pressure = values[1] / 10.0
                                formatted_data.append(f"   🔹 Environmental: Temperature: {temperature:.2f} °C | Pressure: {pressure:.1f} hPa")
                        
                        elif sensor_type == 5:  # SENSOR_TYPE_ACCEL
                            if len(values) >= 3:
                                accel_x = values[0] / 1000.0
                                accel_y = values[1] / 1000.0
                                accel_z = values[2] / 1000.0
                                formatted_data.append(f"   🔹 Acceleration: X: {accel_x:.3f} g | Y: {accel_y:.3f} g | Z: {accel_z:.3f} g")
                        
                        elif sensor_type == 6:  # SENSOR_TYPE_GYRO
                            if len(values) >= 3:
                                gyro_x = values[0] / 100.0
                                gyro_y = values[1] / 100.0
                                gyro_z = values[2] / 100.0
                                formatted_data.append(f"   🔹 Gyroscope: X: {gyro_x:.3f} °/s | Y: {gyro_y:.3f} °/s | Z: {gyro_z:.3f} °/s")
                        
                        elif sensor_type == 7:  # SENSOR_TYPE_GNSS
                            if len(values) >= 7:  # Ensure there are enough values for GNSS data
                                fix_type = values[0]
                                latitude = ((values[1] << 16) | (values[2] & 0xFFFF)) / 1e7
                                longitude = ((values[3] << 16) | (values[4] & 0xFFFF)) / 1e7
                                altitude = ((values[5] << 16) | (values[6] & 0xFFFF)) / 1000.0  # Altitude in meters
                                formatted_data.append(f"   🔹 GNSS: Fix Type: {fix_type} | Latitude: {latitude:.7f}° | Longitude: {longitude:.7f}° | Altitude: {altitude:.2f} m")
                        
                        else:
                            formatted_data.append("   🔹 Unknown Sensor Type")
                
                except Exception as e:
                    formatted_data.append(f"   🔹 Error parsing sensor data: {e}")
    
    if advertisement_data.service_uuids:
        formatted_data.append("🔗 UUIDs de Serviço:")
        for uuid in advertisement_data.service_uuids:
            formatted_data.append(f"   🔹 {uuid}")
    
    if advertisement_data.service_data:
        formatted_data.append("📦 Service Data:")
        for key, value in advertisement_data.service_data.items():
            formatted_data.append(f"   🔹 UUID: {key}")
            formatted_data.append(f"   🔸 Dados: {format_hex(value)}")
    
    return "\n".join(formatted_data) if formatted_data else "Nenhum dado disponível"

def callback(device, advertisement_data):
    """ Callback chamado a cada novo pacote BLE detectado """
    global mac_filter_list, output_file

    # Aplica o filtro de MAC se a lista tiver sido carregada
    if mac_filter_list and device.address.upper() not in mac_filter_list:
        return  # Ignora dispositivos que não estão na lista

    output = []
    output.append("=" * 80)
    output.append(f"📡 Dispositivo encontrado: {device.address}")
    output.append(f"🔹 Nome: {device.name if device.name else 'Desconhecido'}")
    output.append(f"📶 RSSI: {advertisement_data.rssi} dBm")
    output.append(f"🔍 Advertisement Data:\n{format_advertisement_data(advertisement_data)}")
    output.append("=" * 80)

    output_text = "\n".join(output)
    print(output_text)

    # Se um arquivo de saída foi especificado, escreve no arquivo
    if output_file:
        with open(output_file, "a", encoding="utf-8") as file:
            file.write(output_text + "\n")

async def scan(scan_time):
    """ Inicia o scanner BLE por um tempo definido ou indefinidamente """
    scanner = BleakScanner(callback)
    print("🔍 Iniciando scanner BLE...")

    await scanner.start()
    
    try:
        if scan_time:  # Se um tempo foi definido, escaneia por esse período
            await asyncio.sleep(scan_time)
        else:  # Loop infinito até Ctrl + C
            while True:
                await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("\n❌ Scanner interrompido pelo usuário.")
    
    await scanner.stop()
    print("✅ Scanner finalizado.")

if __name__ == "__main__":
    # Configuração do parser de argumentos
    parser = argparse.ArgumentParser(
        description="Scanner BLE com opção de filtro por MAC e tempo de varredura",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("-m", type=str, metavar="mac_file.txt",
                        help="Arquivo contendo os MACs a serem filtrados")
    parser.add_argument("-t", type=int, metavar="time",
                        help="Tempo de escaneamento em segundos (se não informado, escaneia indefinidamente)")
    parser.add_argument("-o", type=str, metavar="output.txt",
                        help="Arquivo para salvar a saída do scan")
    parser.add_argument("-sensor", action="store_true",
                        help="Exibe detalhes do sensor (tipo, timestamp, valores, etc.)")
    
    args = parser.parse_args()

    # Carrega a lista de MACs se o parâmetro for fornecido
    if args.m:
        mac_filter_list = load_mac_list(args.m)

    # Define o tempo de escaneamento (None = loop infinito)
    scan_time = args.t if args.t else None

    # Define o arquivo de saída (se fornecido)
    if args.o:
        output_file = args.o
        with open(output_file, "w", encoding="utf-8") as file:
            file.write("📡 Iniciando scanner BLE...\n")

    # Inicia o scanner BLE
    asyncio.run(scan(scan_time))
