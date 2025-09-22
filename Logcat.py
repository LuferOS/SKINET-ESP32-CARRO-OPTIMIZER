from bluepy.btle import Peripheral, UUID, BTLEException, DefaultDelegate # ¡CORREGIDO!
import time

MAC_ESP32 = "CC:50:E3:B3:66:C4"  # ¡Reemplaza con la MAC de tu ESP32!
SERVICE_UUID = UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
CHARACTERISTIC_UUID = UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8")

class MyDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleNotification(self, cHandle, data):
        # Parsear los datos recibidos
        log_str = data.decode('utf-8')
        parts = log_str.split('|')
        log_data = {}
        for part in parts:
            try:
                key, value = part.split(':', 1)
                log_data[key.strip()] = value.strip()
            except ValueError:
                continue

        # Mostrar la información en un formato claro
        print("\033[2J\033[H", end="") # Limpia la pantalla
        print("====================================")
        print("         ✅ ESTADO DEL CARRO ✅")
        print("====================================")
        print(f"Estado de los motores: {log_data.get('Estado_Motor', 'N/A')}")
        print(f"Velocidad (Izq, Der):  {log_data.get('Velocidad', 'N/A')}")
        print("-" * 36)
        print("         🔋 SALUD DE BATERÍA 🌡️")
        print("------------------------------------")
        print(f"Nivel de batería:      {log_data.get('Bateria', 'N/A')}")
        print(f"Voltaje:               {log_data.get('Voltaje', 'N/A')}")
        print(f"Temperatura:           {log_data.get('Temp', 'N/A')}")
        print("-" * 36)
        print("         🌐 ESTABILIDAD DE CONEXIÓN")
        print("------------------------------------")
        print(f"Estabilidad:           {log_data.get('Estabilidad', 'N/A')}")
        print("====================================")
        print(f"Última actualización: {time.strftime('%H:%M:%S', time.localtime())}")

def main():
    print("Buscando ESP32...")
    while True:
        try:
            carro = Peripheral(MAC_ESP32)
            carro.setDelegate(MyDelegate())
            print("¡Conectado! Esperando datos...")

            service = carro.getServiceByUUID(SERVICE_UUID)
            char = service.getCharacteristic(CHARACTERISTIC_UUID)

            # Habilita notificaciones
            # El valHandle se obtiene del descriptor de la característica
            # En muchos casos, es char.valHandle + 1, pero puede variar.
            # Una forma más robusta es buscar el descriptor con UUID 0x2902
            desc = char.getDescriptors(forUUID=0x2902)
            if desc:
                carro.writeCharacteristic(desc[0].handle, b'\x01\x00')
            else:
                print("No se encontró el descriptor de notificaciones. Intentando con valHandle + 1.")
                carro.writeCharacteristic(char.valHandle + 1, b'\x01\x00')

            while True:
                carro.waitForNotifications(1.0)
                # Puedes agregar aquí una lógica para mantener viva la conexión
                # por ejemplo, enviando un ping o simplemente esperando.

        except BTLEException as e:
            print(f"Error de conexión: {e}")
            print("Reconectando en 5 segundos...")
            time.sleep(5)
            continue
        except Exception as e:
            print(f"Ocurrió un error inesperado: {e}")
            print("Reiniciando el ciclo de conexión en 5 segundos...")
            time.sleep(5)
            continue

if __name__ == "__main__":
    main()
